#pragma once

#include "ui/agent_worker.hpp"
#include "ui/presentation_state.hpp"
#include "ui/terminal_text_sanitizer.hpp"
#include "ui/terminal_session.hpp"
#include "ui/tui_view.hpp"

#include <cstddef>
#include <cstdint>
#include <future>
#include <optional>
#include <string>

namespace agent::ui {

class TuiEventLoop final {
public:
    TuiEventLoop(AgentWorker& worker, TerminalSession& terminal, TuiView& view);
    int run();

private:
    bool drain_events();
    bool collect_completion();
    void handle_event(const AgentEvent& event);
    bool handle_key(wint_t key, bool key_code);
    void submit_input();
    void request_exit();

    AgentWorker& worker_;
    TerminalSession& terminal_;
    TuiView& view_;
    PresentationState presentation_state_;
    TerminalTextSanitizer sanitizer_;
    std::string input_;
    std::size_t cursor_byte_ = 0;
    std::optional<std::future<TurnCompletion>> active_turn_;
    std::optional<TurnId> active_turn_id_;
    bool composer_focused_ = true;
    bool closing_ = false;
    bool event_details_incomplete_ = false;
    bool presentation_state_desynchronized_ = false;
    std::uint64_t dropped_events_at_submit_ = 0;
    std::uint64_t observer_failures_at_submit_ = 0;
    bool dirty_ = true;
    int last_rows_ = -1;
    int last_columns_ = -1;
};

} // namespace agent::ui
