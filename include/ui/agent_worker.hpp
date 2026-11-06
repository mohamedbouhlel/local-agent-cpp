#pragma once

#include "agent.hpp"

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

namespace agent::ui {

struct EventQueueLimits {
    std::size_t max_events = 128;
    std::size_t max_queued_payload_bytes = 256 * 1024;
    std::size_t max_event_payload_bytes = 8 * 1024;
};

struct EventChannelSnapshot {
    std::uint64_t dropped_events = 0;
    std::uint64_t observer_failures = 0;
    std::optional<SessionId> session_id;
    std::optional<TurnId> turn_id;
    bool details_incomplete = false;
};

// Single-producer bridge for the Agent worker. Queue accounting bounds event
// count and variable payload bytes; allocator/object overhead is not an RSS cap.
class EventChannel final : public EventObserver {
public:
    explicit EventChannel(EventQueueLimits limits = {});

    void on_event(const AgentEvent& event) noexcept override;
    void on_observer_failure() noexcept override;

    bool try_pop(AgentEvent& event);
    EventChannelSnapshot snapshot() const noexcept;
    std::size_t queued_events() const;

private:
    static std::size_t payload_bytes(const AgentEvent& event) noexcept;
    void record_drop(const std::optional<TurnId>& turn_id) noexcept;

    const EventQueueLimits limits_;
    mutable std::mutex mutex_;
    std::deque<AgentEvent> queue_;
    std::size_t queued_payload_bytes_ = 0;

    // Called serially by the sole worker-owned Agent.
    std::optional<TurnId> dropping_turn_;
    bool drop_events_without_turn_ = false;

    std::atomic<std::uint64_t> dropped_events_{0};
    std::atomic<std::uint64_t> observer_failures_{0};
    std::atomic<std::uint64_t> session_id_{0};
    std::atomic<std::uint64_t> turn_id_{0};
};

enum class TurnStatus {
    returned,
    exception,
};

struct TurnCompletion {
    std::uint64_t task_id = 0;
    TurnStatus status = TurnStatus::exception;
    std::exception_ptr exception;
    std::optional<std::string> response;
    std::optional<SessionId> session_id;
    std::optional<TurnId> turn_id;
    std::uint64_t dropped_events = 0;
    std::uint64_t observer_failures = 0;

    bool event_details_incomplete() const noexcept
    {
        return dropped_events != 0 || observer_failures != 0;
    }
};

using AgentFactory = std::function<std::unique_ptr<Agent>(std::shared_ptr<EventObserver>)>;

// Agent creation, send() calls, and Agent destruction all occur on the worker.
// The factory must keep non-owned Agent dependencies alive through join().
// The owner must destroy AgentWorker from a non-worker thread.
class AgentWorker final {
public:
    explicit AgentWorker(AgentFactory factory, EventQueueLimits limits = {});
    ~AgentWorker();

    AgentWorker(const AgentWorker&) = delete;
    AgentWorker& operator=(const AgentWorker&) = delete;

    std::future<TurnCompletion> submit(std::string input);
    std::shared_ptr<EventChannel> event_channel() const noexcept { return events_; }
    bool busy() const;

    // Rejects new work; does not cancel an active send. join/destruction may
    // wait until its HTTP/SSE/tool operation actually returns. join() from
    // the worker itself is rejected with std::logic_error.
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
    AgentFactory factory_;
    std::unique_ptr<Agent> agent_; // worker thread only
    mutable std::mutex mutex_;
    std::condition_variable work_available_;
    std::optional<Task> pending_;
    bool accepting_ = true;
    bool closing_ = false;
    bool busy_ = false;
    std::uint64_t next_task_id_ = 1;
    std::thread worker_;
};

} // namespace agent::ui
