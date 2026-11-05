#include "test_support.hpp"
#include "ui/presentation_state.hpp"
#include "ui/terminal_capabilities.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace {

using namespace agent::ui;

class EventTrace {
public:
    AgentEvent make(EventPayload payload,
                    std::optional<RequestId> request_id = std::nullopt,
                    std::optional<ToolCallId> tool_call_id = std::nullopt,
                    std::optional<TurnId> turn_id = std::nullopt,
                    std::optional<SessionId> session_id = std::nullopt)
    {
        EventMetadata metadata;
        metadata.session_id = session_id.value_or(SessionId{41});
        metadata.turn_id = turn_id.value_or(TurnId{1});
        metadata.request_id = request_id;
        metadata.tool_call_id = tool_call_id;
        metadata.sequence = ++sequence_;
        metadata.timestamp_utc = std::chrono::system_clock::time_point{} + std::chrono::seconds(sequence_);
        metadata.elapsed = std::chrono::seconds(sequence_);
        return {std::move(metadata), std::move(payload)};
    }

private:
    std::uint64_t sequence_ = 0;
};

void check_applied(PresentationState& state, const AgentEvent& event)
{
    CHECK(state.apply(event).status == EventApplyStatus::applied);
}

TerminalCapabilities inspect(TerminalProbe probe)
{
    return terminal_capabilities_from_probe(probe);
}

} // namespace

TEST(ui, presentation_state_starts_idle_and_accepts_request_start)
{
    PresentationState state;
    EventTrace trace;

    CHECK(state.snapshot().phase == PresentationPhase::idle);
    CHECK(!state.snapshot().session_id);
    const auto result = state.apply(trace.make(RequestStarted{}));
    CHECK(result.status == EventApplyStatus::applied);
    CHECK(state.snapshot().phase == PresentationPhase::submitted);
    CHECK(state.snapshot().turn_id == TurnId{1});
    CHECK_EQ(state.snapshot().last_sequence, std::uint64_t(1));
}

TEST(ui, presentation_stream_tracks_deltas_without_retaining_text)
{
    PresentationState state;
    EventTrace trace;
    check_applied(state, trace.make(RequestStarted{}));
    check_applied(state, trace.make(HttpRequestStarted{1}, RequestId{90}));
    const auto delta = trace.make(TextDelta{"fragment secret"}, RequestId{90});
    check_applied(state, delta);

    CHECK(state.snapshot().phase == PresentationPhase::streaming);
    CHECK_EQ(state.snapshot().text_delta_count, std::uint64_t(1));
    CHECK_EQ(state.snapshot().active_request_id->value, std::uint64_t(90));
}

TEST(ui, presentation_turn_completes_only_after_transport_and_turn_events)
{
    PresentationState state;
    EventTrace trace;
    check_applied(state, trace.make(RequestStarted{}));
    check_applied(state, trace.make(HttpRequestStarted{1}, RequestId{90}));
    check_applied(state, trace.make(TextDelta{"partial"}, RequestId{90}));

    CHECK(state.snapshot().phase == PresentationPhase::streaming);
    CHECK(!state.snapshot().turn_outcome);
    check_applied(state,
                  trace.make(HttpRequestFinished{std::chrono::milliseconds(12), 32, HttpRequestOutcome::completed},
                             RequestId{90}));
    CHECK(state.snapshot().phase == PresentationPhase::waiting);
    CHECK(!state.snapshot().turn_outcome);
    check_applied(state, trace.make(TurnFinished{TurnOutcome::completed, 1, 0, 0}));
    CHECK(state.snapshot().phase == PresentationPhase::completed);
    CHECK(state.snapshot().turn_outcome == TurnOutcome::completed);
}

TEST(ui, multiple_http_requests_keep_iteration_and_request_correlation)
{
    PresentationState state;
    EventTrace trace;
    check_applied(state, trace.make(RequestStarted{}));
    check_applied(state, trace.make(HttpRequestStarted{1}, RequestId{90}));
    check_applied(state,
                  trace.make(HttpRequestFinished{std::chrono::milliseconds(2), 8, HttpRequestOutcome::completed},
                             RequestId{90}));
    check_applied(state, trace.make(HttpRequestStarted{2}, RequestId{91}));
    check_applied(state,
                  trace.make(HttpRequestFinished{std::chrono::milliseconds(3), 11, HttpRequestOutcome::completed},
                             RequestId{91}));
    check_applied(state, trace.make(TurnFinished{TurnOutcome::completed, 2, 0, 0}));

    CHECK_EQ(state.snapshot().http_requests, std::uint64_t(2));
    CHECK(state.snapshot().last_request_id == RequestId{91});
    CHECK(state.snapshot().phase == PresentationPhase::completed);
}

TEST(ui, consecutive_turns_share_session_but_reset_per_turn_state)
{
    PresentationState state;
    EventTrace trace;
    check_applied(state, trace.make(RequestStarted{}));
    check_applied(state, trace.make(HttpRequestStarted{1}, RequestId{90}));
    check_applied(state,
                  trace.make(HttpRequestFinished{std::chrono::milliseconds(2), 8, HttpRequestOutcome::completed},
                             RequestId{90}));
    check_applied(state, trace.make(TurnFinished{TurnOutcome::completed, 1, 0, 0}));

    check_applied(state, trace.make(RequestStarted{}, std::nullopt, std::nullopt, TurnId{2}));
    CHECK_EQ(state.snapshot().http_requests, std::uint64_t(0));
    CHECK(!state.snapshot().last_request_id);
    check_applied(state, trace.make(HttpRequestStarted{1}, RequestId{91}, std::nullopt, TurnId{2}));
    check_applied(state,
                  trace.make(HttpRequestFinished{std::chrono::milliseconds(3), 10, HttpRequestOutcome::completed},
                             RequestId{91},
                             std::nullopt,
                             TurnId{2}));
    check_applied(state, trace.make(TurnFinished{TurnOutcome::completed, 1, 0, 0}, std::nullopt, std::nullopt, TurnId{2}));

    CHECK(state.snapshot().phase == PresentationPhase::completed);
    CHECK(state.snapshot().turn_id == TurnId{2});
    CHECK(state.snapshot().session_id == SessionId{41});
    CHECK_EQ(state.snapshot().http_requests, std::uint64_t(1));
}

TEST(ui, several_tools_and_failed_tool_can_be_followed_by_successful_turn)
{
    PresentationState state;
    EventTrace trace;
    const RequestId first_request{90};
    const ToolCallId failed_tool{"call-failed"};
    const ToolCallId succeeded_tool{"call-ok"};
    check_applied(state, trace.make(RequestStarted{}));
    check_applied(state, trace.make(HttpRequestStarted{1}, first_request));
    check_applied(state,
                  trace.make(HttpRequestFinished{std::chrono::milliseconds(2), 20, HttpRequestOutcome::completed},
                             first_request));
    check_applied(state, trace.make(ToolStarted{"alpha"}, first_request, failed_tool));
    check_applied(state,
                  trace.make(ToolFailed{"alpha", std::chrono::milliseconds(4)}, first_request, failed_tool));
    CHECK(state.snapshot().phase == PresentationPhase::waiting);
    CHECK_EQ(state.snapshot().tools_failed, std::uint64_t(1));
    check_applied(state, trace.make(ToolStarted{"beta"}, first_request, succeeded_tool));
    check_applied(state,
                  trace.make(ToolSucceeded{"beta", std::chrono::milliseconds(1)}, first_request, succeeded_tool));
    check_applied(state, trace.make(HttpRequestStarted{2}, RequestId{91}));
    check_applied(state, trace.make(TextDelta{"final"}, RequestId{91}));
    check_applied(state,
                  trace.make(HttpRequestFinished{std::chrono::milliseconds(5), 40, HttpRequestOutcome::completed},
                             RequestId{91}));
    check_applied(state, trace.make(TurnFinished{TurnOutcome::completed, 2, 1, 1}));

    CHECK(state.snapshot().phase == PresentationPhase::completed);
    CHECK_EQ(state.snapshot().tools_started, std::uint64_t(2));
    CHECK_EQ(state.snapshot().tools_succeeded, std::uint64_t(1));
    CHECK_EQ(state.snapshot().tools_failed, std::uint64_t(1));
}

TEST(ui, failed_turn_requires_failure_event_and_stays_failed)
{
    PresentationState state;
    EventTrace trace;
    check_applied(state, trace.make(RequestStarted{}));
    check_applied(state, trace.make(HttpRequestStarted{1}, RequestId{90}));
    check_applied(state,
                  trace.make(HttpRequestFinished{std::chrono::milliseconds(1), 0, HttpRequestOutcome::failed},
                             RequestId{90}));
    check_applied(state, trace.make(RequestFailed{RequestFailureCategory::http}, RequestId{90}));
    check_applied(state, trace.make(TurnFinished{TurnOutcome::failed, 1, 0, 0}));
    CHECK(state.snapshot().phase == PresentationPhase::failed);
    CHECK(state.snapshot().request_failed);

    const auto after_terminal = state.snapshot();
    const auto extra = trace.make(TextDelta{"late"}, RequestId{90});
    CHECK(state.apply(extra).status == EventApplyStatus::inconsistent);
    CHECK_EQ(state.snapshot().last_sequence, after_terminal.last_sequence);
    CHECK(state.snapshot().phase == PresentationPhase::failed);
}

TEST(ui, duplicate_out_of_order_and_sequence_gap_policies_are_explicit)
{
    PresentationState state;
    EventTrace trace;
    const auto first = trace.make(RequestStarted{});
    check_applied(state, first);
    CHECK(state.apply(first).status == EventApplyStatus::duplicate);
    const auto second = trace.make(HttpRequestStarted{1}, RequestId{90});
    check_applied(state, second);
    CHECK(state.apply(first).status == EventApplyStatus::out_of_order);
    CHECK_EQ(state.snapshot().last_sequence, std::uint64_t(2));

    PresentationState gap_state;
    EventTrace gap_trace;
    auto gap = gap_trace.make(RequestStarted{});
    gap.metadata.sequence = 2;
    CHECK(gap_state.apply(gap).status == EventApplyStatus::sequence_gap);
    CHECK_EQ(gap_state.snapshot().last_sequence, std::uint64_t(0));

    PresentationState schema_state;
    EventTrace schema_trace;
    auto unsupported = schema_trace.make(RequestStarted{});
    unsupported.metadata.schema_version = 2;
    CHECK(schema_state.apply(unsupported).status == EventApplyStatus::inconsistent);
    auto supported = schema_trace.make(RequestStarted{});
    supported.metadata.sequence = 1;
    check_applied(schema_state, supported);
}

TEST(ui, incoherent_transition_is_rejected_without_consuming_its_sequence)
{
    PresentationState state;
    EventTrace trace;
    check_applied(state, trace.make(RequestStarted{}));
    const auto invalid = trace.make(HttpRequestFinished{std::chrono::milliseconds(1), 4,
                                                        HttpRequestOutcome::completed},
                                    RequestId{90});
    CHECK(state.apply(invalid).status == EventApplyStatus::inconsistent);
    CHECK_EQ(state.snapshot().last_sequence, std::uint64_t(1));

    auto valid = trace.make(HttpRequestStarted{1}, RequestId{90});
    valid.metadata.sequence = 2;
    check_applied(state, valid);
}

TEST(ui, invalid_enum_payload_is_rejected_without_advancing_sequence)
{
    PresentationState state;
    EventTrace trace;
    check_applied(state, trace.make(RequestStarted{}));
    check_applied(state, trace.make(HttpRequestStarted{1}, RequestId{90}));
    auto invalid = trace.make(HttpRequestFinished{std::chrono::milliseconds(1),
                                                  0,
                                                  static_cast<HttpRequestOutcome>(99)},
                              RequestId{90});
    invalid.metadata.sequence = 3;
    CHECK(state.apply(invalid).status == EventApplyStatus::inconsistent);
    CHECK_EQ(state.snapshot().last_sequence, std::uint64_t(2));
    invalid.payload = HttpRequestFinished{std::chrono::milliseconds(1), 0, HttpRequestOutcome::completed};
    check_applied(state, invalid);
}

TEST(ui, mismatched_session_turn_or_request_correlation_is_rejected)
{
    PresentationState state;
    EventTrace trace;
    check_applied(state, trace.make(RequestStarted{}));
    auto wrong_session = trace.make(HttpRequestStarted{1}, RequestId{90}, std::nullopt, TurnId{1}, SessionId{42});
    wrong_session.metadata.sequence = 2;
    CHECK(state.apply(wrong_session).status == EventApplyStatus::inconsistent);
    auto wrong_turn = trace.make(HttpRequestStarted{1}, RequestId{90}, std::nullopt, TurnId{2});
    wrong_turn.metadata.sequence = 2;
    CHECK(state.apply(wrong_turn).status == EventApplyStatus::inconsistent);

    const auto start = trace.make(HttpRequestStarted{1}, RequestId{90});
    auto valid_start = start;
    valid_start.metadata.sequence = 2;
    check_applied(state, valid_start);
    auto wrong_finish = trace.make(HttpRequestFinished{std::chrono::milliseconds(1), 0,
                                                       HttpRequestOutcome::completed},
                                   RequestId{99});
    wrong_finish.metadata.sequence = 3;
    CHECK(state.apply(wrong_finish).status == EventApplyStatus::inconsistent);
    wrong_finish.metadata.sequence = state.snapshot().last_sequence + 1;
    wrong_finish.metadata.request_id = RequestId{90};
    check_applied(state, wrong_finish);

    CHECK(state.snapshot().last_request_id == RequestId{90});
    auto wrong_failure = trace.make(RequestFailed{RequestFailureCategory::protocol}, RequestId{99});
    wrong_failure.metadata.sequence = 4;
    CHECK(state.apply(wrong_failure).status == EventApplyStatus::inconsistent);
    auto missing_failure_id = trace.make(RequestFailed{RequestFailureCategory::protocol});
    missing_failure_id.metadata.sequence = 4;
    CHECK(state.apply(missing_failure_id).status == EventApplyStatus::inconsistent);
    auto correlated_failure = trace.make(RequestFailed{RequestFailureCategory::protocol}, RequestId{90});
    correlated_failure.metadata.sequence = 4;
    check_applied(state, correlated_failure);
}

TEST(ui, tool_completion_must_match_started_call_and_turn_counters)
{
    PresentationState state;
    EventTrace trace;
    const RequestId request{90};
    const ToolCallId tool{"call-one"};
    check_applied(state, trace.make(RequestStarted{}));
    check_applied(state, trace.make(HttpRequestStarted{1}, request));
    check_applied(state,
                  trace.make(HttpRequestFinished{std::chrono::milliseconds(1), 0, HttpRequestOutcome::completed},
                             request));
    check_applied(state, trace.make(ToolStarted{"alpha"}, request, tool));
    auto wrong_result = trace.make(ToolSucceeded{"alpha", std::chrono::milliseconds(2)}, request,
                                   ToolCallId{"call-other"});
    CHECK(state.apply(wrong_result).status == EventApplyStatus::inconsistent);
    wrong_result.metadata.sequence = state.snapshot().last_sequence + 1;
    wrong_result.metadata.tool_call_id = tool;
    check_applied(state, wrong_result);

    const auto bad_terminal = trace.make(TurnFinished{TurnOutcome::completed, 1, 0, 1});
    CHECK(state.apply(bad_terminal).status == EventApplyStatus::inconsistent);
}

TEST(ui, reset_allows_a_fresh_session_sequence)
{
    PresentationState state;
    EventTrace trace;
    check_applied(state, trace.make(RequestStarted{}));
    state.reset();
    CHECK(state.snapshot().phase == PresentationPhase::idle);
    CHECK_EQ(state.snapshot().last_sequence, std::uint64_t(0));
    EventTrace next_session;
    check_applied(state, next_session.make(RequestStarted{}, std::nullopt, std::nullopt, TurnId{1}, SessionId{99}));
    CHECK(state.snapshot().session_id == SessionId{99});
}

TEST(ui, terminal_capabilities_are_injectable_and_separate_stdout_from_stderr)
{
    TerminalProbe probe;
    probe.stdout_is_tty = false;
    probe.stderr_is_tty = true;
    probe.term = "xterm-256color";
    probe.locale = "en_US.UTF-8";
    probe.stdout_columns = 120;
    probe.stderr_columns = 90;
    const auto caps = inspect(probe);

    CHECK(!caps.stdout_is_tty);
    CHECK(caps.stderr_is_tty);
    CHECK(!caps.stdout_ansi);
    CHECK(caps.stderr_ansi);
    CHECK(!caps.stdout_columns);
    CHECK(caps.stderr_columns == 90U);
    CHECK(!caps.stdout_unicode);
    CHECK(caps.stderr_unicode);
    CHECK(caps.stdout_ascii_fallback);
    CHECK(!caps.stderr_ascii_fallback);
}

TEST(ui, dumb_term_disables_ansi_and_unicode)
{
    TerminalProbe probe;
    probe.stdout_is_tty = true;
    probe.stderr_is_tty = true;
    probe.term = "dumb";
    probe.locale = "C.UTF-8";
    const auto caps = inspect(probe);
    CHECK(!caps.stdout_ansi);
    CHECK(!caps.stderr_ansi);
    CHECK(!caps.stdout_unicode);
    CHECK(caps.stdout_ascii_fallback);

    probe.term = "DUMB";
    CHECK(!inspect(probe).stdout_ansi);
}

TEST(ui, no_color_is_observed_without_changing_terminal_capability)
{
    TerminalProbe probe;
    probe.stdout_is_tty = true;
    probe.term = "xterm";
    probe.no_color_present = true;
    probe.locale = "C.UTF-8";
    const auto caps = inspect(probe);
    CHECK(caps.no_color_present);
    CHECK(caps.stdout_ansi);
    // Rendering policy remains separate and must combine these two facts.
}

TEST(ui, redirected_outputs_never_report_ansi_or_unicode)
{
    TerminalProbe probe;
    probe.term = "xterm-256color";
    probe.locale = "en_US.UTF-8";
    probe.stdout_columns = 140;
    probe.stderr_columns = 140;
    const auto caps = inspect(probe);
    CHECK(!caps.stdout_ansi);
    CHECK(!caps.stderr_ansi);
    CHECK(!caps.stdout_unicode);
    CHECK(!caps.stderr_unicode);
    CHECK(!caps.stdout_columns);
    CHECK(!caps.stderr_columns);
    CHECK(caps.stdout_ascii_fallback);
    CHECK(caps.stderr_ascii_fallback);
}

TEST(ui, unknown_and_narrow_widths_are_not_invented)
{
    TerminalProbe unknown;
    unknown.stdout_is_tty = true;
    unknown.term = "xterm";
    const auto unknown_caps = inspect(unknown);
    CHECK(!unknown_caps.stdout_columns);
    CHECK(!unknown_caps.stdout_narrow);

    TerminalProbe narrow = unknown;
    narrow.stdout_columns = narrow_terminal_columns - 1;
    const auto narrow_caps = inspect(narrow);
    CHECK(narrow_caps.stdout_columns == narrow_terminal_columns - 1);
    CHECK(narrow_caps.stdout_narrow);

    narrow.stdout_columns = narrow_terminal_columns;
    CHECK(!inspect(narrow).stdout_narrow);

    narrow.stdout_columns = 0;
    const auto zero_width = inspect(narrow);
    CHECK(!zero_width.stdout_columns);
    CHECK(!zero_width.stdout_narrow);
}

TEST(ui, unicode_requires_utf8_locale_and_has_ascii_fallback)
{
    TerminalProbe probe;
    probe.stdout_is_tty = true;
    probe.term = "xterm";
    probe.locale = "C";
    auto caps = inspect(probe);
    CHECK(!caps.stdout_unicode);
    CHECK(caps.stdout_ascii_fallback);

    probe.locale = "fr_FR.utf8";
    caps = inspect(probe);
    CHECK(caps.stdout_unicode);
    CHECK(!caps.stdout_ascii_fallback);
}

TEST(ui, live_terminal_detection_returns_consistent_observations)
{
    const auto caps = detect_terminal_capabilities();
    if (!caps.stdout_is_tty) {
        CHECK(!caps.stdout_ansi);
        CHECK(!caps.stdout_columns);
    }
    if (!caps.stderr_is_tty) {
        CHECK(!caps.stderr_ansi);
        CHECK(!caps.stderr_columns);
    }
}
