#include "tui_async/agent_worker.hpp"

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace tui_async {
namespace {

std::size_t add_saturated(std::size_t lhs, std::size_t rhs) noexcept
{
    if (rhs > static_cast<std::size_t>(-1) - lhs) {
        return static_cast<std::size_t>(-1);
    }
    return lhs + rhs;
}

} // namespace

bool is_request_started(const agent::ui::AgentEvent& event) noexcept
{
    return std::holds_alternative<agent::ui::RequestStarted>(event.payload);
}

EventChannel::EventChannel(EventQueueLimits limits)
    : limits_(limits)
{
    if (limits_.max_events == 0) {
        throw std::invalid_argument("event queue max_events must be positive");
    }
}

std::size_t EventChannel::payload_bytes(const agent::ui::AgentEvent& event) noexcept
{
    using namespace agent::ui;
    std::size_t bytes = 0;
    if (event.metadata.tool_call_id) {
        bytes = add_saturated(bytes, event.metadata.tool_call_id->value.size());
    }
    std::visit(
        [&bytes](const auto& payload) noexcept {
            using Payload = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Payload, TextDelta>) {
                bytes = add_saturated(bytes, payload.text.size());
            } else if constexpr (std::is_same_v<Payload, ToolStarted>
                                 || std::is_same_v<Payload, ToolSucceeded>
                                 || std::is_same_v<Payload, ToolFailed>) {
                bytes = add_saturated(bytes, payload.name.size());
            }
        },
        event.payload);
    return bytes;
}

void EventChannel::record_drop(const std::optional<agent::ui::TurnId>& turn_id) noexcept
{
    dropped_events_.fetch_add(1, std::memory_order_relaxed);
    if (turn_id) {
        dropping_turn_ = turn_id;
        drop_events_without_turn_ = false;
    } else {
        dropping_turn_.reset();
        drop_events_without_turn_ = true;
    }
}

void EventChannel::on_event(const agent::ui::AgentEvent& event) noexcept
{
    using namespace agent::ui;
    try {
        const auto turn_id = event.metadata.turn_id;
        if (is_request_started(event)) {
            dropping_turn_.reset();
            drop_events_without_turn_ = false;
        }

        if (drop_events_without_turn_ || (turn_id && dropping_turn_ == turn_id)) {
            dropped_events_.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        const auto bytes = payload_bytes(event);
        if (bytes > limits_.max_event_payload_bytes) {
            record_drop(turn_id);
            return;
        }

        std::unique_lock lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock()) {
            record_drop(turn_id);
            return;
        }
        if (queue_.size() >= limits_.max_events
            || bytes > limits_.max_queued_payload_bytes - queued_payload_bytes_) {
            record_drop(turn_id);
            return;
        }

        queue_.emplace_back(event); // owned value copy; any allocation failure is caught below
        queued_payload_bytes_ += bytes;
    } catch (...) {
        record_drop(event.metadata.turn_id);
    }
}

void EventChannel::on_observer_failure() noexcept
{
    observer_failures_.fetch_add(1, std::memory_order_relaxed);
}

bool EventChannel::try_pop(agent::ui::AgentEvent& event)
{
    std::lock_guard lock(mutex_);
    if (queue_.empty()) {
        return false;
    }
    queued_payload_bytes_ -= payload_bytes(queue_.front());
    event = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

EventChannelSnapshot EventChannel::snapshot() const noexcept
{
    const auto dropped = dropped_events_.load(std::memory_order_relaxed);
    const auto observer_failures = observer_failures_.load(std::memory_order_relaxed);
    return {
        dropped,
        observer_failures,
        dropped != 0 || observer_failures != 0,
    };
}

std::size_t EventChannel::queued_events() const
{
    std::lock_guard lock(mutex_);
    return queue_.size();
}

AgentWorker::AgentWorker(ExecutorFactory factory, EventQueueLimits limits)
    : events_(std::make_shared<EventChannel>(limits))
    , factory_(std::move(factory))
{
    if (!factory_) {
        throw std::invalid_argument("agent worker requires an executor factory");
    }

    std::promise<void> started;
    auto ready = started.get_future();
    worker_ = std::thread([this, started = std::move(started)]() mutable { run(std::move(started)); });
    try {
        ready.get();
    } catch (...) {
        if (worker_.joinable()) {
            worker_.join();
        }
        throw;
    }
}

AgentWorker::~AgentWorker()
{
    try {
        request_close();
        join();
    } catch (...) {
        // Destructors must not throw; join itself is attempted below if needed.
        if (worker_.joinable()) {
            worker_.join();
        }
    }
}

std::future<TurnCompletion> AgentWorker::submit(std::string input)
{
    std::lock_guard lock(mutex_);
    if (!accepting_) {
        throw std::logic_error("agent worker is closing");
    }
    if (busy_ || pending_) {
        throw std::logic_error("agent worker already has a turn in flight");
    }

    Task task;
    task.id = next_task_id_++;
    task.input = std::move(input);
    auto future = task.completion.get_future();
    pending_.emplace(std::move(task));
    busy_ = true;
    work_available_.notify_one();
    return future;
}

bool AgentWorker::busy() const
{
    std::lock_guard lock(mutex_);
    return busy_;
}

void AgentWorker::request_close()
{
    {
        std::lock_guard lock(mutex_);
        accepting_ = false;
        closing_ = true;
    }
    work_available_.notify_all();
}

void AgentWorker::join()
{
    if (worker_.joinable()) {
        worker_.join();
    }
}

void AgentWorker::run(std::promise<void> started)
{
    try {
        executor_ = factory_(std::static_pointer_cast<agent::ui::EventObserver>(events_));
        if (!executor_) {
            throw std::runtime_error("executor factory returned null");
        }
        started.set_value();
    } catch (...) {
        started.set_exception(std::current_exception());
        return;
    }

    for (;;) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            work_available_.wait(lock, [this] { return closing_ || pending_.has_value(); });
            if (!pending_) {
                if (closing_) {
                    break;
                }
                continue;
            }
            task = std::move(*pending_);
            pending_.reset();
        }

        const auto before = events_->snapshot();
        TurnCompletion result;
        result.task_id = task.id;
        result.status = TurnStatus::returned;
        try {
            executor_->send(task.input);
        } catch (...) {
            result.status = TurnStatus::exception;
            result.exception = std::current_exception();
        }
        const auto after = events_->snapshot();
        result.dropped_events = after.dropped_events - before.dropped_events;
        result.observer_failures = after.observer_failures - before.observer_failures;

        {
            std::lock_guard lock(mutex_);
            busy_ = false;
        }
        task.completion.set_value(std::move(result));
    }

    executor_.reset(); // destructor runs on the worker thread
}

} // namespace tui_async
