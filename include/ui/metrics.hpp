#pragma once

#include "ui/agent_event.hpp"

#include <chrono>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace agent::ui {

inline constexpr std::size_t max_http_metric_details = 64;
inline constexpr std::size_t max_tool_metric_details = 64;
inline constexpr std::size_t max_tool_id_metric_bytes = 16 * 1024;
inline constexpr std::size_t max_tool_name_metric_bytes = 128;

enum class ToolMetricOutcome {
    succeeded,
    failed,
};

struct HttpRequestMetric {
    RequestId request_id;
    std::optional<std::chrono::steady_clock::duration> duration;
    std::optional<std::uint64_t> response_body_bytes;
    std::optional<HttpRequestOutcome> outcome;
};

struct ToolMetric {
    RequestId request_id;
    ToolCallId tool_call_id;
    std::string name;
    std::optional<std::chrono::steady_clock::duration> duration;
    std::optional<ToolMetricOutcome> outcome;
};

struct TurnMetrics {
    std::optional<TurnId> turn_id;
    std::optional<std::chrono::steady_clock::duration> duration;
    std::optional<std::chrono::steady_clock::duration> first_delta_latency;
    std::optional<TurnOutcome> outcome;

    std::uint64_t http_requests_started = 0;
    std::uint64_t http_requests_finished = 0;
    std::uint64_t tools_started = 0;
    std::uint64_t tools_succeeded = 0;
    std::uint64_t tools_failed = 0;
    std::uint64_t text_fragments = 0;
    std::optional<std::uint64_t> response_body_bytes;

    bool event_details_complete = true;
    bool detail_history_truncated = false;
    bool counter_overflowed = false;
    std::uint64_t evicted_http_details = 0;
    std::uint64_t evicted_tool_details = 0;
    std::uint64_t rejected_tool_details = 0;
    std::size_t retained_tool_id_bytes = 0;

    std::vector<HttpRequestMetric> http_requests;
    std::vector<ToolMetric> tools;

    std::uint64_t successful_tools() const noexcept { return tools_succeeded; }
    std::uint64_t failed_tools() const noexcept { return tools_failed; }
    std::optional<std::uint64_t> response_body_bytes_received() const noexcept { return response_body_bytes; }
};

// Consumes ordered AgentEvent data only. TextDelta payloads and other sensitive
// content are discarded; retained detail is bounded independently of process RSS.
class MetricCollector {
public:
    MetricCollector();

    void begin_pending_turn() noexcept;
    void consume(const AgentEvent& event);
    void mark_event_details_incomplete() noexcept;
    const TurnMetrics& snapshot() const noexcept { return metrics_; }

private:
    void reset_for_turn(std::optional<TurnId> turn_id, std::uint64_t sequence) noexcept;
    void increment(std::uint64_t& counter) noexcept;
    void retain_http_detail(RequestId request_id);
    void retain_tool_detail(RequestId request_id, const ToolCallId& tool_call_id, std::string_view name);
    void evict_oldest_http_detail() noexcept;
    void evict_oldest_tool_detail() noexcept;

    TurnMetrics metrics_;
    std::optional<std::chrono::steady_clock::duration> turn_started_at_;
    std::optional<std::chrono::steady_clock::duration> first_delta_at_;
    std::optional<std::uint64_t> last_sequence_;
    std::uint64_t pending_http_requests_ = 0;
    std::optional<std::uint64_t> received_body_bytes_total_;
    std::array<std::optional<RequestId>, max_http_metric_details> pending_http_request_ids_{};
    std::size_t pending_http_request_count_ = 0;
};

} // namespace agent::ui
