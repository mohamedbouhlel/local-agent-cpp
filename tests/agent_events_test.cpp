#include "agent.hpp"
#include "http_client.hpp"
#include "mock_http_server.hpp"
#include "test_support.hpp"
#include "tool_registry.hpp"
#include "ui/presentation_state.hpp"
#include "ui/metrics.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using agent::ui::AgentEvent;
using agent::ui::EventType;

std::string sse(const nlohmann::json& event)
{
    return "data: " + event.dump() + "\n\n";
}

std::string done()
{
    return "data: [DONE]\n\n";
}

std::string text_delta(const std::string& text, const std::string& finish_reason = {})
{
    nlohmann::json choice{{"index", 0}, {"delta", {{"content", text}}}};
    if (!finish_reason.empty()) {
        choice["finish_reason"] = finish_reason;
    }
    return sse({{"choices", nlohmann::json::array({choice})}});
}

std::string tool_delta(std::size_t index,
                       const std::string& id,
                       const std::string& name,
                       const nlohmann::json& arguments)
{
    const nlohmann::json call{{"index", index},
                              {"id", id},
                              {"type", "function"},
                              {"function", {{"name", name}, {"arguments", arguments.dump()}}}};
    const nlohmann::json choice{{"index", 0},
                                {"delta", {{"tool_calls", nlohmann::json::array({call})}}}};
    return sse({{"choices", nlohmann::json::array({choice})}});
}

std::string tool_delta_fragment(std::size_t index,
                               const std::string& id,
                               const std::string& name_fragment,
                               const std::string& arguments_fragment)
{
    nlohmann::json function = nlohmann::json::object();
    if (!name_fragment.empty()) {
        function["name"] = name_fragment;
    }
    if (!arguments_fragment.empty()) {
        function["arguments"] = arguments_fragment;
    }
    nlohmann::json call{{"index", index}, {"function", function}};
    if (!id.empty()) {
        call["id"] = id;
    }
    const nlohmann::json choice{{"index", 0}, {"delta", {{"tool_calls", nlohmann::json::array({call})}}}};
    return sse({{"choices", nlohmann::json::array({choice})}});
}

MockHttpServer::Response response(std::string body, int status = 200)
{
    MockHttpServer::Response result;
    result.status = status;
    result.body = std::move(body);
    return result;
}

AgentConfig config_for(const MockHttpServer& server, long timeout = 5)
{
    return {server.url("/v1/chat/completions"), "local-model", "", timeout};
}

ToolRegistry make_tools(bool include_failure = false)
{
    ToolRegistry tools;
    const auto schema = nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()}};
    tools.register_tool(Tool{"alpha", "test alpha", schema, [](nlohmann::json args) {
                                 return "alpha:" + args.value("value", std::string());
                             }});
    tools.register_tool(Tool{"beta", "test beta", schema, [](nlohmann::json args) {
                                 return "beta:" + args.value("value", std::string());
                             }});
    if (include_failure) {
        tools.register_tool(Tool{"fail", "test failure", schema, [](nlohmann::json) -> std::string {
                                     throw std::runtime_error("handler failed");
                                 }});
    }
    return tools;
}

class RecordingObserver : public agent::ui::EventObserver {
public:
    void on_event(const AgentEvent& event) override
    {
        if (throw_on && event.type() == *throw_on) {
            throw std::runtime_error("observer failed");
        }
        events.push_back(event);
    }

    void on_observer_failure() override
    {
        ++diagnostics;
        if (throw_from_diagnostic) {
            throw std::runtime_error("diagnostic failed");
        }
    }

    std::vector<AgentEvent> events;
    std::optional<EventType> throw_on;
    int diagnostics = 0;
    bool throw_from_diagnostic = false;
};

std::vector<EventType> event_types(const std::vector<AgentEvent>& events)
{
    std::vector<EventType> result;
    for (const auto& event : events) {
        result.push_back(event.type());
    }
    return result;
}

std::size_t count_type(const std::vector<AgentEvent>& events, EventType type)
{
    return static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [type](const auto& event) {
        return event.type() == type;
    }));
}

void check_single_terminal_sequence(const std::vector<AgentEvent>& events)
{
    CHECK_EQ(count_type(events, EventType::TurnFinished), std::size_t(1));
    CHECK_EQ(count_type(events, EventType::RequestFailed) <= 1, true);
    CHECK_EQ(count_type(events, EventType::HttpRequestStarted),
             count_type(events, EventType::HttpRequestFinished));
    for (std::size_t i = 1; i < events.size(); ++i) {
        CHECK(events[i - 1].metadata.sequence < events[i].metadata.sequence);
        CHECK(events[i - 1].metadata.elapsed <= events[i].metadata.elapsed);
        CHECK(events[i - 1].metadata.session_id == events[i].metadata.session_id);
        CHECK(events[i - 1].metadata.turn_id == events[i].metadata.turn_id);
    }
}

void check_malformed_sse_rejected(const std::string& body, const std::string& expected_partial_text)
{
    MockHttpServer server;
    server.enqueue(response(body));
    server.enqueue(response(text_delta("recovered") + done()));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);

    std::string streamed_text;
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [&streamed_text](const std::string& delta) { streamed_text += delta; };
    CHECK_THROWS_MSG(agent.send("first question", callbacks), "malformed JSON in model stream event");
    CHECK_EQ(streamed_text, expected_partial_text);
    CHECK_EQ(count_type(observer->events, EventType::RequestFailed), std::size_t(1));
    CHECK_EQ(count_type(observer->events, EventType::TurnFinished), std::size_t(1));
    CHECK(std::get<agent::ui::RequestFailed>(observer->events[observer->events.size() - 2].payload).category
          == agent::ui::RequestFailureCategory::protocol);
    CHECK(std::get<agent::ui::TurnFinished>(observer->events.back().payload).outcome
          == agent::ui::TurnOutcome::failed);
    const auto http_finished = std::find_if(observer->events.begin(), observer->events.end(), [](const auto& event) {
        return event.type() == EventType::HttpRequestFinished;
    });
    CHECK(http_finished != observer->events.end());
    if (http_finished != observer->events.end()) {
        CHECK(std::get<agent::ui::HttpRequestFinished>(http_finished->payload).outcome
              == agent::ui::HttpRequestOutcome::completed);
    }
    check_single_terminal_sequence(observer->events);

    agent::ui::PresentationState presentation;
    for (const auto& event : observer->events) {
        CHECK(presentation.apply(event).applied());
    }
    CHECK(presentation.snapshot().phase == agent::ui::PresentationPhase::failed);

    CHECK_EQ(agent.send("second question"), std::string("recovered"));
    const auto requests = server.received_bodies();
    CHECK_EQ(requests.size(), std::size_t(2));
    if (requests.size() == 2) {
        const auto messages = nlohmann::json::parse(requests[1]).at("messages");
        std::vector<std::string> roles;
        for (const auto& message : messages) roles.push_back(message.value("role", "<absent>"));
        CHECK(roles == std::vector<std::string>({"user", "user"}));
    }
}

struct LegacyCallbackError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

} // namespace

TEST(agent, event_text_stream_and_existing_callbacks_remain_compatible)
{
    MockHttpServer server;
    server.enqueue(response(text_delta("bonjour") + done()));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);

    std::string old_stream;
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [&old_stream](const std::string& part) { old_stream += part; };
    CHECK_EQ(agent.send("salut", callbacks), std::string("bonjour"));
    CHECK_EQ(old_stream, std::string("bonjour"));
    CHECK(event_types(observer->events)
          == (std::vector<EventType>{EventType::RequestStarted,
                                     EventType::HttpRequestStarted,
                                     EventType::TextDelta,
                                     EventType::HttpRequestFinished,
                                     EventType::TurnFinished}));
    check_single_terminal_sequence(observer->events);
}

TEST(agent, event_tool_calls_entrelaces_correlent_deux_requetes_et_callbacks_legacy)
{
    MockHttpServer server;
    server.enqueue(response(tool_delta_fragment(1, "call-beta", "be", R"({"value":)")
                            + tool_delta_fragment(0, "call-alpha", "al", R"({"value":)")
                            + tool_delta_fragment(1, "", "ta", R"("B"})")
                            + tool_delta_fragment(0, "", "pha", R"("A"})") + done()));
    server.enqueue(response(text_delta("termine") + done()));
    auto tools = make_tools();
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);

    std::vector<std::string> legacy_calls;
    std::vector<ToolOutcome> legacy_results;
    AgentCallbacks callbacks;
    callbacks.on_tool_call = [&legacy_calls](const std::string& name) { legacy_calls.push_back(name); };
    callbacks.on_tool_result = [&legacy_results](const ToolOutcome& value) { legacy_results.push_back(value); };
    CHECK_EQ(agent.send("lance les outils", callbacks), std::string("termine"));

    CHECK(legacy_calls == (std::vector<std::string>{"alpha", "beta"}));
    CHECK_EQ(legacy_results.size(), std::size_t(2));
    CHECK(legacy_results[0].ok);
    CHECK_EQ(legacy_results[0].result, std::string("alpha:A"));
    CHECK(legacy_results[1].ok);
    CHECK_EQ(legacy_results[1].result, std::string("beta:B"));

    CHECK_EQ(count_type(observer->events, EventType::HttpRequestStarted), std::size_t(2));
    CHECK_EQ(count_type(observer->events, EventType::ToolStarted), std::size_t(2));
    CHECK_EQ(count_type(observer->events, EventType::ToolSucceeded), std::size_t(2));
    CHECK_EQ(count_type(observer->events, EventType::ToolFailed), std::size_t(0));
    CHECK_EQ(count_type(observer->events, EventType::TurnFinished), std::size_t(1));

    std::vector<std::string> tool_ids;
    std::vector<std::string> completed_tool_ids;
    std::vector<std::uint64_t> request_ids;
    std::vector<std::uint64_t> finished_request_ids;
    for (const auto& event : observer->events) {
        if (event.type() == EventType::ToolStarted) {
            CHECK(event.metadata.tool_call_id.has_value());
            tool_ids.push_back(event.metadata.tool_call_id->value);
        }
        if (event.type() == EventType::ToolSucceeded) {
            CHECK(event.metadata.tool_call_id.has_value());
            completed_tool_ids.push_back(event.metadata.tool_call_id->value);
        }
        if (event.type() == EventType::HttpRequestStarted) {
            CHECK(event.metadata.request_id.has_value());
            request_ids.push_back(event.metadata.request_id->value);
        }
        if (event.type() == EventType::HttpRequestFinished) {
            CHECK(event.metadata.request_id.has_value());
            finished_request_ids.push_back(event.metadata.request_id->value);
        }
    }
    CHECK(tool_ids == (std::vector<std::string>{"call-alpha", "call-beta"}));
    CHECK(completed_tool_ids == tool_ids);
    CHECK_EQ(request_ids.size(), std::size_t(2));
    CHECK(finished_request_ids == request_ids);
    CHECK(request_ids[0] != request_ids[1]);
    check_single_terminal_sequence(observer->events);
}

TEST(agent, failed_tool_is_reported_and_turn_can_complete)
{
    MockHttpServer server;
    server.enqueue(response(tool_delta(0, "call-fail", "fail", nlohmann::json::object()) + done()));
    server.enqueue(response(text_delta("recupere") + done()));
    auto tools = make_tools(true);
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);

    CHECK_EQ(agent.send("question"), std::string("recupere"));
    CHECK_EQ(count_type(observer->events, EventType::ToolStarted), std::size_t(1));
    CHECK_EQ(count_type(observer->events, EventType::ToolSucceeded), std::size_t(0));
    CHECK_EQ(count_type(observer->events, EventType::ToolFailed), std::size_t(1));
    CHECK_EQ(count_type(observer->events, EventType::RequestFailed), std::size_t(0));
    const auto& terminal = std::get<agent::ui::TurnFinished>(observer->events.back().payload);
    CHECK(terminal.outcome == agent::ui::TurnOutcome::completed);
    CHECK_EQ(terminal.tools_failed, std::uint32_t(1));

    // A true execution error is fed back as the matching tool message; the
    // next model response (and any repeated request) can observe the failure.
    const auto requests = server.received_bodies();
    CHECK_EQ(requests.size(), std::size_t(2));
    if (requests.size() == 2) {
        const auto messages = nlohmann::json::parse(requests[1]).at("messages");
        CHECK_EQ(messages.size(), std::size_t(3));
        CHECK_EQ(messages.at(1).at("role").get<std::string>(), std::string("assistant"));
        CHECK_EQ(messages.at(1).at("tool_calls").at(0).at("id").get<std::string>(), std::string("call-fail"));
        CHECK_EQ(messages.at(2).at("role").get<std::string>(), std::string("tool"));
        CHECK_EQ(messages.at(2).at("tool_call_id").get<std::string>(), std::string("call-fail"));
        CHECK_EQ(messages.at(2).at("content").get<std::string>(), std::string("tool error: handler failed"));
    }
    check_single_terminal_sequence(observer->events);
}

TEST(agent, non_standard_tool_exception_is_reported_and_rethrown_unchanged)
{
    MockHttpServer server;
    server.enqueue(response(tool_delta(0, "call-nonstd", "nonstd", nlohmann::json::object()) + done()));
    ToolRegistry tools;
    tools.register_tool(Tool{"nonstd",
                             "test non-standard exception",
                             nlohmann::json::object(),
                             [](nlohmann::json) -> std::string { throw 73; }});
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);

    bool original_value_preserved = false;
    try {
        (void)agent.send("question");
    } catch (int value) {
        original_value_preserved = value == 73;
    }
    CHECK(original_value_preserved);
    CHECK_EQ(count_type(observer->events, EventType::ToolStarted), std::size_t(1));
    CHECK_EQ(count_type(observer->events, EventType::ToolFailed), std::size_t(1));
    CHECK_EQ(count_type(observer->events, EventType::ToolSucceeded), std::size_t(0));
    CHECK_EQ(std::get<agent::ui::TurnFinished>(observer->events.back().payload).tools_failed, std::uint32_t(1));
    CHECK(std::get<agent::ui::TurnFinished>(observer->events.back().payload).outcome
          == agent::ui::TurnOutcome::failed);
    check_single_terminal_sequence(observer->events);
}

TEST(agent, malformed_sse_json_followed_by_done_is_protocol_failure)
{
    check_malformed_sse_rejected("data: {not-json}\n\n" + done(), "");
}

TEST(agent, malformed_sse_json_after_text_delta_keeps_only_provisional_text)
{
    check_malformed_sse_rejected(text_delta("provisional") + "data: {not-json}\n\n" + done(), "provisional");
}

TEST(agent, malformed_sse_json_before_valid_delta_does_not_resume_streaming)
{
    check_malformed_sse_rejected("data: {not-json}\n\n" + text_delta("must-not-appear") + done(), "");
}

TEST(agent, malformed_stream_still_obeys_configured_http_timeout)
{
    MockHttpServer server;
    auto slow = response("data: {not-json}\n\n" + std::string(100, 'x'));
    slow.piece_size = 1;
    slow.piece_delay_us = 30'000;
    server.enqueue(std::move(slow));
    server.enqueue(response(text_delta("recovered") + done()));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    auto config = config_for(server, 2);
    // Keep the idle bound above the total bound so this test verifies total
    // timeout deterministically instead of racing two equal one-second limits.
    config.http_timeouts = HttpTimeouts{1, 5, 2};
    Agent agent(tools, std::move(config), observer);

    std::string error_message;
    try {
        (void)agent.send("first question");
    } catch (const HttpError& error) {
        error_message = error.what();
    }
    CHECK(error_message.find("total timeout after 2s") != std::string::npos);
    CHECK(std::get<agent::ui::HttpRequestFinished>(observer->events[2].payload).outcome
          == agent::ui::HttpRequestOutcome::failed);
    CHECK(std::get<agent::ui::RequestFailed>(observer->events[observer->events.size() - 2].payload).category
          == agent::ui::RequestFailureCategory::http);
    CHECK(std::get<agent::ui::TurnFinished>(observer->events.back().payload).outcome
          == agent::ui::TurnOutcome::failed);
    check_single_terminal_sequence(observer->events);

    CHECK_EQ(agent.send("second question"), std::string("recovered"));
    const auto requests = server.received_bodies();
    CHECK_EQ(requests.size(), std::size_t(2));
    if (requests.size() == 2) {
        const auto messages = nlohmann::json::parse(requests[1]).at("messages");
        std::vector<std::string> roles;
        for (const auto& message : messages) roles.push_back(message.value("role", "<absent>"));
        CHECK(roles == std::vector<std::string>({"user", "user"}));
    }
}

TEST(agent, missing_done_emits_failure_without_false_turn_success)
{
    MockHttpServer server;
    server.enqueue(response(text_delta("partial")));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);

    CHECK_THROWS_MSG(agent.send("question"), "missing [DONE]");
    CHECK(std::get<agent::ui::HttpRequestFinished>(observer->events[3].payload).outcome
          == agent::ui::HttpRequestOutcome::completed);
    CHECK(std::get<agent::ui::RequestFailed>(observer->events[4].payload).category
          == agent::ui::RequestFailureCategory::protocol);
    CHECK(std::get<agent::ui::TurnFinished>(observer->events[5].payload).outcome == agent::ui::TurnOutcome::failed);
    check_single_terminal_sequence(observer->events);
}

TEST(agent, length_finish_reason_emits_protocol_failure)
{
    MockHttpServer server;
    server.enqueue(response(text_delta("partial", "length") + done()));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);

    CHECK_THROWS_MSG(agent.send("question"), "finish_reason=length");
    CHECK(std::get<agent::ui::RequestFailed>(observer->events[4].payload).category
          == agent::ui::RequestFailureCategory::protocol);
    CHECK(std::get<agent::ui::TurnFinished>(observer->events[5].payload).outcome == agent::ui::TurnOutcome::failed);
    check_single_terminal_sequence(observer->events);
}

TEST(agent, invalid_tool_call_does_not_emit_tool_start)
{
    MockHttpServer server;
    server.enqueue(response(tool_delta(0, "", "alpha", nlohmann::json::object()) + done()));
    auto tools = make_tools();
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);

    CHECK_THROWS_MSG(agent.send("question"), "empty id or name");
    CHECK_EQ(count_type(observer->events, EventType::ToolStarted), std::size_t(0));
    CHECK(std::get<agent::ui::RequestFailed>(observer->events[3].payload).category
          == agent::ui::RequestFailureCategory::protocol);
    check_single_terminal_sequence(observer->events);
}

TEST(agent, http_error_emits_one_failed_request_and_turn)
{
    MockHttpServer server;
    server.enqueue(response("server failure body", 500));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);

    CHECK_THROWS_MSG(agent.send("question"), "status 500");
    CHECK(std::get<agent::ui::HttpRequestFinished>(observer->events[2].payload).outcome
          == agent::ui::HttpRequestOutcome::failed);
    CHECK(std::get<agent::ui::RequestFailed>(observer->events[3].payload).category
          == agent::ui::RequestFailureCategory::http);
    CHECK_EQ(count_type(observer->events, EventType::RequestFailed), std::size_t(1));
    check_single_terminal_sequence(observer->events);
}

TEST(agent, reasoning_is_counted_but_never_exposed_and_sse_metrics_are_exact)
{
    MockHttpServer server;
    const auto reasoning_one = sse({{"choices", nlohmann::json::array({
        {{"index", 0}, {"delta", {{"reasoning_content", "PRIVATE_REASONING_SENTINEL"}}}}})}});
    const auto reasoning_two = sse({{"choices", nlohmann::json::array({
        {{"index", 0}, {"delta", {{"reasoning", "SECOND_PRIVATE_SENTINEL"}}}}})}});
    const auto visible = text_delta("visible answer", "stop");
    const auto body = reasoning_one + reasoning_two + visible + done();
    server.enqueue(response(body));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);
    std::string streamed;
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [&streamed](const std::string& value) { streamed += value; };

    CHECK_EQ(agent.send("question", callbacks), std::string("visible answer"));
    CHECK_EQ(streamed, std::string("visible answer"));
    const auto finished = std::find_if(observer->events.begin(), observer->events.end(), [](const auto& event) {
        return event.type() == EventType::HttpRequestFinished;
    });
    CHECK(finished != observer->events.end());
    if (finished != observer->events.end()) {
        const auto& metrics = std::get<agent::ui::HttpRequestFinished>(finished->payload);
        CHECK_EQ(metrics.response_bytes, static_cast<std::uint64_t>(body.size()));
        CHECK_EQ(metrics.sse_data_frames, std::uint64_t(4));
        CHECK_EQ(metrics.reasoning_frames, std::uint64_t(2));
        CHECK(!metrics.counter_overflowed);
    }
    for (const auto& event : observer->events) {
        if (const auto* delta = std::get_if<agent::ui::TextDelta>(&event.payload)) {
            CHECK(delta->text.find("PRIVATE_REASONING") == std::string::npos);
            CHECK(delta->text.find("SECOND_PRIVATE") == std::string::npos);
        }
    }
    CHECK_EQ(count_type(observer->events, EventType::TextDelta), std::size_t(1));
    agent::ui::MetricCollector collector;
    for (const auto& event : observer->events) collector.consume(event);
    CHECK_EQ(collector.snapshot().text_fragments, std::uint64_t(1));
    CHECK_EQ(collector.snapshot().sse_data_frames, std::uint64_t(4));
    CHECK_EQ(collector.snapshot().reasoning_frames, std::uint64_t(2));
    CHECK(std::get<agent::ui::TurnFinished>(observer->events.back().payload).outcome
          == agent::ui::TurnOutcome::completed);
    check_single_terminal_sequence(observer->events);
}

TEST(agent, timeout_emits_failed_http_and_request)
{
    MockHttpServer server;
    auto slow = response(std::string(20, 'x'));
    slow.piece_size = 1;
    slow.piece_delay_us = 150000;
    server.enqueue(std::move(slow));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server, 1), observer);

    CHECK_THROWS_MSG(agent.send("question"), "timed out");
    CHECK(std::get<agent::ui::HttpRequestFinished>(observer->events[2].payload).outcome
          == agent::ui::HttpRequestOutcome::failed);
    CHECK(std::get<agent::ui::RequestFailed>(observer->events[3].payload).category
          == agent::ui::RequestFailureCategory::http);
    check_single_terminal_sequence(observer->events);
}

TEST(agent, iteration_limit_has_five_correlated_http_attempts)
{
    MockHttpServer server;
    const auto call = tool_delta(0, "call-id", "unknown", nlohmann::json::object()) + done();
    for (int i = 0; i < 5; ++i) {
        server.enqueue(response(call));
    }
    auto tools = make_tools();
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);

    CHECK_THROWS_MSG(agent.send("question"), "maximum iterations");
    CHECK_EQ(count_type(observer->events, EventType::HttpRequestStarted), std::size_t(5));
    CHECK_EQ(count_type(observer->events, EventType::HttpRequestFinished), std::size_t(5));
    CHECK(std::get<agent::ui::RequestFailed>(observer->events[observer->events.size() - 2].payload).category
          == agent::ui::RequestFailureCategory::iteration_limit);
    CHECK(std::get<agent::ui::TurnFinished>(observer->events.back().payload).outcome == agent::ui::TurnOutcome::failed);
    check_single_terminal_sequence(observer->events);
}

TEST(agent, observer_failure_isolated_disabled_and_diagnosed_once)
{
    MockHttpServer server;
    server.enqueue(response(text_delta("still works") + done()));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    observer->throw_on = EventType::TextDelta;
    observer->throw_from_diagnostic = true;
    Agent agent(tools, config_for(server), observer);

    CHECK_EQ(agent.send("question"), std::string("still works"));
    CHECK_EQ(observer->diagnostics, 1);
    CHECK_EQ(count_type(observer->events, EventType::RequestStarted), std::size_t(1));
    CHECK_EQ(count_type(observer->events, EventType::HttpRequestStarted), std::size_t(1));
    CHECK_EQ(count_type(observer->events, EventType::RequestFailed), std::size_t(0));
    CHECK_EQ(count_type(observer->events, EventType::TurnFinished), std::size_t(0));
    const auto observed_before_second_send = observer->events.size();
    server.enqueue(response(text_delta("still works twice") + done()));
    CHECK_EQ(agent.send("question again"), std::string("still works twice"));
    CHECK_EQ(observer->diagnostics, 1);
    CHECK_EQ(observer->events.size(), observed_before_second_send);
}

TEST(agent, observer_object_lifetime_is_owned_by_agent)
{
    MockHttpServer server;
    server.enqueue(response(text_delta("owned") + done()));
    ToolRegistry tools;
    std::weak_ptr<RecordingObserver> weak_observer;
    {
        auto observer = std::make_shared<RecordingObserver>();
        weak_observer = observer;
        Agent agent(tools, config_for(server), observer);
        observer.reset();
        CHECK(!weak_observer.expired());
        CHECK_EQ(agent.send("question"), std::string("owned"));
    }
    CHECK(weak_observer.expired());
}

TEST(agent, event_payloads_and_metadata_do_not_leak_prompt_arguments_or_tool_result)
{
    constexpr auto prompt_secret = "PROMPT_SECRET_812";
    constexpr auto argument_secret = "ARGUMENT_SECRET_934";
    constexpr auto result_secret = "RESULT_SECRET_271";

    MockHttpServer server;
    server.enqueue(response(tool_delta(0, "call-safe", "alpha", {{"value", argument_secret}}) + done()));
    server.enqueue(response(text_delta("answer") + done()));
    ToolRegistry tools;
    tools.register_tool(Tool{"alpha", "test", nlohmann::json::object(), [](nlohmann::json) {
                                 return std::string(result_secret);
                             }});
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);
    CHECK_EQ(agent.send(prompt_secret), std::string("answer"));

    for (const auto& event : observer->events) {
        if (event.metadata.tool_call_id) {
            CHECK(event.metadata.tool_call_id->value != prompt_secret);
            CHECK(event.metadata.tool_call_id->value != argument_secret);
            CHECK(event.metadata.tool_call_id->value != result_secret);
        }
        if (const auto* text = std::get_if<agent::ui::TextDelta>(&event.payload)) {
            CHECK(text->text.find(prompt_secret) == std::string::npos);
            CHECK(text->text.find(argument_secret) == std::string::npos);
            CHECK(text->text.find(result_secret) == std::string::npos);
        }
        if (const auto* started = std::get_if<agent::ui::ToolStarted>(&event.payload)) {
            CHECK(started->name.find(prompt_secret) == std::string::npos);
            CHECK(started->name.find(argument_secret) == std::string::npos);
            CHECK(started->name.find(result_secret) == std::string::npos);
        }
        if (const auto* succeeded = std::get_if<agent::ui::ToolSucceeded>(&event.payload)) {
            CHECK(succeeded->name.find(prompt_secret) == std::string::npos);
            CHECK(succeeded->name.find(argument_secret) == std::string::npos);
            CHECK(succeeded->name.find(result_secret) == std::string::npos);
        }
    }
}

TEST(agent, legacy_stream_callback_exception_keeps_type_and_message)
{
    MockHttpServer server;
    auto streamed = response(text_delta("piece") + done());
    streamed.piece_size = 1;
    streamed.piece_delay_us = 1000;
    server.enqueue(std::move(streamed));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [](const std::string&) { throw LegacyCallbackError("legacy marker"); };

    bool caught_original_type = false;
    try {
        (void)agent.send("question", callbacks);
    } catch (const LegacyCallbackError& error) {
        caught_original_type = true;
        CHECK_EQ(std::string(error.what()), std::string("legacy marker"));
    } catch (...) {
        CHECK(false);
    }
    CHECK(caught_original_type);
    CHECK(std::get<agent::ui::HttpRequestFinished>(observer->events[3].payload).outcome
          == agent::ui::HttpRequestOutcome::legacy_callback_aborted);
    CHECK(std::get<agent::ui::RequestFailed>(observer->events[4].payload).category
          == agent::ui::RequestFailureCategory::legacy_callback);
    CHECK(std::get<agent::ui::TurnFinished>(observer->events[5].payload).outcome == agent::ui::TurnOutcome::failed);
    check_single_terminal_sequence(observer->events);
}

TEST(agent, legacy_callback_exception_from_final_sse_flush_still_finishes_http_event)
{
    MockHttpServer server;
    auto body = text_delta("last piece");
    body.resize(body.size() - 2); // Final data frame has no blank-line terminator; delivered by flush().
    server.enqueue(response(body));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [](const std::string&) { throw LegacyCallbackError("flush marker"); };

    bool caught_original_type = false;
    try {
        (void)agent.send("question", callbacks);
    } catch (const LegacyCallbackError& error) {
        caught_original_type = true;
        CHECK_EQ(std::string(error.what()), std::string("flush marker"));
    } catch (...) {
        CHECK(false);
    }
    CHECK(caught_original_type);
    CHECK_EQ(count_type(observer->events, EventType::HttpRequestStarted), std::size_t(1));
    CHECK_EQ(count_type(observer->events, EventType::HttpRequestFinished), std::size_t(1));
    CHECK(std::get<agent::ui::HttpRequestFinished>(observer->events[3].payload).outcome
          == agent::ui::HttpRequestOutcome::legacy_callback_aborted);
    CHECK(std::get<agent::ui::RequestFailed>(observer->events[4].payload).category
          == agent::ui::RequestFailureCategory::legacy_callback);
    CHECK(std::get<agent::ui::TurnFinished>(observer->events[5].payload).outcome == agent::ui::TurnOutcome::failed);
    check_single_terminal_sequence(observer->events);
}

TEST(agent, legacy_agent_error_callback_is_not_misclassified_as_protocol_error)
{
    MockHttpServer server;
    server.enqueue(response(text_delta("delta") + done()));
    ToolRegistry tools;
    auto observer = std::make_shared<RecordingObserver>();
    Agent agent(tools, config_for(server), observer);
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [](const std::string&) { throw AgentError("legacy agent error"); };

    bool preserved = false;
    try {
        (void)agent.send("question", callbacks);
    } catch (const AgentError& error) {
        preserved = true;
        CHECK_EQ(std::string(error.what()), std::string("legacy agent error"));
    }
    CHECK(preserved);
    bool classified_as_legacy = false;
    for (const auto& event : observer->events) {
        if (event.type() == EventType::RequestFailed) {
            classified_as_legacy = std::get<agent::ui::RequestFailed>(event.payload).category
                == agent::ui::RequestFailureCategory::legacy_callback;
        }
    }
    CHECK(classified_as_legacy);
}

TEST(agent, legacy_two_argument_constructor_remains_source_compatible)
{
    MockHttpServer server;
    server.enqueue(response(text_delta("ok") + done()));
    ToolRegistry tools;
    Agent agent(tools, config_for(server));
    CHECK_EQ(agent.send("question"), std::string("ok"));
}
