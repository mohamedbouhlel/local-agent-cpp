#pragma once

#include "ui/agent_event.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace agent::ui {

enum class PresentationPhase {
    idle,
    submitted,
    waiting,
    streaming,
    tool_running,
    completed,
    failed,
};

enum class EventApplyStatus {
    applied,
    duplicate,
    out_of_order,
    sequence_gap,
    inconsistent,
};

struct EventApplyResult {
    EventApplyStatus status = EventApplyStatus::inconsistent;
    std::string detail;

    bool applied() const noexcept { return status == EventApplyStatus::applied; }
};

struct PresentationSnapshot {
    PresentationPhase phase = PresentationPhase::idle;
    std::optional<SessionId> session_id;
    std::optional<TurnId> turn_id;
    std::optional<RequestId> active_request_id;
    std::optional<RequestId> last_request_id;
    std::optional<ToolCallId> active_tool_call_id;
    std::string active_tool_name;
    std::optional<ToolCallId> pending_authorization_tool_call_id;
    std::string pending_authorization_tool_name;
    std::optional<ToolCallId> approved_authorization_tool_call_id;
    std::string approved_authorization_tool_name;
    std::uint64_t last_sequence = 0;
    std::uint64_t http_requests = 0;
    std::uint64_t tools_started = 0;
    std::uint64_t tools_succeeded = 0;
    std::uint64_t tools_failed = 0;
    std::uint64_t text_delta_count = 0;
    bool request_failed = false;
    std::optional<HttpRequestOutcome> last_http_outcome;
    std::optional<TurnOutcome> turn_outcome;
};

// A pure event consumer. It stores status and counts, never TextDelta content.
// Rejected events do not advance last_sequence or mutate the snapshot.
class PresentationState {
public:
    EventApplyResult apply(const AgentEvent& event);

    const PresentationSnapshot& snapshot() const noexcept { return snapshot_; }
    void reset() noexcept;

private:
    struct ActiveTool {
        ToolCallId id;
        RequestId request_id;
        std::string name;
    };

    PresentationSnapshot snapshot_;
    std::optional<ActiveTool> active_tool_;
    std::vector<ToolCallId> tool_ids_in_turn_;
    std::optional<RequestId> last_request_id_in_session_;
};

} // namespace agent::ui
