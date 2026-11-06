#include "agent.hpp"
#include "http_client.hpp"
#include "mock_http_server.hpp"
#include "test_support.hpp"
#include "tool_registry.hpp"
#include "ui/agent_worker.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using namespace agent::ui;

std::string sse(const nlohmann::json& value)
{
    return "data: " + value.dump() + "\n\n";
}

std::string done()
{
    return "data: [DONE]\n\n";
}

std::string delta(const std::string& text)
{
    return sse({{"choices", nlohmann::json::array({{{"index", 0}, {"delta", {{"content", text}}}}})}});
}

std::string tool_call(const std::string& id, const std::string& name, const nlohmann::json& args = {})
{
    const nlohmann::json call{{"index", 0},
                              {"id", id},
                              {"type", "function"},
                              {"function", {{"name", name}, {"arguments", args.dump()}}}};
    return sse({{"choices", nlohmann::json::array({{{"index", 0},
                                                     {"delta", {{"tool_calls", nlohmann::json::array({call})}}}}})}});
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

std::vector<AgentEvent> drain(const std::shared_ptr<EventChannel>& channel)
{
    std::vector<AgentEvent> result;
    AgentEvent event;
    while (channel->try_pop(event)) {
        result.push_back(std::move(event));
    }
    return result;
}

std::size_t count_type(const std::vector<AgentEvent>& events, EventType type)
{
    return static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [type](const auto& event) {
        return event.type() == type;
    }));
}

std::string text_of(const std::vector<AgentEvent>& events)
{
    std::string result;
    for (const auto& event : events) {
        if (const auto* text = std::get_if<TextDelta>(&event.payload)) {
            result += text->text;
        }
    }
    return result;
}

void check_sequences(const std::vector<AgentEvent>& events)
{
    for (std::size_t index = 1; index < events.size(); ++index) {
        CHECK(events[index].metadata.sequence > events[index - 1].metadata.sequence);
        CHECK(events[index].metadata.session_id == events.front().metadata.session_id);
    }
}

class ThrowOnTextObserver final : public EventObserver {
public:
    ThrowOnTextObserver(std::shared_ptr<EventObserver> inner, std::shared_ptr<int> diagnostics)
        : inner_(std::move(inner))
        , diagnostics_(std::move(diagnostics))
    {
    }

    void on_event(const AgentEvent& event) override
    {
        if (event.type() == EventType::TextDelta) {
            throw std::runtime_error("synthetic bridge observer failure");
        }
        inner_->on_event(event);
    }

    void on_observer_failure() override
    {
        ++*diagnostics_;
        inner_->on_observer_failure();
    }

private:
    std::shared_ptr<EventObserver> inner_;
    std::shared_ptr<int> diagnostics_;
};

AgentFactory make_agent_factory(const MockHttpServer& server,
                                std::shared_ptr<ToolRegistry> tools,
                                std::shared_ptr<int> observer_diagnostics = {})
{
    const auto config = config_for(server);
    return [tools = std::move(tools), config, observer_diagnostics = std::move(observer_diagnostics)](
               std::shared_ptr<EventObserver> observer) {
        if (observer_diagnostics) {
            observer = std::make_shared<ThrowOnTextObserver>(std::move(observer), observer_diagnostics);
        }
        return std::make_unique<Agent>(*tools, config, std::move(observer));
    };
}

std::shared_ptr<ToolRegistry> empty_tools()
{
    return std::make_shared<ToolRegistry>();
}

} // namespace

TEST(tui2, real_agent_success_streams_ordered_deltas_and_returns_exact_response)
{
    MockHttpServer server;
    server.enqueue(response(delta("hello ") + delta("from ") + delta("agent") + done()));
    AgentWorker worker(make_agent_factory(server, empty_tools()));

    auto completion = worker.submit("say hello").get();
    const auto events = drain(worker.event_channel());

    CHECK(completion.status == TurnStatus::returned);
    CHECK(completion.response.has_value());
    CHECK_EQ(completion.response.value_or(""), std::string("hello from agent"));
    CHECK(!completion.event_details_incomplete());
    CHECK(completion.session_id.has_value());
    CHECK(completion.turn_id == std::optional<TurnId>{TurnId{1}});
    CHECK_EQ(text_of(events), *completion.response);
    CHECK_EQ(count_type(events, EventType::TextDelta), std::size_t(3));
    CHECK_EQ(count_type(events, EventType::TurnFinished), std::size_t(1));
    CHECK(std::get<TurnFinished>(events.back().payload).outcome == TurnOutcome::completed);
    check_sequences(events);
}

TEST(tui2, real_agent_http_failure_is_preserved_in_future)
{
    MockHttpServer server;
    server.enqueue(response("mock failure", 500));
    AgentWorker worker(make_agent_factory(server, empty_tools()));

    auto completion = worker.submit("trigger HTTP error").get();
    const auto events = drain(worker.event_channel());
    bool original_http_error = false;
    try {
        std::rethrow_exception(completion.exception);
    } catch (const HttpError& error) {
        original_http_error = std::string(error.what()).find("status 500") != std::string::npos;
    }

    CHECK(completion.status == TurnStatus::exception);
    CHECK(!completion.response.has_value());
    CHECK(original_http_error);
    CHECK_EQ(count_type(events, EventType::RequestFailed), std::size_t(1));
    CHECK(std::get<TurnFinished>(events.back().payload).outcome == TurnOutcome::failed);
}

TEST(tui2, real_agent_incomplete_sse_keeps_partial_text_nonfinal)
{
    MockHttpServer server;
    server.enqueue(response(delta("partial without done")));
    AgentWorker worker(make_agent_factory(server, empty_tools()));

    auto completion = worker.submit("trigger missing DONE").get();
    const auto events = drain(worker.event_channel());
    bool original_agent_error = false;
    try {
        std::rethrow_exception(completion.exception);
    } catch (const AgentError& error) {
        original_agent_error = std::string(error.what()).find("missing [DONE]") != std::string::npos;
    }

    CHECK(completion.status == TurnStatus::exception);
    CHECK(!completion.response.has_value());
    CHECK(original_agent_error);
    CHECK_EQ(text_of(events), std::string("partial without done"));
    CHECK(std::get<TurnFinished>(events.back().payload).outcome == TurnOutcome::failed);
}

TEST(tui2, real_agent_tool_success_and_failure_can_be_followed_by_final_response)
{
    MockHttpServer server;
    server.enqueue(response(tool_call("call-ok", "ok_tool") + done()));
    server.enqueue(response(tool_call("call-fail", "fail_tool") + done()));
    server.enqueue(response(delta("recovered after tool failure") + done()));

    auto tools = std::make_shared<ToolRegistry>();
    const auto schema = nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()}};
    tools->register_tool(Tool{"ok_tool", "test success", schema, [](nlohmann::json) { return "tool result"; }});
    tools->register_tool(Tool{"fail_tool", "test failure", schema, [](nlohmann::json) -> std::string {
                                   throw std::runtime_error("expected handler failure");
                               }});
    AgentWorker worker(make_agent_factory(server, std::move(tools)));

    auto completion = worker.submit("use both tools").get();
    const auto events = drain(worker.event_channel());

    CHECK(completion.status == TurnStatus::returned);
    CHECK_EQ(completion.response.value_or(""), std::string("recovered after tool failure"));
    CHECK_EQ(count_type(events, EventType::ToolSucceeded), std::size_t(1));
    CHECK_EQ(count_type(events, EventType::ToolFailed), std::size_t(1));
    CHECK_EQ(count_type(events, EventType::HttpRequestStarted), std::size_t(3));
    CHECK_EQ(count_type(events, EventType::RequestFailed), std::size_t(0));
    const auto failed = std::find_if(events.begin(), events.end(), [](const auto& event) {
        return event.type() == EventType::ToolFailed;
    });
    CHECK(failed != events.end());
    if (failed != events.end()) {
        CHECK(failed->metadata.tool_call_id == std::optional<ToolCallId>{ToolCallId{"call-fail"}});
    }
    CHECK(std::get<TurnFinished>(events.back().payload).outcome == TurnOutcome::completed);
    check_sequences(events);
}

TEST(tui2, real_agent_nonstandard_tool_exception_keeps_dynamic_type)
{
    MockHttpServer server;
    server.enqueue(response(tool_call("call-int", "throw_int") + done()));
    auto tools = std::make_shared<ToolRegistry>();
    tools->register_tool(Tool{"throw_int", "test non-standard exception", nlohmann::json::object(),
                              [](nlohmann::json) -> std::string { throw 73; }});
    AgentWorker worker(make_agent_factory(server, std::move(tools)));

    auto completion = worker.submit("trigger non-standard exception").get();
    const auto events = drain(worker.event_channel());
    bool original_value = false;
    try {
        std::rethrow_exception(completion.exception);
    } catch (int value) {
        original_value = value == 73;
    }

    CHECK(completion.status == TurnStatus::exception);
    CHECK(original_value);
    CHECK_EQ(count_type(events, EventType::ToolFailed), std::size_t(1));
    CHECK(std::get<TurnFinished>(events.back().payload).outcome == TurnOutcome::failed);
}

TEST(tui2, observer_failure_marks_turn_details_incomplete_and_final_response_recovers_text)
{
    MockHttpServer server;
    server.enqueue(response(delta("complete final answer") + done()));
    auto diagnostics = std::make_shared<int>(0);
    AgentWorker worker(make_agent_factory(server, empty_tools(), diagnostics));

    auto completion = worker.submit("observer will fail").get();
    const auto events = drain(worker.event_channel());

    CHECK(completion.status == TurnStatus::returned);
    CHECK_EQ(completion.response.value_or(""), std::string("complete final answer"));
    CHECK_EQ(completion.observer_failures, std::uint64_t(1));
    CHECK(completion.event_details_incomplete());
    CHECK_EQ(*diagnostics, 1);
    CHECK(text_of(events).empty());
}

TEST(tui2, queue_saturation_reports_loss_and_final_agent_response_can_replace_partial_text)
{
    MockHttpServer server;
    const std::string body = delta("alpha") + delta("beta") + delta("gamma") + done();
    server.enqueue(response(body));
    EventQueueLimits limits{4, 4096, 1024};
    AgentWorker worker(make_agent_factory(server, empty_tools()), limits);

    auto future = worker.submit("saturate event channel");
    CHECK(future.wait_for(2s) == std::future_status::ready);
    auto completion = future.get();
    const auto events = drain(worker.event_channel());
    const auto partial = text_of(events);

    CHECK(completion.status == TurnStatus::returned);
    CHECK_EQ(completion.response.value_or(""), std::string("alphabetagamma"));
    CHECK(completion.dropped_events > 0);
    CHECK(completion.event_details_incomplete());
    CHECK(events.size() <= limits.max_events);
    CHECK_EQ(completion.dropped_events, std::uint64_t(3));
    CHECK_EQ(count_type(events, EventType::TurnFinished), std::size_t(0));
    CHECK(partial != *completion.response);
    CHECK(std::get<TextDelta>(events.back().payload).text == "beta");
    // A consumer must replace the partial assistant turn with response; appending
    // response to partial would duplicate content and is not the recovery policy.
}

TEST(tui2, two_real_agent_turns_keep_ids_and_per_turn_loss_counts_separate)
{
    MockHttpServer server;
    server.enqueue(response(delta("first") + done()));
    server.enqueue(response(delta("second") + done()));
    AgentWorker worker(make_agent_factory(server, empty_tools()));

    auto first = worker.submit("one").get();
    const auto first_events = drain(worker.event_channel());
    auto second = worker.submit("two").get();
    const auto second_events = drain(worker.event_channel());

    CHECK_EQ(first.response.value_or(""), std::string("first"));
    CHECK_EQ(second.response.value_or(""), std::string("second"));
    CHECK(first.session_id == second.session_id);
    CHECK(first.turn_id == std::optional<TurnId>{TurnId{1}});
    CHECK(second.turn_id == std::optional<TurnId>{TurnId{2}});
    CHECK_EQ(first.dropped_events, std::uint64_t(0));
    CHECK_EQ(second.dropped_events, std::uint64_t(0));
    CHECK(first_events.back().metadata.turn_id == first.turn_id);
    CHECK(second_events.back().metadata.turn_id == second.turn_id);
}

TEST(tui2, close_during_real_agent_tool_waits_for_send_then_rejects_new_turns)
{
    MockHttpServer server;
    server.enqueue(response(tool_call("call-wait", "wait_tool") + done()));
    server.enqueue(response(delta("finished after close request") + done()));
    auto started = std::make_shared<std::promise<void>>();
    auto started_future = started->get_future();
    auto release = std::make_shared<std::promise<void>>();
    auto release_future = release->get_future().share();
    auto tools = std::make_shared<ToolRegistry>();
    tools->register_tool(Tool{"wait_tool", "blocks under test", nlohmann::json::object(),
                              [started, release_future](nlohmann::json) {
                                  started->set_value();
                                  release_future.wait();
                                  return std::string("waited");
                              }});
    AgentWorker worker(make_agent_factory(server, std::move(tools)));

    auto completion_future = worker.submit("close while tool runs");
    const bool handler_started = started_future.wait_for(3s) == std::future_status::ready;
    worker.request_close();
    bool submit_rejected = false;
    try {
        static_cast<void>(worker.submit("must be rejected"));
    } catch (const std::logic_error&) {
        submit_rejected = true;
    }
    const bool still_active = completion_future.wait_for(0ms) == std::future_status::timeout;
    release->set_value();
    const bool completed = completion_future.wait_for(5s) == std::future_status::ready;
    std::optional<TurnCompletion> completion;
    if (completed) {
        completion = completion_future.get();
    }
    worker.join();

    CHECK(handler_started);
    CHECK(submit_rejected);
    CHECK(still_active);
    CHECK(completed);
    CHECK(completion.has_value());
    if (completion) {
        CHECK(completion->status == TurnStatus::returned);
        CHECK_EQ(completion->response.value_or(""), std::string("finished after close request"));
    }
}

TEST(tui2, join_without_active_turn_closes_worker_and_rejects_future_submissions)
{
    MockHttpServer server;
    AgentWorker worker(make_agent_factory(server, empty_tools()));
    worker.join();

    bool rejected = false;
    try {
        static_cast<void>(worker.submit("after join"));
    } catch (const std::logic_error&) {
        rejected = true;
    }
    CHECK(rejected);
}

TEST(tui2, self_join_from_tool_handler_is_rejected_without_stopping_worker)
{
    MockHttpServer server;
    server.enqueue(response(tool_call("call-self-join", "self_join") + done()));
    server.enqueue(response(delta("continued after rejected self join") + done()));

    auto worker_ref = std::make_shared<AgentWorker*>(nullptr);
    auto tools = std::make_shared<ToolRegistry>();
    tools->register_tool(Tool{"self_join", "attempts self-join", nlohmann::json::object(),
                              [worker_ref](nlohmann::json) {
                                  (*worker_ref)->join();
                                  return std::string("unexpected");
                              }});
    AgentWorker worker(make_agent_factory(server, std::move(tools)));
    *worker_ref = &worker;

    const auto completion = worker.submit("attempt self join").get();
    const auto events = drain(worker.event_channel());
    worker.join();

    CHECK(completion.status == TurnStatus::returned);
    CHECK_EQ(completion.response.value_or(""), std::string("continued after rejected self join"));
    CHECK_EQ(count_type(events, EventType::ToolFailed), std::size_t(1));
    CHECK(std::get<TurnFinished>(events.back().payload).outcome == TurnOutcome::completed);
}
