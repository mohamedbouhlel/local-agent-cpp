#pragma once

#include "ui/agent_event.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <deque>

namespace tui_async {

struct EventQueueLimits {
    std::size_t max_events = 128;
    std::size_t max_queued_payload_bytes = 256 * 1024;
    std::size_t max_event_payload_bytes = 8 * 1024;
};

struct EventChannelSnapshot {
    std::uint64_t dropped_events = 0;
    std::uint64_t observer_failures = 0;
    bool details_incomplete = false;
};

// Single-producer AgentEvent bridge. on_event uses try_lock and owns a copy
// before returning. Queue accounting bounds event count and variable payload
// bytes; allocator and object overhead are not an RSS bound.
class EventChannel final : public agent::ui::EventObserver {
public:
    explicit EventChannel(EventQueueLimits limits = {});

    void on_event(const agent::ui::AgentEvent& event) noexcept override;
    void on_observer_failure() noexcept override;

    bool try_pop(agent::ui::AgentEvent& event);
    EventChannelSnapshot snapshot() const noexcept;
    std::size_t queued_events() const;

private:
    static std::size_t payload_bytes(const agent::ui::AgentEvent& event) noexcept;
    void record_drop(const std::optional<agent::ui::TurnId>& turn_id) noexcept;

    const EventQueueLimits limits_;
    mutable std::mutex mutex_;
    std::deque<agent::ui::AgentEvent> queue_;
    std::size_t queued_payload_bytes_ = 0;

    // These producer-only fields are touched only by the serialized observer
    // calls from the single worker thread.
    std::optional<agent::ui::TurnId> dropping_turn_;
    bool drop_events_without_turn_ = false;

    std::atomic<std::uint64_t> dropped_events_{0};
    std::atomic<std::uint64_t> observer_failures_{0};
};

enum class TurnStatus {
    returned,
    exception,
};

struct TurnCompletion {
    std::uint64_t task_id = 0;
    TurnStatus status = TurnStatus::exception;
    std::exception_ptr exception;
    std::uint64_t dropped_events = 0;
    std::uint64_t observer_failures = 0;

    bool event_details_incomplete() const noexcept
    {
        return dropped_events != 0 || observer_failures != 0;
    }
};

class TurnExecutor {
public:
    virtual ~TurnExecutor() = default;
    virtual void send(const std::string& input) = 0;
};

using ExecutorFactory = std::function<std::unique_ptr<TurnExecutor>(
    std::shared_ptr<agent::ui::EventObserver>)>;

// The executor is constructed and destroyed on the worker thread. A future
// carries the authoritative return/exception status independently of the
// lossy bounded details queue.
class AgentWorker final {
public:
    explicit AgentWorker(ExecutorFactory factory, EventQueueLimits limits = {});
    ~AgentWorker();

    AgentWorker(const AgentWorker&) = delete;
    AgentWorker& operator=(const AgentWorker&) = delete;

    std::future<TurnCompletion> submit(std::string input);
    std::shared_ptr<EventChannel> event_channel() const noexcept { return events_; }
    bool busy() const;

    // Closes admission without cancelling an active send. Accepted pending
    // work is drained; join may wait as long as the executor/send takes.
    void request_close();
    void join();

private:
    struct Task {
        std::uint64_t id = 0;
        std::string input;
        std::promise<TurnCompletion> completion;
    };

    void run(std::promise<void> started);

    std::shared_ptr<EventChannel> events_;
    ExecutorFactory factory_;
    std::unique_ptr<TurnExecutor> executor_; // worker thread only
    mutable std::mutex mutex_;
    std::condition_variable work_available_;
    std::optional<Task> pending_;
    bool accepting_ = true;
    bool closing_ = false;
    bool busy_ = false;
    std::uint64_t next_task_id_ = 1;
    std::thread worker_;
};

} // namespace tui_async
