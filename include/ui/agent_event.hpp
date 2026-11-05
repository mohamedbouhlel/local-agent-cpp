#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace agent::ui {

struct SessionId {
    std::uint64_t value = 0;
    bool operator==(const SessionId&) const = default;
};

struct TurnId {
    std::uint64_t value = 0;
    bool operator==(const TurnId&) const = default;
};

struct RequestId {
    std::uint64_t value = 0;
    bool operator==(const RequestId&) const = default;
};

struct ToolCallId {
    std::string value;
    bool operator==(const ToolCallId&) const = default;
};

enum class EventType {
    RequestStarted,
    HttpRequestStarted,
    TextDelta,
    ToolStarted,
    ToolSucceeded,
    ToolFailed,
    HttpRequestFinished,
    RequestFailed,
    TurnFinished,
};

struct EventMetadata {
    std::uint32_t schema_version = 1;
    SessionId session_id;
    std::optional<TurnId> turn_id;
    std::optional<RequestId> request_id;
    std::optional<ToolCallId> tool_call_id;
    std::uint64_t sequence = 0;
    std::chrono::system_clock::time_point timestamp_utc;
    std::chrono::steady_clock::duration elapsed{};
};

struct RequestStarted {};

struct HttpRequestStarted {
    std::uint32_t iteration = 0;
};

// Text is transient event data for streaming consumers. It is not metadata and
// must not be copied into persistent logs by later presentation components.
struct TextDelta {
    std::string text;
};

struct ToolStarted {
    std::string name;
};

struct ToolSucceeded {
    std::string name;
    std::chrono::steady_clock::duration duration{};
};

struct ToolFailed {
    std::string name;
    std::chrono::steady_clock::duration duration{};
};

enum class HttpRequestOutcome {
    completed,
    failed,
    legacy_callback_aborted,
};

struct HttpRequestFinished {
    std::chrono::steady_clock::duration duration{};
    std::uint64_t response_bytes = 0;
    HttpRequestOutcome outcome = HttpRequestOutcome::completed;
};

enum class RequestFailureCategory {
    http,
    protocol,
    iteration_limit,
    legacy_callback,
    unexpected,
};

struct RequestFailed {
    RequestFailureCategory category = RequestFailureCategory::unexpected;
};

enum class TurnOutcome {
    completed,
    failed,
};

struct TurnFinished {
    TurnOutcome outcome = TurnOutcome::failed;
    std::uint32_t http_requests = 0;
    std::uint32_t tools_succeeded = 0;
    std::uint32_t tools_failed = 0;
};

using EventPayload = std::variant<RequestStarted,
                                  HttpRequestStarted,
                                  TextDelta,
                                  ToolStarted,
                                  ToolSucceeded,
                                  ToolFailed,
                                  HttpRequestFinished,
                                  RequestFailed,
                                  TurnFinished>;

struct AgentEvent {
    EventMetadata metadata;
    EventPayload payload;

    EventType type() const noexcept;
};

// Implementations are owned by shared_ptr held by Agent. on_observer_failure
// is called at most once after publication is permanently disabled; it must
// report directly and must not publish another AgentEvent.
class EventObserver {
public:
    virtual ~EventObserver() = default;
    virtual void on_event(const AgentEvent& event) = 0;
    virtual void on_observer_failure() = 0;
};

} // namespace agent::ui
