#pragma once

#include "ui/agent_worker.hpp"
#include "ui/presentation_state.hpp"
#include "ui/terminal_text_sanitizer.hpp"
#include "ui/terminal_session.hpp"
#include "ui/tui_view.hpp"
#include "ui/tui_navigation.hpp"
#include "ui/tui_approval_broker.hpp"

#include <cstddef>
#include <cstdint>
#include <future>
#include <optional>
#include <string>

namespace agent::ui {

class TuiEventLoop final {
public:
    TuiEventLoop(AgentWorker& worker, TerminalSession& terminal, TuiView& view,
                 std::shared_ptr<TuiApprovalBroker> approvals);
    int run();

private:
    bool drain_events();
    bool collect_completion();
    void handle_event(const AgentEvent& event);
    bool handle_key(wint_t key, bool key_code);
    bool sync_approval_dialog();
    bool handle_approval_key(wint_t key, bool key_code);
    void submit_input();
    void request_exit();

    AgentWorker& worker_;
    TerminalSession& terminal_;
    TuiView& view_;
    std::shared_ptr<TuiApprovalBroker> approvals_;
    PresentationState presentation_state_;
    TuiNavigation navigation_;
    TerminalTextSanitizer sanitizer_;
    std::string input_;
    std::size_t cursor_byte_ = 0;
    std::optional<int> preferred_composer_column_;
    std::optional<std::future<TurnCompletion>> active_turn_;
    std::optional<TurnId> active_turn_id_;
    std::shared_ptr<const WriteApprovalRequest> approval_request_;
    std::string approval_challenge_;
    std::string approval_confirmation_input_;
    std::size_t approval_scroll_ = 0;
    bool approval_expired_ = false;
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
