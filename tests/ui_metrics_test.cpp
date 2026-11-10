#include "agent.hpp"
#include "mock_http_server.hpp"
#include "test_support.hpp"
#include "tool_registry.hpp"
#include "ui/console_renderer.hpp"
#include "ui/console_session.hpp"
#include "ui/metrics.hpp"
#include "ui/presentation_controller.hpp"
#include "ui/terminal_capabilities.hpp"
#include "ui/tui_view.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace {

using namespace std::chrono_literals;

agent::ui::AgentEvent make_event(std::uint64_t sequence,
                                std::chrono::steady_clock::duration elapsed,
                                agent::ui::EventPayload payload,
                                std::optional<agent::ui::RequestId> request_id = std::nullopt,
                                std::optional<agent::ui::ToolCallId> tool_call_id = std::nullopt,
                                agent::ui::TurnId turn_id = {1})
{
    agent::ui::EventMetadata metadata;
    metadata.session_id = {9};
    metadata.turn_id = turn_id;
    metadata.request_id = request_id;
    metadata.tool_call_id = tool_call_id;
    metadata.sequence = sequence;
    metadata.elapsed = elapsed;
    return {std::move(metadata), std::move(payload)};
}

std::string text_event(std::string_view text)
{
    const nlohmann::json event{{"choices", nlohmann::json::array({{{"index", 0}, {"delta", {{"content", text}}}}})}};
    return "data: " + event.dump() + "\n\n";
}

} // namespace

TEST(ui4, completed_turn_uses_observed_elapsed_and_first_delta)
{
    agent::ui::MetricCollector collector;
    const agent::ui::RequestId request_id{1};
    collector.consume(make_event(1, 10ms, agent::ui::RequestStarted{}));
    collector.consume(make_event(2, 12ms, agent::ui::HttpRequestStarted{1}, request_id));
    collector.consume(make_event(3, 35ms, agent::ui::TextDelta{"one"}, request_id));
    collector.consume(make_event(4, 42ms, agent::ui::TextDelta{" two"}, request_id));
    collector.consume(make_event(5, 75ms,
                                 agent::ui::HttpRequestFinished{60ms, 240, agent::ui::HttpRequestOutcome::completed},
                                 request_id));
    collector.consume(make_event(6, 90ms, agent::ui::TurnFinished{agent::ui::TurnOutcome::completed, 1, 0, 0}));

    const auto& metrics = collector.snapshot();
    CHECK(metrics.turn_id == agent::ui::TurnId{1});
    CHECK(metrics.duration == 80ms);
    CHECK(metrics.first_delta_latency == 25ms);
    CHECK(metrics.outcome == agent::ui::TurnOutcome::completed);
    CHECK_EQ(metrics.text_fragments, std::uint64_t(2));
    CHECK_EQ(metrics.http_requests.size(), std::size_t(1));
    CHECK(metrics.http_requests[0].duration == 60ms);
    CHECK(metrics.http_requests[0].response_body_bytes == std::uint64_t(240));
    CHECK(metrics.http_requests[0].outcome == agent::ui::HttpRequestOutcome::completed);
    CHECK(metrics.response_body_bytes_received() == std::uint64_t(240));
}

TEST(ui4, multiple_requests_and_tools_keep_individual_measures_and_recovery)
{
    agent::ui::MetricCollector collector;
    const agent::ui::RequestId first_request{1};
    const agent::ui::RequestId second_request{2};
    const agent::ui::ToolCallId failed_tool{"call-failed"};
    const agent::ui::ToolCallId successful_tool{"call-success"};
    collector.consume(make_event(1, 0ms, agent::ui::RequestStarted{}));
    collector.consume(make_event(2, 1ms, agent::ui::HttpRequestStarted{1}, first_request));
    collector.consume(make_event(3, 20ms,
                                 agent::ui::HttpRequestFinished{19ms, 100, agent::ui::HttpRequestOutcome::completed},
                                 first_request));
    collector.consume(make_event(4, 22ms, agent::ui::ToolStarted{"failed"}, first_request, failed_tool));
    collector.consume(make_event(5, 26ms, agent::ui::ToolFailed{"failed", 4ms}, first_request, failed_tool));
    collector.consume(make_event(6, 27ms, agent::ui::ToolStarted{"succeeded"}, first_request, successful_tool));
    collector.consume(make_event(7, 34ms, agent::ui::ToolSucceeded{"succeeded", 7ms}, first_request, successful_tool));
    collector.consume(make_event(8, 40ms, agent::ui::HttpRequestStarted{2}, second_request));
    collector.consume(make_event(9, 45ms, agent::ui::TextDelta{"recovered"}, second_request));
    collector.consume(make_event(10, 50ms,
                                 agent::ui::HttpRequestFinished{10ms, 80, agent::ui::HttpRequestOutcome::completed},
                                 second_request));
    collector.consume(make_event(11, 55ms, agent::ui::TurnFinished{agent::ui::TurnOutcome::completed, 2, 1, 1}));

    const auto& metrics = collector.snapshot();
    CHECK(metrics.duration == 55ms);
    CHECK(metrics.first_delta_latency == 45ms);
    CHECK(metrics.outcome == agent::ui::TurnOutcome::completed);
    CHECK_EQ(metrics.http_requests.size(), std::size_t(2));
    CHECK(metrics.http_requests[0].duration == 19ms);
    CHECK(metrics.http_requests[1].duration == 10ms);
    CHECK_EQ(metrics.tools.size(), std::size_t(2));
    CHECK_EQ(metrics.tools[0].name, std::string("failed"));
    CHECK_EQ(metrics.tools[1].name, std::string("succeeded"));
    CHECK(metrics.tools[0].duration == 4ms);
    CHECK(metrics.tools[0].outcome == agent::ui::ToolMetricOutcome::failed);
    CHECK(metrics.tools[1].duration == 7ms);
    CHECK(metrics.tools[1].outcome == agent::ui::ToolMetricOutcome::succeeded);
    CHECK_EQ(metrics.successful_tools(), std::uint64_t(1));
    CHECK_EQ(metrics.failed_tools(), std::uint64_t(1));
    CHECK(metrics.response_body_bytes_received() == std::uint64_t(180));
}

TEST(ui4, absent_first_delta_and_unfinished_request_remain_unavailable)
{
    agent::ui::MetricCollector collector;
    const agent::ui::RequestId request_id{4};
    collector.consume(make_event(1, 3ms, agent::ui::RequestStarted{}));
    collector.consume(make_event(2, 4ms, agent::ui::HttpRequestStarted{1}, request_id));

    const auto& metrics = collector.snapshot();
    CHECK(!metrics.duration.has_value());
    CHECK(!metrics.first_delta_latency.has_value());
    CHECK(!metrics.outcome.has_value());
    CHECK_EQ(metrics.text_fragments, std::uint64_t(0));
    CHECK_EQ(metrics.http_requests.size(), std::size_t(1));
    CHECK(!metrics.http_requests[0].duration.has_value());
    CHECK(!metrics.http_requests[0].response_body_bytes.has_value());
    CHECK(!metrics.response_body_bytes_received().has_value());
}

TEST(ui4, failed_turn_without_delta_has_real_zero_fragments_and_no_first_delta)
{
    agent::ui::MetricCollector collector;
    const agent::ui::RequestId request_id{8};
    collector.consume(make_event(1, 2ms, agent::ui::RequestStarted{}));
    collector.consume(make_event(2, 3ms, agent::ui::HttpRequestStarted{1}, request_id));
    collector.consume(make_event(3, 7ms,
                                 agent::ui::HttpRequestFinished{4ms, 9, agent::ui::HttpRequestOutcome::failed},
                                 request_id));
    collector.consume(make_event(4, 7ms, agent::ui::RequestFailed{agent::ui::RequestFailureCategory::http},
                                 request_id));
    collector.consume(make_event(5, 8ms,
                                 agent::ui::TurnFinished{agent::ui::TurnOutcome::failed, 1, 0, 0}));

    const auto& metrics = collector.snapshot();
    CHECK(metrics.duration == 6ms);
    CHECK(metrics.outcome == agent::ui::TurnOutcome::failed);
    CHECK_EQ(metrics.text_fragments, std::uint64_t(0));
    CHECK(!metrics.first_delta_latency.has_value());
    CHECK_EQ(metrics.http_requests.size(), std::size_t(1));
    CHECK(metrics.http_requests[0].outcome == agent::ui::HttpRequestOutcome::failed);
    CHECK(metrics.response_body_bytes_received() == std::uint64_t(9));
}

TEST(ui4, request_started_resets_metrics_for_the_next_turn)
{
    agent::ui::MetricCollector collector;
    const agent::ui::RequestId first_request{1};
    collector.consume(make_event(1, 0ms, agent::ui::RequestStarted{}));
    collector.consume(make_event(2, 1ms, agent::ui::HttpRequestStarted{1}, first_request));
    collector.consume(make_event(3, 2ms, agent::ui::TextDelta{"old"}, first_request));
    collector.consume(make_event(4, 5ms,
                                 agent::ui::HttpRequestFinished{4ms, 12, agent::ui::HttpRequestOutcome::completed},
                                 first_request));
    collector.consume(make_event(5, 8ms, agent::ui::TurnFinished{agent::ui::TurnOutcome::completed, 1, 0, 0}));

    collector.consume(make_event(6, 20ms, agent::ui::RequestStarted{}, std::nullopt, std::nullopt, {2}));
    const auto& metrics = collector.snapshot();
    CHECK(metrics.turn_id == agent::ui::TurnId{2});
    CHECK(!metrics.duration.has_value());
    CHECK(!metrics.first_delta_latency.has_value());
    CHECK(!metrics.outcome.has_value());
    CHECK_EQ(metrics.http_requests.size(), std::size_t(0));
    CHECK_EQ(metrics.text_fragments, std::uint64_t(0));
}

TEST(ui4, http_finish_is_correlated_by_request_id_and_orphans_make_totals_unavailable)
{
    agent::ui::MetricCollector collector;
    const agent::ui::RequestId expected{10};
    const agent::ui::RequestId unrelated{11};
    collector.consume(make_event(1, 0ms, agent::ui::RequestStarted{}));
    collector.consume(make_event(2, 1ms, agent::ui::HttpRequestStarted{1}, expected));
    collector.consume(make_event(3, 2ms,
                                 agent::ui::HttpRequestFinished{1ms, 99, agent::ui::HttpRequestOutcome::completed},
                                 unrelated));
    CHECK(!collector.snapshot().event_details_complete);
    CHECK(!collector.snapshot().response_body_bytes_received().has_value());
    CHECK(!collector.snapshot().http_requests.front().response_body_bytes.has_value());
    collector.consume(make_event(4, 3ms,
                                 agent::ui::HttpRequestFinished{2ms, 7, agent::ui::HttpRequestOutcome::completed},
                                 expected));
    CHECK_EQ(collector.snapshot().http_requests_finished, std::uint64_t(2));
    CHECK(!collector.snapshot().response_body_bytes_received().has_value());
}

TEST(ui4, retained_request_details_are_bounded_and_fifo)
{
    agent::ui::MetricCollector collector;
    collector.consume(make_event(1, 0ms, agent::ui::RequestStarted{}));
    std::uint64_t sequence = 2;
    for (std::uint64_t id = 1; id <= agent::ui::max_http_metric_details + 1; ++id) {
        collector.consume(make_event(sequence++, std::chrono::milliseconds(id), agent::ui::HttpRequestStarted{1},
                                     agent::ui::RequestId{id}));
        collector.consume(make_event(sequence++, std::chrono::milliseconds(id + 1),
                                     agent::ui::HttpRequestFinished{1ms, id, agent::ui::HttpRequestOutcome::completed},
                                     agent::ui::RequestId{id}));
    }
    const auto& metrics = collector.snapshot();
    CHECK_EQ(metrics.http_requests_started, std::uint64_t(agent::ui::max_http_metric_details + 1));
    CHECK_EQ(metrics.http_requests.size(), agent::ui::max_http_metric_details);
    CHECK_EQ(metrics.evicted_http_details, std::uint64_t(1));
    CHECK(metrics.detail_history_truncated);
    CHECK_EQ(metrics.http_requests.front().request_id.value, std::uint64_t(2));
    CHECK_EQ(metrics.http_requests.back().request_id.value,
             std::uint64_t(agent::ui::max_http_metric_details + 1));
}

TEST(ui4, tool_detail_id_and_name_byte_limits_are_enforced)
{
    agent::ui::MetricCollector collector;
    const agent::ui::RequestId request{4};
    collector.consume(make_event(1, 0ms, agent::ui::RequestStarted{}));
    const std::string first(agent::ui::max_tool_id_metric_bytes / 2, 'a');
    const std::string second(agent::ui::max_tool_id_metric_bytes / 2, 'b');
    const std::string oversized_id(agent::ui::max_tool_id_metric_bytes + 1, 'x');
    const std::string oversized_name(agent::ui::max_tool_name_metric_bytes + 1, 'n');
    collector.consume(make_event(2, 1ms, agent::ui::ToolStarted{"one"}, request, agent::ui::ToolCallId{first}));
    collector.consume(make_event(3, 2ms, agent::ui::ToolStarted{"two"}, request, agent::ui::ToolCallId{second}));
    collector.consume(make_event(4, 3ms, agent::ui::ToolStarted{"three"}, request, agent::ui::ToolCallId{"c"}));
    collector.consume(make_event(5, 4ms, agent::ui::ToolStarted{"large-id"}, request,
                                 agent::ui::ToolCallId{oversized_id}));
    collector.consume(make_event(6, 5ms, agent::ui::ToolStarted{oversized_name}, request,
                                 agent::ui::ToolCallId{"d"}));
    const auto& metrics = collector.snapshot();
    CHECK_EQ(metrics.tools_started, std::uint64_t(5));
    CHECK_EQ(metrics.tools.size(), std::size_t(2));
    CHECK_EQ(metrics.evicted_tool_details, std::uint64_t(1));
    CHECK_EQ(metrics.rejected_tool_details, std::uint64_t(2));
    CHECK(metrics.retained_tool_id_bytes <= agent::ui::max_tool_id_metric_bytes);
    CHECK(metrics.detail_history_truncated);
}

TEST(ui4, metrics_view_shows_partial_even_when_request_start_was_lost)
{
    agent::ui::TuiView view;
    agent::ui::TurnMetrics metrics;
    metrics.event_details_complete = false;
    view.set_metrics(std::move(metrics));
    const auto rows = view.rendered_page_rows(agent::ui::TuiPage::metrics, 80);
    CHECK(std::any_of(rows.begin(), rows.end(), [](const std::string& row) {
        return row.find("PARTIAL:") != std::string::npos;
    }));
    CHECK(view.status_line(agent::ui::TuiPage::metrics, agent::ui::TuiFocus::content, 80).find("PARTIAL") != std::string::npos);
}

TEST(ui4, authorization_events_are_not_counted_as_executed_tool_results)
{
    agent::ui::MetricCollector collector;
    const agent::ui::RequestId request{3};
    const agent::ui::ToolCallId tool{"approval-only"};
    collector.consume(make_event(1, 0ms, agent::ui::RequestStarted{}));
    collector.consume(make_event(2, 1ms, agent::ui::ToolAuthorizationRequested{"write_file"}, request, tool));
    collector.consume(make_event(3, 2ms,
                                 agent::ui::ToolAuthorizationResolved{agent::ui::ToolAuthorizationStatus::denied},
                                 request, tool));
    CHECK_EQ(collector.snapshot().tools_started, std::uint64_t(0));
    CHECK_EQ(collector.snapshot().tools_succeeded, std::uint64_t(0));
    CHECK_EQ(collector.snapshot().tools_failed, std::uint64_t(0));
    CHECK(collector.snapshot().tools.empty());
}

TEST(ui4, tool_names_are_bounded_and_rendered_without_arguments_or_results)
{
    agent::ui::MetricCollector collector;
    const agent::ui::RequestId request{8};
    const agent::ui::ToolCallId tool{"call-8"};
    collector.consume(make_event(1, 0ms, agent::ui::RequestStarted{}));
    collector.consume(make_event(2, 1ms, agent::ui::ToolStarted{"list_files"}, request, tool));
    agent::ui::TuiView view;
    view.set_metrics(collector.snapshot());
    const auto rows = view.rendered_page_rows(agent::ui::TuiPage::metrics, 100);
    CHECK(std::any_of(rows.begin(), rows.end(), [](const std::string& row) {
        return row.find("Tool list_files (request 8)") != std::string::npos;
    }));
    CHECK(std::none_of(rows.begin(), rows.end(), [](const std::string& row) {
        return row.find("call-8") != std::string::npos || row.find("arguments") != std::string::npos;
    }));
}

TEST(ui4, cli_presents_measured_metrics_without_duplicate_response_or_fake_progress)
{
    const auto body = text_event("measured response") + "data: [DONE]\n\n";
    MockHttpServer server;
    MockHttpServer::Response mock_response;
    mock_response.status = 200;
    mock_response.body = body;
    server.enqueue(std::move(mock_response));

    agent::ui::TerminalProbe probe;
    const auto caps = agent::ui::terminal_capabilities_from_probe(probe);
    std::ostringstream output;
    std::ostringstream diagnostics;
    auto controller = std::make_shared<agent::ui::PresentationController>(
        agent::ui::make_console_renderer(output, diagnostics, caps));
    ToolRegistry tools;
    AgentConfig config{server.url("/v1/chat/completions"), "local-model", "", 3};
    Agent agent(tools, std::move(config), controller);

    CHECK(agent::ui::run_console_turn(agent, "question", server.url("/v1/chat/completions"), *controller)
          == agent::ui::ConsoleTurnResult::completed);
    CHECK_EQ(output.str(), std::string("measured response\n"));
    CHECK_EQ(controller->metrics().text_fragments, std::uint64_t(1));
    CHECK_EQ(controller->metrics().http_requests.size(), std::size_t(1));
    CHECK(controller->metrics().duration.has_value());
    CHECK(controller->metrics().first_delta_latency.has_value());
    CHECK(controller->metrics().response_body_bytes_received() == static_cast<std::uint64_t>(body.size()));
    CHECK(diagnostics.str().find("HTTP request 1 finished in") != std::string::npos);
    CHECK(diagnostics.str().find("first delta observed after") != std::string::npos);
    CHECK(diagnostics.str().find("text fragments 1") != std::string::npos);
    CHECK(diagnostics.str().find("response body bytes received: " + std::to_string(body.size())) != std::string::npos);
    CHECK(diagnostics.str().find("tokens/s") == std::string::npos);
    CHECK(diagnostics.str().find('%') == std::string::npos);
}

TEST(ui4, rich_metrics_respect_no_color_without_changing_response_text)
{
    MockHttpServer server;
    MockHttpServer::Response mock_response;
    mock_response.status = 200;
    mock_response.body = text_event("rich response") + "data: [DONE]\n\n";
    server.enqueue(std::move(mock_response));

    agent::ui::TerminalProbe probe;
    probe.stdout_is_tty = true;
    probe.stderr_is_tty = true;
    probe.term = "xterm-256color";
    probe.no_color_present = true;
    probe.locale = "C";
    const auto caps = agent::ui::terminal_capabilities_from_probe(probe);
    std::ostringstream output;
    std::ostringstream diagnostics;
    auto controller = std::make_shared<agent::ui::PresentationController>(
        agent::ui::make_console_renderer(output, diagnostics, caps));
    ToolRegistry tools;
    AgentConfig config{server.url("/v1/chat/completions"), "local-model", "", 3};
    Agent agent(tools, std::move(config), controller);

    CHECK(controller->mode() == agent::ui::RendererMode::rich);
    CHECK(agent::ui::run_console_turn(agent, "question", server.url("/v1/chat/completions"), *controller)
          == agent::ui::ConsoleTurnResult::completed);
    CHECK_EQ(output.str(), std::string("rich response\n"));
    CHECK(diagnostics.str().find("Turn metrics (technical outcome: completed)") != std::string::npos);
    CHECK(diagnostics.str().find('\x1b') == std::string::npos);
    CHECK(!controller->metrics().http_requests.empty());
}
