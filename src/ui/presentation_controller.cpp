#include "ui/presentation_controller.hpp"
#include "ui/terminal_text_sanitizer.hpp"

#include <iostream>
#include <stdexcept>

namespace agent::ui {

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
        renderer_->render_status(StatusKind::success, "Tool completed: " + payload.name);
        break;
    }
    case EventType::ToolFailed: {
        const auto& payload = std::get<ToolFailed>(event.payload);
        renderer_->render_status(StatusKind::error, "Tool failed: " + payload.name);
        break;
    }
    case EventType::HttpRequestFinished: {
        const auto& payload = std::get<HttpRequestFinished>(event.payload);
        if (payload.outcome != HttpRequestOutcome::completed) {
            renderer_->render_status(StatusKind::warning, "HTTP transport ended without a complete response.");
        }
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
        break;
    }
    }
}

void PresentationController::on_observer_failure() noexcept
{
    observer_failed_ = true;
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
