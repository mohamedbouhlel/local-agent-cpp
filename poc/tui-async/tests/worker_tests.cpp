#include "tui_async/agent_worker.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using namespace agent::ui;
using namespace tui_async;

int assertions = 0;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        ++assertions;                                                                                                  \
        if (!(condition)) {                                                                                            \
            throw std::runtime_error(std::string("check failed: ") + #condition + " at line " + std::to_string(__LINE__)); \
        }                                                                                                              \
    } while (false)

AgentEvent event(std::uint64_t sequence, std::uint64_t turn, EventPayload payload)
{
    EventMetadata metadata;
    metadata.session_id = SessionId{7};
    metadata.turn_id = TurnId{turn};
    metadata.sequence = sequence;
    metadata.timestamp_utc = std::chrono::system_clock::now();
    metadata.elapsed = std::chrono::steady_clock::duration::zero();
    return {std::move(metadata), std::move(payload)};
}

bool is_type(const AgentEvent& value, EventType type)
{
    switch (type) {
    case EventType::RequestStarted: return std::holds_alternative<RequestStarted>(value.payload);
    case EventType::HttpRequestStarted: return std::holds_alternative<HttpRequestStarted>(value.payload);
    case EventType::TextDelta: return std::holds_alternative<TextDelta>(value.payload);
    case EventType::ToolStarted: return std::holds_alternative<ToolStarted>(value.payload);
    case EventType::ToolSucceeded: return std::holds_alternative<ToolSucceeded>(value.payload);
    case EventType::ToolFailed: return std::holds_alternative<ToolFailed>(value.payload);
    case EventType::HttpRequestFinished: return std::holds_alternative<HttpRequestFinished>(value.payload);
    case EventType::RequestFailed: return std::holds_alternative<RequestFailed>(value.payload);
    case EventType::TurnFinished: return std::holds_alternative<TurnFinished>(value.payload);
    }
    return false;
}

class LambdaExecutor final : public TurnExecutor {
public:
    using Operation = std::function<void(const std::string&)>;
    explicit LambdaExecutor(Operation operation) : operation_(std::move(operation)) {}
    void send(const std::string& input) override { operation_(input); }

private:
    Operation operation_;
};

class DestructionProbeExecutor final : public TurnExecutor {
public:
    explicit DestructionProbeExecutor(std::shared_ptr<std::promise<std::thread::id>> destroyed)
        : destroyed_(std::move(destroyed))
    {
    }
    void send(const std::string&) override {}
    ~DestructionProbeExecutor() override { destroyed_->set_value(std::this_thread::get_id()); }

private:
    std::shared_ptr<std::promise<std::thread::id>> destroyed_;
};

ExecutorFactory factory_for(std::function<void(const std::string&, const std::shared_ptr<EventObserver>&)> operation)
{
    return [operation = std::move(operation)](std::shared_ptr<EventObserver> observer) {
        return std::make_unique<LambdaExecutor>([operation, observer = std::move(observer)](const std::string& input) {
            operation(input, observer);
        });
    };
}

class AgentStylePublisher {
public:
    explicit AgentStylePublisher(std::shared_ptr<EventObserver> observer) : observer_(std::move(observer)) {}

    void publish(const AgentEvent& value)
    {
        if (!enabled_) {
            return;
        }
        try {
            observer_->on_event(value);
        } catch (...) {
            enabled_ = false;
            try {
                observer_->on_observer_failure();
            } catch (...) {
            }
        }
    }

private:
    std::shared_ptr<EventObserver> observer_;
    bool enabled_ = true;
};

class ThrowOnTextObserver final : public EventObserver {
public:
    explicit ThrowOnTextObserver(std::shared_ptr<EventObserver> inner) : inner_(std::move(inner)) {}

    void on_event(const AgentEvent& value) override
    {
        if (is_type(value, EventType::TextDelta)) {
            throw std::runtime_error("synthetic observer failure");
        }
        inner_->on_event(value);
    }
    void on_observer_failure() override
    {
        ++diagnostics;
        inner_->on_observer_failure();
    }

    int diagnostics = 0;

private:
    std::shared_ptr<EventObserver> inner_;
};

void drain(const std::shared_ptr<EventChannel>& channel, std::vector<AgentEvent>& out)
{
    AgentEvent value;
    while (channel->try_pop(value)) {
        out.push_back(std::move(value));
    }
}

void test_ordered_owned_events_and_worker_reuse()
{
    struct ScriptState {
        std::uint64_t sequence = 1;
        std::uint64_t turn = 0;
    };
    AgentWorker worker(factory_for([state = std::make_shared<ScriptState>()](const std::string&,
                                                                              const std::shared_ptr<EventObserver>& observer) {
        const auto turn = ++state->turn;
        AgentStylePublisher publisher(observer);
        publisher.publish(event(state->sequence++, turn, RequestStarted{}));
        publisher.publish(event(state->sequence++, turn, HttpRequestStarted{1}));
        publisher.publish(event(state->sequence++, turn, TextDelta{"alpha"}));
        publisher.publish(event(state->sequence++, turn, TextDelta{"beta"}));
        publisher.publish(event(state->sequence++, turn, TurnFinished{TurnOutcome::completed, 1, 0, 0}));
    }));

    std::vector<AgentEvent> received;
    auto first = worker.submit("turn one");
    auto first_result = first.get();
    drain(worker.event_channel(), received);
    auto second = worker.submit("turn two");
    auto second_result = second.get();
    drain(worker.event_channel(), received);
    worker.request_close();
    worker.join();

    CHECK(first_result.status == TurnStatus::returned);
    CHECK(second_result.status == TurnStatus::returned);
    CHECK(first_result.task_id == 1 && second_result.task_id == 2);
    CHECK(received.size() == 10);
    for (std::size_t i = 0; i < received.size(); ++i) {
        CHECK(received[i].metadata.sequence == i + 1);
    }
    CHECK(std::get<TextDelta>(received[2].payload).text == "alpha");
    CHECK(std::get<TextDelta>(received[3].payload).text == "beta");
    CHECK(received[0].metadata.turn_id == TurnId{1});
    CHECK(received[5].metadata.turn_id == TurnId{2});
    CHECK(!first_result.event_details_incomplete() && !second_result.event_details_incomplete());
}

class SyntheticFailure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void test_saturation_is_reported_and_terminal_outcome_survives()
{
    EventQueueLimits limits{3, 64, 32};
    auto calls = std::make_shared<std::uint64_t>(0);
    AgentWorker worker(factory_for([calls](const std::string&, const std::shared_ptr<EventObserver>& observer) {
        const auto turn = ++*calls;
        AgentStylePublisher publisher(observer);
        if (turn == 1) {
            publisher.publish(event(1, turn, RequestStarted{}));
            for (std::uint64_t sequence = 2; sequence < 1002; ++sequence) {
                publisher.publish(event(sequence, turn, TextDelta{"x"}));
            }
            publisher.publish(event(1002, turn, RequestFailed{RequestFailureCategory::http}));
            publisher.publish(event(1003, turn, TurnFinished{TurnOutcome::failed, 1, 0, 0}));
            throw SyntheticFailure("synthetic transport failure");
        }
        publisher.publish(event(1004, turn, RequestStarted{}));
        publisher.publish(event(1005, turn, TurnFinished{TurnOutcome::completed, 0, 0, 0}));
    }), limits);

    auto future = worker.submit("fill without a consumer");
    const auto ready = future.wait_for(1s);
    CHECK(ready == std::future_status::ready);
    auto result = future.get();
    CHECK(result.status == TurnStatus::exception);
    CHECK(result.event_details_incomplete());
    CHECK(result.dropped_events == 1000);
    bool exact_exception = false;
    try {
        std::rethrow_exception(result.exception);
    } catch (const SyntheticFailure& error) {
        exact_exception = std::string(error.what()) == "synthetic transport failure";
    }
    CHECK(exact_exception);

    std::vector<AgentEvent> received;
    drain(worker.event_channel(), received);
    CHECK(received.size() == 3);
    CHECK(is_type(received.front(), EventType::RequestStarted));
    CHECK(std::none_of(received.begin(), received.end(), [](const auto& item) {
        return is_type(item, EventType::RequestFailed) || is_type(item, EventType::TurnFinished);
    }));
    // Even though both terminal events were dropped, completion plus the
    // explicit loss count remains authoritative and cannot be mistaken for success.

    auto recovered = worker.submit("next turn after drain").get();
    std::vector<AgentEvent> recovered_events;
    drain(worker.event_channel(), recovered_events);
    CHECK(recovered.status == TurnStatus::returned);
    CHECK(recovered.dropped_events == 0);
    CHECK(!recovered.event_details_incomplete());
    CHECK(recovered_events.size() == 2);
    CHECK(recovered_events[0].metadata.turn_id == TurnId{2});
    worker.request_close();
    worker.join();
}

void test_nonstandard_exception_is_preserved_and_worker_reused()
{
    auto calls = std::make_shared<int>(0);
    AgentWorker worker(factory_for([calls](const std::string&, const std::shared_ptr<EventObserver>&) {
        if (++*calls == 1) {
            throw 73;
        }
    }));

    auto failed = worker.submit("throw non-standard exception").get();
    bool original_int_preserved = false;
    try {
        std::rethrow_exception(failed.exception);
    } catch (int value) {
        original_int_preserved = value == 73;
    }
    auto recovered = worker.submit("reuse after exception").get();
    worker.request_close();
    worker.join();

    CHECK(failed.status == TurnStatus::exception);
    CHECK(original_int_preserved);
    CHECK(recovered.status == TurnStatus::returned);
}

void test_large_callback_payload_is_rejected_without_copying_or_waiting()
{
    EventQueueLimits limits{8, 64, 16};
    auto channel = std::make_shared<EventChannel>(limits);
    AgentEvent too_large = event(1, 1, TextDelta{std::string(4 * 1024 * 1024, 'z')});
    const auto start = std::chrono::steady_clock::now();
    channel->on_event(too_large);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(elapsed < 250ms);
    CHECK(channel->queued_events() == 0);
    CHECK(channel->snapshot().dropped_events == 1);
    CHECK(channel->snapshot().details_incomplete);
}

void test_observer_exception_isolated_and_diagnosed_once()
{
    struct State {
        std::uint64_t sequence = 1;
        std::shared_ptr<ThrowOnTextObserver> throwing;
    };
    auto state = std::make_shared<State>();
    AgentWorker worker([state](std::shared_ptr<EventObserver> bridge) {
        state->throwing = std::make_shared<ThrowOnTextObserver>(std::move(bridge));
        return std::make_unique<LambdaExecutor>([state](const std::string&) {
            AgentStylePublisher publisher(state->throwing);
            publisher.publish(event(state->sequence++, 1, RequestStarted{}));
            publisher.publish(event(state->sequence++, 1, TextDelta{"throw here"}));
            publisher.publish(event(state->sequence++, 1, TurnFinished{TurnOutcome::completed, 0, 0, 0}));
        });
    });

    auto result = worker.submit("observer exception").get();
    std::vector<AgentEvent> received;
    drain(worker.event_channel(), received);
    worker.request_close();
    worker.join();

    CHECK(result.status == TurnStatus::returned);
    CHECK(result.observer_failures == 1);
    CHECK(result.event_details_incomplete());
    CHECK(state->throwing->diagnostics == 1);
    CHECK(received.size() == 1);
    CHECK(is_type(received.front(), EventType::RequestStarted));
    CHECK(worker.event_channel()->snapshot().details_incomplete);
}

void test_concurrent_consumer_and_producer_keep_accepted_order()
{
    EventQueueLimits limits{16, 1024, 32};
    auto first_published = std::make_shared<std::promise<void>>();
    auto first_published_future = first_published->get_future();
    auto first_consumed = std::make_shared<std::promise<void>>();
    auto first_consumed_future = first_consumed->get_future().share();
    AgentWorker worker(factory_for([first_published, first_consumed_future](const std::string&,
                                                            const std::shared_ptr<EventObserver>& observer) {
        AgentStylePublisher publisher(observer);
        publisher.publish(event(1, 1, RequestStarted{}));
        first_published->set_value();
        first_consumed_future.wait();
        for (std::uint64_t sequence = 2; sequence < 10002; ++sequence) {
            publisher.publish(event(sequence, 1, TextDelta{"d"}));
        }
        publisher.publish(event(10002, 1, TurnFinished{TurnOutcome::completed, 1, 0, 0}));
    }), limits);

    std::atomic<bool> completed{false};
    std::vector<std::uint64_t> received_sequences;
    auto future = worker.submit("concurrent drain");
    AgentEvent first_event;
    const auto published_in_time = first_published_future.wait_for(1s) == std::future_status::ready;
    const auto first_was_queued = published_in_time && worker.event_channel()->try_pop(first_event);
    first_consumed->set_value();
    if (first_was_queued) {
        received_sequences.push_back(first_event.metadata.sequence);
    }

    std::thread consumer([&] {
        AgentEvent value;
        auto channel = worker.event_channel();
        while (!completed.load(std::memory_order_acquire) || channel->queued_events() != 0) {
            if (channel->try_pop(value)) {
                received_sequences.push_back(value.metadata.sequence);
            } else {
                std::this_thread::yield();
            }
        }
    });
    auto result = future.get();
    completed.store(true, std::memory_order_release);
    consumer.join();
    worker.request_close();
    worker.join();

    CHECK(published_in_time);
    CHECK(first_was_queued);
    CHECK(is_type(first_event, EventType::RequestStarted));
    CHECK(first_event.metadata.sequence == 1);
    CHECK(result.status == TurnStatus::returned);
    CHECK(std::is_sorted(received_sequences.begin(), received_sequences.end()));
    CHECK(!received_sequences.empty());
    CHECK(result.event_details_incomplete() == (result.dropped_events != 0));
}

void test_close_during_active_send_waits_for_real_completion()
{
    auto started = std::make_shared<std::promise<void>>();
    auto started_future = started->get_future();
    auto release = std::make_shared<std::promise<void>>();
    auto release_future = release->get_future().share();
    AgentWorker worker(factory_for([started, release_future](const std::string&,
                                                             const std::shared_ptr<EventObserver>& observer) {
        AgentStylePublisher publisher(observer);
        publisher.publish(event(1, 1, RequestStarted{}));
        started->set_value();
        release_future.wait();
        publisher.publish(event(2, 1, TurnFinished{TurnOutcome::completed, 0, 0, 0}));
    }));

    auto completion = worker.submit("blocked until released");
    CHECK(started_future.wait_for(1s) == std::future_status::ready);
    const auto close_start = std::chrono::steady_clock::now();
    worker.request_close();
    const auto close_elapsed = std::chrono::steady_clock::now() - close_start;
    const auto still_running = completion.wait_for(0ms) == std::future_status::timeout;
    release->set_value();
    const auto finished = completion.wait_for(1s) == std::future_status::ready;
    TurnCompletion result;
    if (finished) {
        result = completion.get();
    }
    worker.join();

    CHECK(close_elapsed < 500ms);
    CHECK(still_running); // close requests shutdown; it does not cancel send
    CHECK(finished);
    CHECK(result.status == TurnStatus::returned);
}

void test_executor_is_destroyed_on_worker_before_join_returns()
{
    auto destroyed = std::make_shared<std::promise<std::thread::id>>();
    auto destroyed_future = destroyed->get_future();
    const auto caller = std::this_thread::get_id();
    AgentWorker worker([destroyed](std::shared_ptr<EventObserver>) {
        return std::make_unique<DestructionProbeExecutor>(destroyed);
    });
    worker.request_close();
    worker.join();

    CHECK(destroyed_future.wait_for(1s) == std::future_status::ready);
    CHECK(destroyed_future.get() != caller);
}

void test_rejects_second_inflight_turn_and_new_work_after_close()
{
    auto started = std::make_shared<std::promise<void>>();
    auto started_future = started->get_future();
    auto release = std::make_shared<std::promise<void>>();
    auto release_future = release->get_future().share();
    AgentWorker worker(factory_for([started, release_future](const std::string&,
                                                             const std::shared_ptr<EventObserver>&) {
        started->set_value();
        release_future.wait();
    }));
    auto first = worker.submit("first");
    const auto started_in_time = started_future.wait_for(1s) == std::future_status::ready;
    bool rejected_busy = false;
    try {
        static_cast<void>(worker.submit("second"));
    } catch (const std::logic_error&) {
        rejected_busy = true;
    }
    release->set_value();
    auto result = first.get();
    worker.request_close();
    bool rejected_closed = false;
    try {
        static_cast<void>(worker.submit("after close"));
    } catch (const std::logic_error&) {
        rejected_closed = true;
    }
    worker.join();
    CHECK(started_in_time);
    CHECK(rejected_busy);
    CHECK(rejected_closed);
    CHECK(result.status == TurnStatus::returned);
}

} // namespace

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"order and worker reuse", test_ordered_owned_events_and_worker_reuse},
        {"saturation and terminal outcome", test_saturation_is_reported_and_terminal_outcome_survives},
        {"non-standard exception and reuse", test_nonstandard_exception_is_preserved_and_worker_reused},
        {"nonblocking oversized callback", test_large_callback_payload_is_rejected_without_copying_or_waiting},
        {"observer exception", test_observer_exception_isolated_and_diagnosed_once},
        {"concurrent producer/consumer", test_concurrent_consumer_and_producer_keep_accepted_order},
        {"close during active send", test_close_during_active_send_waits_for_real_completion},
        {"executor destruction thread", test_executor_is_destroyed_on_worker_before_join_returns},
        {"single-flight admission", test_rejects_second_inflight_turn_and_new_work_after_close},
    };

    for (const auto& [name, test] : tests) {
        test();
        std::cout << "PASS " << name << '\n';
    }
    std::cout << tests.size() << " tests, " << assertions << " checks\n";
    return 0;
}
