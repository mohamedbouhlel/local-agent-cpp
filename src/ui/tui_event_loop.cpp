#include "ui/tui_event_loop.hpp"

#include "http_client.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <utility>

namespace agent::ui {

TuiEventLoop::TuiEventLoop(AgentWorker& worker, TerminalSession& terminal, TuiView& view,
                           std::shared_ptr<TuiApprovalBroker> approvals)
    : worker_(worker), terminal_(terminal), view_(view), approvals_(std::move(approvals))
{
    if (!approvals_) throw std::invalid_argument("TUI event loop requires an approval broker");
}

int TuiEventLoop::run()
{
    WINDOW* window = terminal_.window();
    if (window == nullptr) throw std::runtime_error("terminal session has no screen");
    while (!closing_ || active_turn_) {
        bool changed = drain_events();
        changed = sync_approval_dialog() || changed;
        changed = collect_completion() || changed;
        int rows = 0, cols = 0;
        getmaxyx(window, rows, cols);
        if (rows != last_rows_ || cols != last_columns_) {
            last_rows_ = rows; last_columns_ = cols;
            view_.on_resize(cols, std::max(1, rows - 8));
            view_.set_status("Terminal resized; compact layout active when space is limited.");
            changed = true;
        }
        if (changed || dirty_) {
            if (approval_request_) view_.draw_approval(window, *approval_request_, approval_scroll_,
                                                       approval_challenge_, approval_confirmation_input_, approval_expired_);
            else view_.draw(window, terminal_.unicode_enabled(), navigation_, input_, cursor_byte_);
            dirty_ = false;
        }
        wint_t key{};
        const int rc = wget_wch(window, &key);
        if (rc != ERR && handle_key(key, rc == KEY_CODE_YES)) {
            dirty_ = true;
            if (closing_ && active_turn_) {
                if (approval_request_) view_.draw_approval(window, *approval_request_, approval_scroll_,
                                                           approval_challenge_, approval_confirmation_input_, approval_expired_);
                else view_.draw(window, terminal_.unicode_enabled(), navigation_, input_, cursor_byte_);
                dirty_ = false;
            }
        }
        (void)collect_completion();
        if (closing_ && !active_turn_ && dirty_) {
            view_.draw(window, terminal_.unicode_enabled(), navigation_, input_, cursor_byte_);
            dirty_ = false;
        }
    }
    approvals_->cancel_pending();
    worker_.request_close();
    worker_.join();
    return 0;
}

bool TuiEventLoop::sync_approval_dialog()
{
    const auto pending = approvals_->pending_request();
    if (pending && (!approval_request_ || pending->approval_id != approval_request_->approval_id)) {
        std::string challenge;
        try { challenge = generate_approval_challenge(); }
        catch (...) {
            static_cast<void>(approvals_->resolve(pending->approval_id, ApprovalDecision::unavailable));
            return true;
        }
        approval_request_ = pending;
        approval_challenge_ = std::move(challenge);
        approval_confirmation_input_.clear();
        approval_scroll_ = 0;
        approval_expired_ = false;
        if (!approval_mouse_suspended_) {
            terminal_.set_mouse_reporting_enabled(false);
            approval_mouse_suspended_ = true;
        }
        return true;
    }
    if (approval_request_ && !approvals_->is_pending(approval_request_->approval_id)) {
        if (approvals_->was_expired(approval_request_->approval_id)) {
            approval_expired_ = true;
            approval_challenge_.clear();
            approval_confirmation_input_.clear();
            return true;
        }
        close_approval_dialog();
        return true;
    }
    return false;
}

void TuiEventLoop::close_approval_dialog() noexcept
{
    approval_request_.reset();
    approval_challenge_.clear();
    approval_confirmation_input_.clear();
    approval_scroll_ = 0;
    approval_expired_ = false;
    if (approval_mouse_suspended_) {
        terminal_.set_mouse_reporting_enabled(true);
        approval_mouse_suspended_ = false;
    }
}

bool TuiEventLoop::handle_approval_key(wint_t key, bool key_code)
{
    if (!approval_request_) return false;
    if (key_code && key == KEY_MOUSE) {
        MEVENT ignored{};
        static_cast<void>(getmouse(&ignored));
        return false;
    }
    if (key_code && key == KEY_F(10)) {
        static_cast<void>(approvals_->resolve(approval_request_->approval_id, ApprovalDecision::cancelled));
        request_exit();
        return true;
    }
    if (approval_expired_ || std::chrono::steady_clock::now() >= approval_request_->expires_at) {
        approval_expired_ = true;
        approval_challenge_.clear();
        approval_confirmation_input_.clear();
        if ((key_code && key == KEY_ENTER) || (!key_code && (key == 10 || key == 13 || key == 27))) {
            close_approval_dialog();
        }
        return true;
    }
    int rows = 0, cols = 0;
    getmaxyx(terminal_.window(), rows, cols);
    const auto page = static_cast<std::size_t>(std::max(1, rows - 2));
    if (key_code) {
        if (key == KEY_ENTER) {
            const auto decision = !TuiView::approval_inspectable(rows, cols) ? ApprovalDecision::unavailable
                : matches_approval_confirmation(approval_challenge_, approval_confirmation_input_)
                    ? ApprovalDecision::approved : ApprovalDecision::denied;
            static_cast<void>(approvals_->resolve(approval_request_->approval_id, decision));
            return true;
        }
        if (key == KEY_BACKSPACE || key == KEY_DC) {
            if (!approval_confirmation_input_.empty()) approval_confirmation_input_.pop_back();
            return true;
        }
        if (key == KEY_UP) { if (approval_scroll_ > 0) --approval_scroll_; return true; }
        if (key == KEY_DOWN) { ++approval_scroll_; return true; }
        if (key == KEY_PPAGE) { approval_scroll_ = approval_scroll_ > page ? approval_scroll_ - page : 0; return true; }
        if (key == KEY_NPAGE) { approval_scroll_ += page; return true; }
        if (key == KEY_RESIZE) return true;
        return true;
    }
    if (key == 'y' || key == 'Y') {
        if (approval_confirmation_input_.size() < 64) approval_confirmation_input_.push_back(static_cast<char>(key));
        return true;
    }
    if (key == 27) { static_cast<void>(approvals_->resolve(approval_request_->approval_id, ApprovalDecision::cancelled)); return true; }
    if (key == 10 || key == 13) {
        const auto decision = !TuiView::approval_inspectable(rows, cols) ? ApprovalDecision::unavailable
            : matches_approval_confirmation(approval_challenge_, approval_confirmation_input_)
                ? ApprovalDecision::approved : ApprovalDecision::denied;
        static_cast<void>(approvals_->resolve(approval_request_->approval_id, decision));
        return true;
    }
    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (!approval_confirmation_input_.empty()) approval_confirmation_input_.pop_back();
        return true;
    }
    if (key >= 32 && key <= 126 && approval_confirmation_input_.size() < 64) {
        approval_confirmation_input_.push_back(static_cast<char>(key));
        return true;
    }
    return true;
}

bool TuiEventLoop::drain_events()
{
    bool changed = false;
    AgentEvent event;
    const auto channel = worker_.event_channel();
    while (channel->try_pop(event)) {
        if (!std::holds_alternative<TextDelta>(event.payload)) {
            changed = flush_pending_assistant_delta() || changed;
        }
        handle_event(event);
        changed = true;
    }
    changed = flush_pending_assistant_delta() || changed;
    const auto snapshot = channel->snapshot();
    if (active_turn_ && (snapshot.dropped_events > dropped_events_at_submit_
                         || snapshot.observer_failures > observer_failures_at_submit_)) {
        if (!event_details_incomplete_) view_.add_diagnostic("Event queue reported lost details; this turn's event detail is incomplete.");
        event_details_incomplete_ = true;
        metrics_.mark_event_details_incomplete();
        view_.set_metrics(metrics_.snapshot());
        view_.set_status("Event details incomplete: the event queue reported lost details.");
        changed = true;
    }
    return changed;
}

bool TuiEventLoop::flush_pending_assistant_delta()
{
    if (pending_assistant_delta_.empty()) return false;
    view_.append_assistant_delta(pending_assistant_delta_);
    pending_assistant_delta_.clear();
    return true;
}

void TuiEventLoop::handle_event(const AgentEvent& event)
{
    const bool turn_boundary = std::holds_alternative<RequestStarted>(event.payload) && !active_turn_id_;
    bool applied = false;
    if (!presentation_state_desynchronized_) {
        const auto apply_result = presentation_state_.apply(event);
        if (apply_result.applied()) {
            applied = true;
        } else {
            presentation_state_desynchronized_ = true;
            // A new RequestStarted after a previous dropped turn is a valid
            // resynchronization boundary. The worker's FIFO still preserves
            // order within this new turn; its completion independently says
            // whether this turn lost details.
            if (!turn_boundary) {
                event_details_incomplete_ = true;
                metrics_.mark_event_details_incomplete();
                view_.add_diagnostic("Presentation event sequence is inconsistent; details may be incomplete.");
                view_.set_status("Event details incomplete: inconsistent or missing event sequence.");
            }
        }
    }
    if (!active_turn_id_) active_turn_id_ = event.metadata.turn_id;
    if (event.metadata.turn_id != active_turn_id_) {
        event_details_incomplete_ = true;
        metrics_.mark_event_details_incomplete();
        view_.set_metrics(metrics_.snapshot());
        view_.add_diagnostic("Presentation event turn correlation mismatch; details are incomplete.");
        view_.set_status("Event details incomplete: event correlation mismatch.");
        return;
    }
    if (applied || turn_boundary || presentation_state_desynchronized_) {
        try {
            metrics_.consume(event);
        } catch (...) {
            event_details_incomplete_ = true;
            metrics_.mark_event_details_incomplete();
            view_.add_diagnostic("Metric collection failed; turn metrics are incomplete.");
            view_.set_status("Metrics incomplete: collection failed.");
        }
        if (!metrics_.snapshot().event_details_complete) event_details_incomplete_ = true;
        view_.set_metrics(metrics_.snapshot());
    }
    std::visit([this](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, RequestStarted>) view_.set_status("Request submitted.");
        else if constexpr (std::is_same_v<T, HttpRequestStarted>) view_.set_status("Waiting for model response…");
        else if constexpr (std::is_same_v<T, TextDelta>) {
            const auto safe = sanitizer_.feed(payload.text);
            pending_assistant_delta_ += safe;
            view_.set_status("Receiving model stream…");
        } else if constexpr (std::is_same_v<T, ToolAuthorizationRequested>) {
            view_.set_status("Waiting for explicit write approval.");
        } else if constexpr (std::is_same_v<T, ToolAuthorizationResolved>) {
            const auto status = payload.status;
            const auto authorization_message = status == ToolAuthorizationStatus::approved ? "write_file approval granted; execution follows."
                : status == ToolAuthorizationStatus::denied ? "write_file refused; no file was created."
                : status == ToolAuthorizationStatus::cancelled ? "write_file approval cancelled; no file was created."
                : status == ToolAuthorizationStatus::expired ? "write_file approval expired; no file was created."
                : "write_file approval unavailable; no file was created.";
            view_.add_diagnostic(authorization_message);
            if (status != ToolAuthorizationStatus::approved) view_.add_notice(authorization_message);
            view_.set_status(status == ToolAuthorizationStatus::approved ? "Write approved; starting writer."
                : status == ToolAuthorizationStatus::denied ? "Write refused; agent may continue."
                : status == ToolAuthorizationStatus::cancelled ? "Write cancelled; agent may continue."
                : status == ToolAuthorizationStatus::expired ? "Write approval expired; agent may continue."
                : "Write approval unavailable; agent may continue.");
        } else if constexpr (std::is_same_v<T, ToolStarted>) {
            const auto name = sanitize_terminal_line(payload.name);
            view_.add_tool_activity("Started: " + name);
            view_.add_notice("Running tool: " + name);
            view_.set_status("Tool running.");
        } else if constexpr (std::is_same_v<T, ToolSucceeded>) {
            const auto name = sanitize_terminal_line(payload.name);
            view_.add_tool_activity("Succeeded: " + name + " (" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(payload.duration).count()) + " ms)");
            view_.add_notice("Tool completed: " + name);
            view_.set_status("Tool completed; waiting for model.");
        } else if constexpr (std::is_same_v<T, ToolFailed>) {
            const auto name = sanitize_terminal_line(payload.name);
            view_.add_tool_activity("Failed: " + name + " (" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(payload.duration).count()) + " ms)");
            view_.add_notice("Tool failed: " + name);
            view_.set_status("Tool failed; agent may continue.");
        } else if constexpr (std::is_same_v<T, RequestFailed>) {
            const auto category = payload.category == RequestFailureCategory::http ? "HTTP" :
                                  payload.category == RequestFailureCategory::protocol ? "protocol" :
                                  payload.category == RequestFailureCategory::iteration_limit ? "iteration limit" :
                                  payload.category == RequestFailureCategory::legacy_callback ? "callback" : "unexpected";
            view_.add_diagnostic(std::string("Request failed: ") + category + ".");
            view_.set_status("HTTP/protocol request failed.");
        } else if constexpr (std::is_same_v<T, TurnFinished>) {
            view_.add_diagnostic(payload.outcome == TurnOutcome::completed ? "Turn completed technically; functional success is unverified." : "Turn failed.");
            view_.set_status(payload.outcome == TurnOutcome::completed ? "Turn completed technically; task success is not verified." : "Turn failed.");
        } else if constexpr (std::is_same_v<T, HttpRequestFinished>) view_.set_status("HTTP transport finished; model turn may continue.");
    }, event.payload);
}

bool TuiEventLoop::collect_completion()
{
    if (!active_turn_ || active_turn_->wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return false;
    // A ready future means the worker has finished publishing for this turn.
    // Drain once more before consuming completion so close cannot leave
    // already-published terminal/tool events unrendered.
    (void)drain_events();
    (void)sync_approval_dialog();
    auto result = active_turn_->get();
    active_turn_.reset();
    event_details_incomplete_ = event_details_incomplete_ || result.event_details_incomplete();
    if (result.event_details_incomplete()) metrics_.mark_event_details_incomplete();
    const auto tail = sanitizer_.finish();
    if (!tail.empty()) view_.append_assistant_delta(tail);
    if (result.status == TurnStatus::returned) {
        if (event_details_incomplete_ && result.response) {
            TerminalTextSanitizer replacement;
            view_.replace_assistant_response(replacement.feed(*result.response) + replacement.finish());
            view_.finish_assistant(true);
            view_.set_status("Turn completed technically; event details were incomplete. Task success is not verified.");
        } else {
            view_.finish_assistant(true);
            view_.set_status("Turn completed technically; task success is not verified.");
        }
    } else {
        view_.finish_assistant(false);
        std::string message = "Unexpected agent failure.";
        std::string diagnostic = "Turn failed with an unexpected error.";
        try { if (result.exception) std::rethrow_exception(result.exception); }
        catch (const HttpError& e) { message = std::string("HTTP error: ") + e.what(); diagnostic = "Turn failed with an HTTP error."; }
        catch (const AgentError& e) { message = std::string("Agent protocol error: ") + e.what(); diagnostic = "Turn failed with an agent protocol error."; }
        catch (const std::exception& e) { message = std::string("Agent error: ") + e.what(); diagnostic = "Turn failed with an agent error."; }
        catch (...) { message = "Agent failed with a non-standard exception."; diagnostic = "Turn failed with a non-standard exception."; }
        view_.add_notice(sanitize_terminal_line(message));
        view_.add_diagnostic(diagnostic);
        view_.set_status(event_details_incomplete_ ? "Turn failed; streamed details are incomplete." : "Turn failed; partial response is not complete.");
    }
    active_turn_id_.reset();
    sanitizer_ = TerminalTextSanitizer{};
    event_details_incomplete_ = false;
    view_.set_metrics(metrics_.snapshot());
    dirty_ = true;
    return true;
}

bool TuiEventLoop::handle_key(wint_t key, bool key_code)
{
    if (approval_request_) return handle_approval_key(key, key_code);
    if (key_code) {
        int terminal_rows = 0, terminal_columns = 0;
        getmaxyx(terminal_.window(), terminal_rows, terminal_columns);
        if (key == KEY_F(10)) { request_exit(); return true; }
        if (key == KEY_F(9)) {
            if (!TuiView::layout_supported(terminal_rows, terminal_columns)) {
                view_.set_status("Resize terminal to at least 60x20 before sending.");
            } else if (navigation_.can_submit()) submit_input();
            else view_.set_status("F9 sends only from Conversation.");
            return true;
        }
        if (key == KEY_MOUSE) {
            MEVENT event{};
            if (!terminal_.mouse_enabled() || getmouse(&event) != OK) return false;
            if (event.bstate & (BUTTON4_PRESSED | BUTTON5_PRESSED)) {
                bool changed = false;
                if (event.bstate & BUTTON4_PRESSED) changed = view_.scroll_page_up(navigation_.page()) || changed;
                if (event.bstate & BUTTON5_PRESSED) changed = view_.scroll_page_down(navigation_.page()) || changed;
                return changed;
            }
            if (event.bstate & BUTTON1_PRESSED) {
                if (event.y == 1) {
                    const int tab = view_.hit_test_tab(event.x, terminal_columns, navigation_.page(),
                                                       navigation_.focus() == TuiFocus::menu);
                    if (tab >= 0) {
                        if (static_cast<int>(navigation_.page()) == tab) return false;
                        navigation_.select_function_key(tab + 1);
                        return true;
                    }
                }
                if (navigation_.page() == TuiPage::conversation && view_.hit_test_composer(event.y, terminal_rows)) {
                    if (navigation_.focus() == TuiFocus::composer) return false;
                    navigation_.set_focus(TuiFocus::composer);
                    return true;
                }
                const int subsection = view_.hit_test_subsection(navigation_.page(), event.y);
                if (subsection >= 0) {
                    const int old_index = navigation_.subsection_index();
                    const auto old_focus = navigation_.focus();
                    const bool old_open = navigation_.subsection_open();
                    if (!navigation_.select_subsection(subsection)) return false;
                    navigation_.set_focus(TuiFocus::menu);
                    static_cast<void>(navigation_.activate_subsection());
                    return old_index != navigation_.subsection_index() || old_focus != navigation_.focus()
                        || old_open != navigation_.subsection_open();
                }
                if (!view_.hit_test_content(event.x, event.y, terminal_rows, terminal_columns,
                                            navigation_.page(), navigation_.subsection_index(),
                                            navigation_.subsection_open())) return false;
                if (navigation_.focus() == TuiFocus::content) return false;
                navigation_.set_focus(TuiFocus::content);
                return true;
            }
            return false;
        }
        for (int number = 1; number <= 6; ++number) {
            if (key == KEY_F(number)) {
                static_cast<void>(navigation_.select_function_key(number));
                return true;
            }
        }
        if (key == KEY_RESIZE) return true;
        if (key == KEY_PPAGE) return view_.scroll_page_up(navigation_.page());
        if (key == KEY_NPAGE) return view_.scroll_page_down(navigation_.page());
        if (key == KEY_UP || key == KEY_DOWN) {
            const int direction = key == KEY_UP ? -1 : 1;
            if (navigation_.page() == TuiPage::conversation && navigation_.focus() == TuiFocus::composer) {
                if (!preferred_composer_column_) {
                    preferred_composer_column_ = view_.composer_cursor_column(input_, cursor_byte_, terminal_columns);
                }
                cursor_byte_ = view_.move_composer_cursor_vertical(input_, cursor_byte_, direction,
                                                                    *preferred_composer_column_, terminal_columns);
                return true;
            }
            if (navigation_.move_selection(direction)) return true;
            if (navigation_.focus() == TuiFocus::content) {
                return direction < 0 ? view_.scroll_page_up(navigation_.page())
                                     : view_.scroll_page_down(navigation_.page());
            }
        }
        if (key == KEY_ENTER && navigation_.focus() == TuiFocus::menu) {
            return navigation_.activate_subsection();
        }
        if (key == KEY_HOME || key == KEY_END) {
            if (navigation_.page() == TuiPage::conversation && navigation_.focus() == TuiFocus::composer) {
                cursor_byte_ = view_.move_composer_cursor_to_row_edge(input_, cursor_byte_, key == KEY_END,
                                                                       terminal_columns);
                preferred_composer_column_.reset();
                return true;
            }
        }
        if (key == KEY_LEFT && cursor_byte_ > 0) { --cursor_byte_; while (cursor_byte_ > 0 && (static_cast<unsigned char>(input_[cursor_byte_]) & 0xc0) == 0x80) --cursor_byte_; preferred_composer_column_.reset(); return true; }
        if (key == KEY_RIGHT && cursor_byte_ < input_.size()) { ++cursor_byte_; while (cursor_byte_ < input_.size() && (static_cast<unsigned char>(input_[cursor_byte_]) & 0xc0) == 0x80) ++cursor_byte_; preferred_composer_column_.reset(); return true; }
        if (key == KEY_BACKSPACE || key == KEY_DC) {
            if (key == KEY_BACKSPACE && cursor_byte_ > 0) { auto start = cursor_byte_ - 1; while (start > 0 && (static_cast<unsigned char>(input_[start]) & 0xc0) == 0x80) --start; input_.erase(start, cursor_byte_ - start); cursor_byte_ = start; }
            else if (key == KEY_DC && cursor_byte_ < input_.size()) { auto end = cursor_byte_ + 1; while (end < input_.size() && (static_cast<unsigned char>(input_[end]) & 0xc0) == 0x80) ++end; input_.erase(cursor_byte_, end - cursor_byte_); }
            preferred_composer_column_.reset();
            return true;
        }
        return false;
    }
    if (key == 27) {
        if (!navigation_.close_subsection()) navigation_.escape_to_conversation();
        view_.set_status("Returned/back."); return true;
    }
    if (key == 9) {
        navigation_.cycle_focus();
        view_.set_status(navigation_.focus() == TuiFocus::composer ? "Composer focused." :
                         navigation_.focus() == TuiFocus::menu ? "View menu focused." : "View content focused.");
        return true;
    }
    if (key == KEY_UP) return view_.scroll_page_up(navigation_.page());
    if (key == KEY_DOWN) return view_.scroll_page_down(navigation_.page());
    if ((key == 10 || key == 13) && navigation_.focus() == TuiFocus::menu) {
        return navigation_.activate_subsection();
    }
    if ((key == 'm' || key == 'M') && navigation_.page() == TuiPage::conversation
        && navigation_.focus() == TuiFocus::content) {
        view_.set_markdown_enabled(!view_.markdown_enabled());
        view_.set_status(view_.markdown_enabled() ? "Formatted Markdown view." : "Plain text view; source content preserved.");
        return true;
    }
    if (navigation_.focus() != TuiFocus::composer || !navigation_.can_submit()) return false;
    if (key == 10 || key == 13) { input_.insert(cursor_byte_++, 1, '\n'); preferred_composer_column_.reset(); return true; }
    if (key >= 32 && key <= 0x10ffff) {
        wchar_t wc = static_cast<wchar_t>(key); char bytes[MB_LEN_MAX]{}; std::mbstate_t state{};
        const auto n = std::wcrtomb(bytes, wc, &state);
        if (n != static_cast<std::size_t>(-1)) { input_.insert(cursor_byte_, bytes, n); cursor_byte_ += n; preferred_composer_column_.reset(); return true; }
    }
    return false;
}

void TuiEventLoop::submit_input()
{
    if (active_turn_) { view_.set_status("A turn is already active."); return; }
    if (input_.empty()) { view_.set_status("Empty message not sent."); return; }
    const auto message = std::move(input_);
    input_.clear(); cursor_byte_ = 0;
    preferred_composer_column_.reset();
    view_.add_user_message(message);
    event_details_incomplete_ = false;
    metrics_.begin_pending_turn();
    view_.set_metrics(metrics_.snapshot());
    active_turn_id_.reset();
    const auto before = worker_.event_channel()->snapshot();
    dropped_events_at_submit_ = before.dropped_events;
    observer_failures_at_submit_ = before.observer_failures;
    active_turn_.emplace(worker_.submit(message));
    view_.set_status("Request submitted; waiting for agent events.");
}

void TuiEventLoop::request_exit()
{
    if (closing_) return;
    closing_ = true;
    approvals_->cancel_pending();
    worker_.request_close();
    if (active_turn_) view_.set_status("Waiting for active request/tool; cancellation is unavailable.");
    else view_.set_status("Closing.");
    dirty_ = true;
}

} // namespace agent::ui
