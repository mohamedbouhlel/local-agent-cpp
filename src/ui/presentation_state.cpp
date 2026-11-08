#include "ui/presentation_state.hpp"

#include <algorithm>
#include <utility>

namespace agent::ui {
namespace {

EventApplyResult reject(EventApplyStatus status, std::string detail)
{
    return {status, std::move(detail)};
}

bool is_terminal(PresentationPhase phase)
{
    return phase == PresentationPhase::completed || phase == PresentationPhase::failed;
}

bool has_request_or_tool_id(const EventMetadata& metadata)
{
    return metadata.request_id.has_value() || metadata.tool_call_id.has_value();
}

bool valid_http_outcome(HttpRequestOutcome outcome)
{
    switch (outcome) {
    case HttpRequestOutcome::completed:
    case HttpRequestOutcome::failed:
    case HttpRequestOutcome::legacy_callback_aborted:
        return true;
    }
    return false;
}

bool valid_failure_category(RequestFailureCategory category)
{
    switch (category) {
    case RequestFailureCategory::http:
    case RequestFailureCategory::protocol:
    case RequestFailureCategory::iteration_limit:
    case RequestFailureCategory::legacy_callback:
    case RequestFailureCategory::unexpected:
        return true;
    }
    return false;
}

bool valid_turn_outcome(TurnOutcome outcome)
{
    switch (outcome) {
    case TurnOutcome::completed:
    case TurnOutcome::failed:
        return true;
    }
    return false;
}

} // namespace

EventApplyResult PresentationState::apply(const AgentEvent& event)
{
    if (event.metadata.schema_version != 1) {
        return reject(EventApplyStatus::inconsistent, "unsupported event schema version");
    }
    const auto sequence = event.metadata.sequence;
    if (sequence == 0) {
        return reject(EventApplyStatus::inconsistent, "event sequence must be non-zero");
    }
    if (snapshot_.last_sequence != 0) {
        if (sequence == snapshot_.last_sequence) {
            return reject(EventApplyStatus::duplicate, "event sequence was already applied");
        }
        if (sequence < snapshot_.last_sequence) {
            return reject(EventApplyStatus::out_of_order, "event sequence moved backwards");
        }
        if (sequence != snapshot_.last_sequence + 1) {
            return reject(EventApplyStatus::sequence_gap, "event sequence has a gap");
        }
    } else if (sequence != 1) {
        return reject(EventApplyStatus::sequence_gap, "first event sequence must be 1");
    }

    if (snapshot_.session_id && *snapshot_.session_id != event.metadata.session_id) {
        return reject(EventApplyStatus::inconsistent, "event belongs to a different session");
    }
    if (!event.metadata.turn_id) {
        return reject(EventApplyStatus::inconsistent, "event is missing its turn id");
    }
    if (event.metadata.session_id.value == 0 || event.metadata.turn_id->value == 0) {
        return reject(EventApplyStatus::inconsistent, "session and turn ids must be non-zero");
    }

    const auto turn_id = *event.metadata.turn_id;
    const auto type = event.type();
    const auto no_tool_id = !event.metadata.tool_call_id;
    const auto matching_last_request = event.metadata.request_id && snapshot_.last_request_id
        && *event.metadata.request_id == *snapshot_.last_request_id;

    switch (type) {
    case EventType::RequestStarted: {
        if (!std::holds_alternative<RequestStarted>(event.payload) || has_request_or_tool_id(event.metadata)) {
            return reject(EventApplyStatus::inconsistent, "RequestStarted has unexpected payload metadata");
        }
        if (snapshot_.phase != PresentationPhase::idle && !is_terminal(snapshot_.phase)) {
            return reject(EventApplyStatus::inconsistent, "a turn is already active");
        }
        if (snapshot_.turn_id && turn_id.value <= snapshot_.turn_id->value) {
            return reject(EventApplyStatus::inconsistent, "turn id did not increase");
        }
        snapshot_.session_id = event.metadata.session_id;
        snapshot_.turn_id = turn_id;
        snapshot_.phase = PresentationPhase::submitted;
        snapshot_.active_request_id.reset();
        snapshot_.last_request_id.reset();
        snapshot_.active_tool_call_id.reset();
        snapshot_.active_tool_name.clear();
        snapshot_.pending_authorization_tool_call_id.reset();
        snapshot_.pending_authorization_tool_name.clear();
        snapshot_.approved_authorization_tool_call_id.reset();
        snapshot_.approved_authorization_tool_name.clear();
        snapshot_.http_requests = 0;
        snapshot_.tools_started = 0;
        snapshot_.tools_succeeded = 0;
        snapshot_.tools_failed = 0;
        snapshot_.text_delta_count = 0;
        snapshot_.request_failed = false;
        snapshot_.last_http_outcome.reset();
        snapshot_.turn_outcome.reset();
        active_tool_.reset();
        tool_ids_in_turn_.clear();
        break;
    }
    case EventType::HttpRequestStarted: {
        const auto* payload = std::get_if<HttpRequestStarted>(&event.payload);
        if (payload == nullptr || !event.metadata.request_id || event.metadata.request_id->value == 0 || !no_tool_id) {
            return reject(EventApplyStatus::inconsistent, "HttpRequestStarted is missing or has unexpected correlation");
        }
        if (turn_id != snapshot_.turn_id.value_or(TurnId{})
            || (snapshot_.phase != PresentationPhase::submitted && snapshot_.phase != PresentationPhase::waiting)
            || snapshot_.active_request_id || active_tool_) {
            return reject(EventApplyStatus::inconsistent, "HTTP request cannot start in the current state");
        }
        if (payload->iteration != snapshot_.http_requests + 1) {
            return reject(EventApplyStatus::inconsistent, "HTTP iteration number is not consecutive");
        }
        if (last_request_id_in_session_ && event.metadata.request_id->value <= last_request_id_in_session_->value) {
            return reject(EventApplyStatus::inconsistent, "request id was reused or moved backwards");
        }
        snapshot_.active_request_id = event.metadata.request_id;
        snapshot_.last_request_id = event.metadata.request_id;
        snapshot_.last_http_outcome.reset();
        last_request_id_in_session_ = event.metadata.request_id;
        ++snapshot_.http_requests;
        snapshot_.phase = PresentationPhase::waiting;
        break;
    }
    case EventType::TextDelta: {
        const auto* payload = std::get_if<TextDelta>(&event.payload);
        if (payload == nullptr || payload->text.empty() || !event.metadata.request_id || !no_tool_id
            || !snapshot_.active_request_id || *event.metadata.request_id != *snapshot_.active_request_id
            || turn_id != snapshot_.turn_id.value_or(TurnId{})) {
            return reject(EventApplyStatus::inconsistent, "TextDelta does not match an active HTTP request");
        }
        if (snapshot_.phase != PresentationPhase::waiting && snapshot_.phase != PresentationPhase::streaming) {
            return reject(EventApplyStatus::inconsistent, "TextDelta arrived in the current invalid state");
        }
        ++snapshot_.text_delta_count;
        snapshot_.phase = PresentationPhase::streaming;
        break;
    }
    case EventType::HttpRequestFinished: {
        const auto* payload = std::get_if<HttpRequestFinished>(&event.payload);
        if (payload == nullptr || !event.metadata.request_id || !no_tool_id || !snapshot_.active_request_id
            || *event.metadata.request_id != *snapshot_.active_request_id
            || turn_id != snapshot_.turn_id.value_or(TurnId{})) {
            return reject(EventApplyStatus::inconsistent, "HttpRequestFinished does not match the active request");
        }
        if (!valid_http_outcome(payload->outcome)
            || payload->duration < std::chrono::steady_clock::duration::zero()) {
            return reject(EventApplyStatus::inconsistent, "HTTP outcome or duration is invalid");
        }
        snapshot_.active_request_id.reset();
        snapshot_.last_http_outcome = payload->outcome;
        snapshot_.phase = PresentationPhase::waiting;
        break;
    }
    case EventType::ToolAuthorizationRequested: {
        const auto* payload = std::get_if<ToolAuthorizationRequested>(&event.payload);
        if (payload == nullptr || payload->name != "write_file" || !event.metadata.tool_call_id
            || event.metadata.tool_call_id->value.empty() || !event.metadata.request_id || !matching_last_request
            || turn_id != snapshot_.turn_id.value_or(TurnId{}) || snapshot_.phase != PresentationPhase::waiting
            || snapshot_.active_request_id || active_tool_ || snapshot_.pending_authorization_tool_call_id
            || snapshot_.approved_authorization_tool_call_id
            || snapshot_.last_http_outcome != HttpRequestOutcome::completed) {
            return reject(EventApplyStatus::inconsistent, "write authorization request has invalid state or correlation");
        }
        if (std::find(tool_ids_in_turn_.begin(), tool_ids_in_turn_.end(), *event.metadata.tool_call_id)
            != tool_ids_in_turn_.end()) {
            return reject(EventApplyStatus::inconsistent, "tool call id was already used in this turn");
        }
        tool_ids_in_turn_.push_back(*event.metadata.tool_call_id);
        snapshot_.pending_authorization_tool_call_id = event.metadata.tool_call_id;
        snapshot_.pending_authorization_tool_name = payload->name;
        break;
    }
    case EventType::ToolAuthorizationResolved: {
        const auto* payload = std::get_if<ToolAuthorizationResolved>(&event.payload);
        if (payload == nullptr || !event.metadata.tool_call_id || !event.metadata.request_id || !matching_last_request
            || turn_id != snapshot_.turn_id.value_or(TurnId{}) || snapshot_.phase != PresentationPhase::waiting
            || snapshot_.active_request_id || active_tool_ || !snapshot_.pending_authorization_tool_call_id
            || *snapshot_.pending_authorization_tool_call_id != *event.metadata.tool_call_id) {
            return reject(EventApplyStatus::inconsistent, "write authorization result does not match a pending request");
        }
        snapshot_.pending_authorization_tool_call_id.reset();
        snapshot_.pending_authorization_tool_name.clear();
        if (payload->status == ToolAuthorizationStatus::approved) {
            snapshot_.approved_authorization_tool_call_id = event.metadata.tool_call_id;
            snapshot_.approved_authorization_tool_name = "write_file";
        } else if (payload->status != ToolAuthorizationStatus::denied
                   && payload->status != ToolAuthorizationStatus::cancelled
                   && payload->status != ToolAuthorizationStatus::unavailable
                   && payload->status != ToolAuthorizationStatus::expired) {
            return reject(EventApplyStatus::inconsistent, "invalid write authorization status");
        }
        break;
    }
    case EventType::ToolStarted: {
        const auto* payload = std::get_if<ToolStarted>(&event.payload);
        if (payload == nullptr || payload->name.empty() || !event.metadata.tool_call_id
            || event.metadata.tool_call_id->value.empty() || !event.metadata.request_id
            || !matching_last_request) {
            return reject(EventApplyStatus::inconsistent, "ToolStarted is missing valid tool/request correlation");
        }
        const bool authorized = snapshot_.approved_authorization_tool_call_id
            && *snapshot_.approved_authorization_tool_call_id == *event.metadata.tool_call_id
            && snapshot_.approved_authorization_tool_name == payload->name;
        if (turn_id != snapshot_.turn_id.value_or(TurnId{}) || snapshot_.phase != PresentationPhase::waiting
            || snapshot_.active_request_id || active_tool_
            || snapshot_.pending_authorization_tool_call_id
            || snapshot_.last_http_outcome != HttpRequestOutcome::completed) {
            return reject(EventApplyStatus::inconsistent, "tool cannot start before a completed response");
        }
        if (authorized) {
            snapshot_.approved_authorization_tool_call_id.reset();
            snapshot_.approved_authorization_tool_name.clear();
        } else {
            if (std::find(tool_ids_in_turn_.begin(), tool_ids_in_turn_.end(), *event.metadata.tool_call_id)
                != tool_ids_in_turn_.end()) {
                return reject(EventApplyStatus::inconsistent, "tool call id was already used in this turn");
            }
            tool_ids_in_turn_.push_back(*event.metadata.tool_call_id);
        }
        active_tool_ = ActiveTool{*event.metadata.tool_call_id, *event.metadata.request_id, payload->name};
        snapshot_.active_tool_call_id = event.metadata.tool_call_id;
        snapshot_.active_tool_name = payload->name;
        ++snapshot_.tools_started;
        snapshot_.phase = PresentationPhase::tool_running;
        break;
    }
    case EventType::ToolSucceeded:
    case EventType::ToolFailed: {
        const auto* succeeded = std::get_if<ToolSucceeded>(&event.payload);
        const auto* failed = std::get_if<ToolFailed>(&event.payload);
        const auto* name = succeeded != nullptr ? &succeeded->name : (failed != nullptr ? &failed->name : nullptr);
        const auto duration = succeeded != nullptr ? succeeded->duration
                                                   : (failed != nullptr ? failed->duration
                                                                        : std::chrono::steady_clock::duration::zero());
        if (name == nullptr || name->empty() || duration < std::chrono::steady_clock::duration::zero()
            || !event.metadata.tool_call_id || !event.metadata.request_id || !active_tool_) {
            return reject(EventApplyStatus::inconsistent, "tool result is missing active tool correlation");
        }
        if (turn_id != snapshot_.turn_id.value_or(TurnId{}) || snapshot_.phase != PresentationPhase::tool_running
            || *event.metadata.tool_call_id != active_tool_->id || *event.metadata.request_id != active_tool_->request_id
            || *name != active_tool_->name) {
            return reject(EventApplyStatus::inconsistent, "tool result does not match the active tool call");
        }
        if (succeeded != nullptr) {
            ++snapshot_.tools_succeeded;
        } else {
            ++snapshot_.tools_failed;
        }
        snapshot_.active_tool_call_id.reset();
        snapshot_.active_tool_name.clear();
        active_tool_.reset();
        snapshot_.phase = PresentationPhase::waiting;
        break;
    }
    case EventType::RequestFailed: {
        const auto* payload = std::get_if<RequestFailed>(&event.payload);
        if (payload == nullptr || !valid_failure_category(payload->category) || event.metadata.tool_call_id) {
            return reject(EventApplyStatus::inconsistent, "RequestFailed has invalid correlation metadata");
        }
        if (turn_id != snapshot_.turn_id.value_or(TurnId{}) || is_terminal(snapshot_.phase)
            || snapshot_.active_request_id || active_tool_ || snapshot_.pending_authorization_tool_call_id
            || snapshot_.approved_authorization_tool_call_id || snapshot_.request_failed) {
            return reject(EventApplyStatus::inconsistent, "RequestFailed arrived in the current invalid state");
        }
        if (event.metadata.request_id != snapshot_.last_request_id) {
            return reject(EventApplyStatus::inconsistent, "RequestFailed does not match the current turn request");
        }
        snapshot_.request_failed = true;
        snapshot_.phase = PresentationPhase::waiting;
        break;
    }
    case EventType::TurnFinished: {
        const auto* payload = std::get_if<TurnFinished>(&event.payload);
        if (payload == nullptr || !valid_turn_outcome(payload->outcome) || has_request_or_tool_id(event.metadata)) {
            return reject(EventApplyStatus::inconsistent, "TurnFinished has unexpected payload metadata");
        }
        if (turn_id != snapshot_.turn_id.value_or(TurnId{}) || is_terminal(snapshot_.phase)
            || snapshot_.active_request_id || active_tool_ || snapshot_.pending_authorization_tool_call_id
            || snapshot_.approved_authorization_tool_call_id) {
            return reject(EventApplyStatus::inconsistent, "turn cannot finish in the current state");
        }
        if (payload->http_requests != snapshot_.http_requests || payload->tools_succeeded != snapshot_.tools_succeeded
            || payload->tools_failed != snapshot_.tools_failed) {
            return reject(EventApplyStatus::inconsistent, "TurnFinished counters do not match observed events");
        }
        if (snapshot_.tools_succeeded > snapshot_.tools_started
            || snapshot_.tools_failed != snapshot_.tools_started - snapshot_.tools_succeeded) {
            return reject(EventApplyStatus::inconsistent, "a started tool has no matching terminal event");
        }
        if (payload->outcome == TurnOutcome::completed) {
            if (snapshot_.request_failed || snapshot_.http_requests == 0
                || snapshot_.last_http_outcome != HttpRequestOutcome::completed) {
                return reject(EventApplyStatus::inconsistent, "successful turn lacks a completed final request");
            }
            snapshot_.phase = PresentationPhase::completed;
        } else {
            if (!snapshot_.request_failed) {
                return reject(EventApplyStatus::inconsistent, "failed turn lacks a RequestFailed event");
            }
            snapshot_.phase = PresentationPhase::failed;
        }
        snapshot_.turn_outcome = payload->outcome;
        break;
    }
    }

    snapshot_.last_sequence = sequence;
    return {EventApplyStatus::applied, {}};
}

void PresentationState::reset() noexcept
{
    snapshot_ = {};
    active_tool_.reset();
    tool_ids_in_turn_.clear();
    last_request_id_in_session_.reset();
}

} // namespace agent::ui
