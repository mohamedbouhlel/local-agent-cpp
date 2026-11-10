#include "ui/metrics.hpp"

#include <algorithm>
#include <limits>

namespace agent::ui {
namespace {

std::optional<std::chrono::steady_clock::duration> nonnegative_difference(
    std::chrono::steady_clock::duration end,
    std::chrono::steady_clock::duration start)
{
    if (end < start) return std::nullopt;
    return end - start;
}

template <typename Metric>
Metric* find_by_request(std::vector<Metric>& metrics, RequestId id)
{
    const auto found = std::find_if(metrics.begin(), metrics.end(), [id](const Metric& metric) {
        return metric.request_id == id;
    });
    return found == metrics.end() ? nullptr : &*found;
}

ToolMetric* find_tool(std::vector<ToolMetric>& metrics, RequestId request_id, const ToolCallId& tool_call_id)
{
    const auto found = std::find_if(metrics.begin(), metrics.end(), [&](const ToolMetric& metric) {
        return metric.request_id == request_id && metric.tool_call_id == tool_call_id;
    });
    return found == metrics.end() ? nullptr : &*found;
}

} // namespace

MetricCollector::MetricCollector()
{
    metrics_.http_requests.reserve(max_http_metric_details);
    metrics_.tools.reserve(max_tool_metric_details);
}

void MetricCollector::begin_pending_turn() noexcept
{
    reset_for_turn(std::nullopt, 0);
}

void MetricCollector::reset_for_turn(std::optional<TurnId> turn_id, std::uint64_t sequence) noexcept
{
    metrics_.turn_id = turn_id;
    metrics_.duration.reset();
    metrics_.first_delta_latency.reset();
    metrics_.outcome.reset();
    metrics_.http_requests_started = 0;
    metrics_.http_requests_finished = 0;
    metrics_.tools_started = 0;
    metrics_.tools_succeeded = 0;
    metrics_.tools_failed = 0;
    metrics_.text_fragments = 0;
    metrics_.response_body_bytes = turn_id ? std::optional<std::uint64_t>{0} : std::nullopt;
    metrics_.event_details_complete = true;
    metrics_.detail_history_truncated = false;
    metrics_.counter_overflowed = false;
    metrics_.evicted_http_details = 0;
    metrics_.evicted_tool_details = 0;
    metrics_.rejected_tool_details = 0;
    metrics_.retained_tool_id_bytes = 0;
    metrics_.http_requests.clear();
    metrics_.tools.clear();
    turn_started_at_.reset();
    first_delta_at_.reset();
    last_sequence_ = sequence == 0 ? std::nullopt : std::optional<std::uint64_t>{sequence};
    pending_http_requests_ = 0;
    pending_http_request_ids_.fill(std::nullopt);
    pending_http_request_count_ = 0;
    received_body_bytes_total_ = turn_id ? std::optional<std::uint64_t>{0} : std::nullopt;
}

void MetricCollector::increment(std::uint64_t& counter) noexcept
{
    if (counter == std::numeric_limits<std::uint64_t>::max()) {
        metrics_.counter_overflowed = true;
        return;
    }
    ++counter;
}

void MetricCollector::mark_event_details_incomplete() noexcept
{
    metrics_.event_details_complete = false;
    metrics_.response_body_bytes.reset();
}

void MetricCollector::evict_oldest_http_detail() noexcept
{
    if (metrics_.http_requests.empty()) return;
    metrics_.http_requests.erase(metrics_.http_requests.begin());
    increment(metrics_.evicted_http_details);
    metrics_.detail_history_truncated = true;
}

void MetricCollector::evict_oldest_tool_detail() noexcept
{
    if (metrics_.tools.empty()) return;
    metrics_.retained_tool_id_bytes -= metrics_.tools.front().tool_call_id.value.size();
    metrics_.tools.erase(metrics_.tools.begin());
    increment(metrics_.evicted_tool_details);
    metrics_.detail_history_truncated = true;
}

void MetricCollector::retain_http_detail(RequestId request_id)
{
    if (metrics_.http_requests.size() == max_http_metric_details) evict_oldest_http_detail();
    metrics_.http_requests.push_back(HttpRequestMetric{request_id});
}

void MetricCollector::retain_tool_detail(RequestId request_id, const ToolCallId& tool_call_id, std::string_view name)
{
    const auto bytes = tool_call_id.value.size();
    if (bytes > max_tool_id_metric_bytes || name.size() > max_tool_name_metric_bytes) {
        increment(metrics_.rejected_tool_details);
        metrics_.detail_history_truncated = true;
        return;
    }
    while (!metrics_.tools.empty()
           && (metrics_.tools.size() == max_tool_metric_details
               || bytes > max_tool_id_metric_bytes - metrics_.retained_tool_id_bytes)) {
        evict_oldest_tool_detail();
    }
    metrics_.retained_tool_id_bytes += bytes;
    metrics_.tools.push_back(ToolMetric{request_id, tool_call_id, std::string(name)});
}

void MetricCollector::consume(const AgentEvent& event)
{
    if (const auto* started = std::get_if<RequestStarted>(&event.payload)) {
        (void)started;
        if (metrics_.turn_id && event.metadata.turn_id == metrics_.turn_id) {
            mark_event_details_incomplete(); // A duplicate start must not erase the existing turn.
            return;
        }
        reset_for_turn(event.metadata.turn_id, event.metadata.sequence);
        if (event.metadata.sequence == 0 || !event.metadata.turn_id) {
            mark_event_details_incomplete();
            return;
        }
        turn_started_at_ = event.metadata.elapsed >= std::chrono::steady_clock::duration::zero()
            ? std::optional{event.metadata.elapsed}
            : std::nullopt;
        return;
    }

    if (!metrics_.turn_id || event.metadata.turn_id != metrics_.turn_id) {
        mark_event_details_incomplete();
        return;
    }
    if (!last_sequence_ || event.metadata.sequence == 0 || event.metadata.sequence <= *last_sequence_) {
        mark_event_details_incomplete();
        return;
    }
    if (*last_sequence_ != std::numeric_limits<std::uint64_t>::max()
        && event.metadata.sequence != *last_sequence_ + 1) {
        mark_event_details_incomplete();
    }
    last_sequence_ = event.metadata.sequence;

    switch (event.type()) {
    case EventType::RequestStarted:
        break;
    case EventType::HttpRequestStarted:
        if (!event.metadata.request_id) {
            mark_event_details_incomplete();
            break;
        }
        if (pending_http_request_count_ == pending_http_request_ids_.size()
            || std::find(pending_http_request_ids_.begin(),
                         pending_http_request_ids_.begin() + static_cast<std::ptrdiff_t>(pending_http_request_count_),
                         event.metadata.request_id)
                   != pending_http_request_ids_.begin() + static_cast<std::ptrdiff_t>(pending_http_request_count_)) {
            mark_event_details_incomplete();
            break;
        }
        increment(metrics_.http_requests_started);
        increment(pending_http_requests_);
        pending_http_request_ids_[pending_http_request_count_++] = event.metadata.request_id;
        metrics_.response_body_bytes.reset();
        retain_http_detail(*event.metadata.request_id);
        break;
    case EventType::TextDelta:
        increment(metrics_.text_fragments);
        if (!first_delta_at_) {
            first_delta_at_ = event.metadata.elapsed;
            if (turn_started_at_) {
                metrics_.first_delta_latency = nonnegative_difference(*first_delta_at_, *turn_started_at_);
            }
        }
        break;
    case EventType::ToolStarted:
        if (!event.metadata.request_id || !event.metadata.tool_call_id) {
            mark_event_details_incomplete();
            break;
        }
        increment(metrics_.tools_started);
        retain_tool_detail(*event.metadata.request_id, *event.metadata.tool_call_id,
                           std::get<ToolStarted>(event.payload).name);
        break;
    case EventType::ToolSucceeded: {
        if (!event.metadata.request_id || !event.metadata.tool_call_id) {
            mark_event_details_incomplete();
            break;
        }
        increment(metrics_.tools_succeeded);
        auto* metric = find_tool(metrics_.tools, *event.metadata.request_id, *event.metadata.tool_call_id);
        if (!metric) {
            metrics_.detail_history_truncated = true;
            break;
        }
        const auto duration = std::get<ToolSucceeded>(event.payload).duration;
        if (duration >= std::chrono::steady_clock::duration::zero()) metric->duration = duration;
        metric->outcome = ToolMetricOutcome::succeeded;
        break;
    }
    case EventType::ToolFailed: {
        if (!event.metadata.request_id || !event.metadata.tool_call_id) {
            mark_event_details_incomplete();
            break;
        }
        increment(metrics_.tools_failed);
        auto* metric = find_tool(metrics_.tools, *event.metadata.request_id, *event.metadata.tool_call_id);
        if (!metric) {
            metrics_.detail_history_truncated = true;
            break;
        }
        const auto duration = std::get<ToolFailed>(event.payload).duration;
        if (duration >= std::chrono::steady_clock::duration::zero()) metric->duration = duration;
        metric->outcome = ToolMetricOutcome::failed;
        break;
    }
    case EventType::HttpRequestFinished: {
        if (!event.metadata.request_id) {
            mark_event_details_incomplete();
            break;
        }
        increment(metrics_.http_requests_finished);
        auto pending_end = pending_http_request_ids_.begin()
            + static_cast<std::ptrdiff_t>(pending_http_request_count_);
        const auto pending = std::find(pending_http_request_ids_.begin(), pending_end, event.metadata.request_id);
        const bool matched_request = pending != pending_end;
        if (!matched_request) {
            mark_event_details_incomplete();
        } else {
            std::move(pending + 1, pending_end, pending);
            pending_http_request_ids_[--pending_http_request_count_].reset();
            --pending_http_requests_;
        }
        const auto& payload = std::get<HttpRequestFinished>(event.payload);
        if (auto* metric = find_by_request(metrics_.http_requests, *event.metadata.request_id)) {
            if (payload.duration >= std::chrono::steady_clock::duration::zero()) {
                metric->duration = payload.duration;
            }
            metric->response_body_bytes = payload.response_bytes;
            metric->outcome = payload.outcome;
        } else {
            metrics_.detail_history_truncated = true;
        }
        if (matched_request && received_body_bytes_total_) {
            if (payload.response_bytes > std::numeric_limits<std::uint64_t>::max() - *received_body_bytes_total_) {
                received_body_bytes_total_.reset();
                metrics_.counter_overflowed = true;
            } else {
                *received_body_bytes_total_ += payload.response_bytes;
            }
        }
        if (pending_http_requests_ == 0 && metrics_.event_details_complete) {
            metrics_.response_body_bytes = received_body_bytes_total_;
        }
        break;
    }
    case EventType::RequestFailed:
        break;
    case EventType::ToolAuthorizationRequested:
    case EventType::ToolAuthorizationResolved:
        // Authorization outcomes are not tool execution outcomes.
        break;
    case EventType::TurnFinished: {
        const auto& payload = std::get<TurnFinished>(event.payload);
        metrics_.outcome = payload.outcome;
        if (turn_started_at_) {
            metrics_.duration = nonnegative_difference(event.metadata.elapsed, *turn_started_at_);
        }
        if (pending_http_requests_ != 0 || !metrics_.event_details_complete) metrics_.response_body_bytes.reset();
        else metrics_.response_body_bytes = received_body_bytes_total_;
        break;
    }
    }
}

} // namespace agent::ui
