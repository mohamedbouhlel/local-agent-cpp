#include "ui/presentation_controller.hpp"
#include "ui/terminal_text_sanitizer.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace agent::ui {
namespace {

std::string format_duration(std::chrono::steady_clock::duration duration)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(3)
           << std::chrono::duration<double, std::milli>(duration).count() << " ms";
    return output.str();
}

std::string optional_duration(const std::optional<std::chrono::steady_clock::duration>& duration)
{
    return duration ? format_duration(*duration) : "unavailable";
}

std::string exact_count(std::uint64_t value, bool overflowed)
{
    return overflowed ? "unavailable" : std::to_string(value);
}

std::string metrics_summary(const TurnMetrics& metrics)
{
    std::ostringstream output;
    output << "Turn metrics (technical outcome: ";
    if (metrics.outcome) output << (*metrics.outcome == TurnOutcome::completed ? "completed" : "failed");
    else output << "unavailable";
    output << "): duration " << optional_duration(metrics.duration)
           << "; HTTP requests " << exact_count(metrics.http_requests_started, metrics.counter_overflowed)
           << "; tools " << exact_count(metrics.tools_started, metrics.counter_overflowed) << " ("
           << exact_count(metrics.successful_tools(), metrics.counter_overflowed) << " succeeded, "
           << exact_count(metrics.failed_tools(), metrics.counter_overflowed)
           << " failed); text fragments " << exact_count(metrics.text_fragments, metrics.counter_overflowed)
           << "; first delta " << optional_duration(metrics.first_delta_latency)
           << "; response body bytes received ";
    if (metrics.response_body_bytes_received()) output << *metrics.response_body_bytes_received();
    else output << "unavailable";
    output << ".";
    if (!metrics.event_details_complete) output << " Event details incomplete; observed counts are partial.";
    if (metrics.detail_history_truncated) output << " Per-request/tool detail history is truncated.";
    if (metrics.counter_overflowed) output << " One or more counters overflowed; exact value unavailable.";
    return output.str();
}

} // namespace

PresentationController::PresentationController(std::unique_ptr<ConsoleRenderer> renderer)
    : renderer_(std::move(renderer))
{
    if (!renderer_) {
        throw std::invalid_argument("presentation controller requires a renderer");
    }
}

void PresentationController::on_event(const AgentEvent& event)
{
    const auto applied = state_.apply(event);
    if (!applied.applied()) {
        throw std::runtime_error("presentation rejected agent event: " + applied.detail);
    }

    const bool first_text_delta = event.type() == EventType::TextDelta && metrics_.snapshot().text_fragments == 0;
    metrics_.consume(event);

    switch (event.type()) {
    case EventType::RequestStarted:
        turn_had_text_ = false;
        renderer_->render_status(StatusKind::info, "Request submitted.");
        break;
    case EventType::HttpRequestStarted: {
        const auto& payload = std::get<HttpRequestStarted>(event.payload);
        renderer_->render_status(StatusKind::info,
                                 "Waiting for model response (request " + std::to_string(payload.iteration) + ").");
        break;
    }
    case EventType::TextDelta:
        turn_had_text_ = true;
        if (first_text_delta) {
            renderer_->render_status(StatusKind::model,
                                     "Receiving model stream; first delta observed after "
                                         + optional_duration(metrics_.snapshot().first_delta_latency)
                                         + " (not server TTFT).");
        }
        renderer_->render_text(std::get<TextDelta>(event.payload).text);
        break;
    case EventType::ToolAuthorizationRequested:
        renderer_->render_status(StatusKind::warning, "Waiting for explicit approval to create a workspace file.");
        break;
    case EventType::ToolAuthorizationResolved: {
        const auto status = std::get<ToolAuthorizationResolved>(event.payload).status;
        const char* message = status == ToolAuthorizationStatus::approved ? "File creation approved; execution is starting."
            : status == ToolAuthorizationStatus::denied ? "File creation refused; no file was created."
            : status == ToolAuthorizationStatus::cancelled ? "File creation cancelled; no file was created."
            : status == ToolAuthorizationStatus::expired ? "File approval expired; no file was created."
            : "File approval unavailable; no file was created.";
        renderer_->render_status(status == ToolAuthorizationStatus::approved ? StatusKind::info : StatusKind::warning, message);
        break;
    }
    case EventType::ToolStarted: {
        const auto& payload = std::get<ToolStarted>(event.payload);
        renderer_->render_status(StatusKind::tool, "Running tool: " + payload.name);
        break;
    }
    case EventType::ToolSucceeded: {
        const auto& payload = std::get<ToolSucceeded>(event.payload);
        renderer_->render_status(StatusKind::success,
                                 "Tool completed: " + payload.name + " in " + format_duration(payload.duration));
        break;
    }
    case EventType::ToolFailed: {
        const auto& payload = std::get<ToolFailed>(event.payload);
        renderer_->render_status(StatusKind::error,
                                 "Tool failed: " + payload.name + " after " + format_duration(payload.duration));
        break;
    }
    case EventType::HttpRequestFinished: {
        const auto& payload = std::get<HttpRequestFinished>(event.payload);
        const auto request_id = event.metadata.request_id
            ? std::to_string(event.metadata.request_id->value) : std::string("unknown");
        const auto kind = payload.outcome == HttpRequestOutcome::completed ? StatusKind::info : StatusKind::warning;
        renderer_->render_status(kind,
                                 "HTTP request " + request_id + " finished in " + format_duration(payload.duration)
                                     + "; response body bytes received: " + std::to_string(payload.response_bytes)
                                     + ". Transport completion does not prove LLM stream completeness.");
        break;
    }
    case EventType::RequestFailed:
        renderer_->flush_text();
        break;
    case EventType::TurnFinished: {
        const auto& payload = std::get<TurnFinished>(event.payload);
        renderer_->flush_text();
        if (payload.outcome == TurnOutcome::completed) {
            renderer_->render_text("\n");
            renderer_->render_status(StatusKind::success,
                                     "Turn completed technically; task success is not independently verified.");
        }
        renderer_->render_status(payload.outcome == TurnOutcome::completed ? StatusKind::secondary : StatusKind::warning,
                                 metrics_summary(metrics_.snapshot()));
        break;
    }
    }
}

void PresentationController::on_observer_failure() noexcept
{
    observer_failed_ = true;
    metrics_.mark_event_details_incomplete();
    try {
        renderer_->render_status(StatusKind::error, "UI observer disabled after a presentation failure.");
    } catch (...) {
        try {
            std::cerr << "[error] UI observer disabled after a presentation failure.\n";
        } catch (...) {
        }
    }
}

void PresentationController::render_fallback_response(std::string_view response) noexcept
{
    try {
        renderer_->flush_text();
        renderer_->render_text(response);
        renderer_->render_text("\n");
        renderer_->render_status(StatusKind::warning,
                                 "Complete model response rendered after event observation became unavailable.");
    } catch (...) {
        try {
            std::cerr << "[error] Complete model response could not be rendered by the console UI.\n";
        } catch (...) {
        }
    }
}

void PresentationController::report_observer_failure_after_text() noexcept
{
    try {
        renderer_->render_status(
            StatusKind::warning,
            "The agent call completed, but event rendering failed after text began; displayed output may be incomplete.");
    } catch (...) {
        try {
            std::cerr << "[warning] The agent call completed, but displayed model output may be incomplete.\n";
        } catch (...) {
        }
    }
}

void PresentationController::report_failure(std::string_view message) noexcept
{
    try {
        renderer_->flush_text();
        if (turn_had_text_) {
            renderer_->render_text("\n");
        }
        renderer_->render_status(StatusKind::error, message);
        if (turn_had_text_) {
            renderer_->render_status(StatusKind::warning,
                                     "Streamed model output may be incomplete; it is not a completed response.");
        }
    } catch (...) {
        try {
            std::cerr << "[error] " << sanitize_terminal_line(message) << '\n';
        } catch (...) {
        }
    }
}

} // namespace agent::ui
