#pragma once

#include "ui/agent_event.hpp"
#include "ui/console_renderer.hpp"
#include "ui/presentation_state.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace agent::ui {

class PresentationController final : public EventObserver {
public:
    explicit PresentationController(std::unique_ptr<ConsoleRenderer> renderer);

    void on_event(const AgentEvent& event) override;
    void on_observer_failure() noexcept override;

    const PresentationSnapshot& snapshot() const noexcept { return state_.snapshot(); }
    RendererMode mode() const noexcept { return renderer_->mode(); }
    void begin_turn() noexcept { turn_had_text_ = false; }
    bool observer_failed() const noexcept { return observer_failed_; }
    bool turn_had_text() const noexcept { return turn_had_text_; }
    void render_fallback_response(std::string_view response) noexcept;
    void report_observer_failure_after_text() noexcept;
    void report_failure(std::string_view message) noexcept;

private:
    std::unique_ptr<ConsoleRenderer> renderer_;
    PresentationState state_;
    bool observer_failed_ = false;
    bool turn_had_text_ = false;
};

} // namespace agent::ui
