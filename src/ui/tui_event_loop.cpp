#include "ui/tui_event_loop.hpp"

#include "http_client.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <utility>

namespace agent::ui {

TuiEventLoop::TuiEventLoop(AgentWorker& worker, TerminalSession& terminal, TuiView& view)
    : worker_(worker), terminal_(terminal), view_(view)
{
}

int TuiEventLoop::run()
{
    WINDOW* window = terminal_.window();
    if (window == nullptr) throw std::runtime_error("terminal session has no screen");
    while (!closing_ || active_turn_) {
        bool changed = drain_events();
        changed = collect_completion() || changed;
        int rows = 0, cols = 0;
        getmaxyx(window, rows, cols);
        if (rows != last_rows_ || cols != last_columns_) {
            last_rows_ = rows; last_columns_ = cols;
            view_.on_resize(cols, std::max(1, rows - 6));
            view_.set_status("Terminal resized; compact layout active when space is limited.");
            changed = true;
        }
        if (changed || dirty_) { view_.draw(window, terminal_.unicode_enabled(), input_, cursor_byte_); dirty_ = false; }
        wint_t key{};
        const int rc = wget_wch(window, &key);
        if (rc != ERR && handle_key(key, rc == KEY_CODE_YES)) {
            dirty_ = true;
            if (closing_ && active_turn_) {
                view_.draw(window, terminal_.unicode_enabled(), input_, cursor_byte_);
                dirty_ = false;
            }
        }
        (void)collect_completion();
        if (closing_ && !active_turn_ && dirty_) {
            view_.draw(window, terminal_.unicode_enabled(), input_, cursor_byte_);
            dirty_ = false;
        }
    }
    worker_.request_close();
    worker_.join();
    return 0;
}

bool TuiEventLoop::drain_events()
{
    bool changed = false;
    AgentEvent event;
    const auto channel = worker_.event_channel();
    while (channel->try_pop(event)) { handle_event(event); changed = true; }
    const auto snapshot = channel->snapshot();
    if (active_turn_ && (snapshot.dropped_events > dropped_events_at_submit_
                         || snapshot.observer_failures > observer_failures_at_submit_)) {
        event_details_incomplete_ = true;
        view_.set_status("Event details incomplete: the event queue reported lost details.");
        changed = true;
    }
    return changed;
}

void TuiEventLoop::handle_event(const AgentEvent& event)
{
    if (!presentation_state_desynchronized_) {
        const auto applied = presentation_state_.apply(event);
        if (!applied.applied()) {
            presentation_state_desynchronized_ = true;
            // A new RequestStarted after a previous dropped turn is a valid
            // resynchronization boundary. The worker's FIFO still preserves
            // order within this new turn; its completion independently says
            // whether this turn lost details.
            const bool turn_boundary = std::holds_alternative<RequestStarted>(event.payload) && !active_turn_id_;
            if (!turn_boundary) {
                event_details_incomplete_ = true;
                view_.set_status("Event details incomplete: inconsistent or missing event sequence.");
            }
        }
    }
    if (!active_turn_id_) active_turn_id_ = event.metadata.turn_id;
    if (event.metadata.turn_id != active_turn_id_) {
        event_details_incomplete_ = true;
        view_.set_status("Event details incomplete: event correlation mismatch.");
        return;
    }
    std::visit([this](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, RequestStarted>) view_.set_status("Request submitted.");
        else if constexpr (std::is_same_v<T, HttpRequestStarted>) view_.set_status("Waiting for model response…");
        else if constexpr (std::is_same_v<T, TextDelta>) {
            const auto safe = sanitizer_.feed(payload.text);
            if (!safe.empty()) view_.append_assistant_delta(safe);
            view_.set_status("Receiving model stream…");
        } else if constexpr (std::is_same_v<T, ToolStarted>) {
            view_.add_notice("Running tool: " + sanitize_terminal_line(payload.name));
            view_.set_status("Tool running.");
        } else if constexpr (std::is_same_v<T, ToolSucceeded>) {
            view_.add_notice("Tool completed: " + sanitize_terminal_line(payload.name));
            view_.set_status("Tool completed; waiting for model.");
        } else if constexpr (std::is_same_v<T, ToolFailed>) {
            view_.add_notice("Tool failed: " + sanitize_terminal_line(payload.name));
            view_.set_status("Tool failed; agent may continue.");
        } else if constexpr (std::is_same_v<T, RequestFailed>) view_.set_status("HTTP/protocol request failed.");
        else if constexpr (std::is_same_v<T, TurnFinished>) view_.set_status(payload.outcome == TurnOutcome::completed ? "Turn completed technically; task success is not verified." : "Turn failed.");
        else if constexpr (std::is_same_v<T, HttpRequestFinished>) view_.set_status("HTTP transport finished; model turn may continue.");
    }, event.payload);
}

bool TuiEventLoop::collect_completion()
{
    if (!active_turn_ || active_turn_->wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return false;
    auto result = active_turn_->get();
    active_turn_.reset();
    event_details_incomplete_ = event_details_incomplete_ || result.event_details_incomplete();
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
        try { if (result.exception) std::rethrow_exception(result.exception); }
        catch (const HttpError& e) { message = std::string("HTTP error: ") + e.what(); }
        catch (const AgentError& e) { message = std::string("Agent protocol error: ") + e.what(); }
        catch (const std::exception& e) { message = std::string("Agent error: ") + e.what(); }
        catch (...) { message = "Agent failed with a non-standard exception."; }
        view_.add_notice(sanitize_terminal_line(message));
        view_.set_status(event_details_incomplete_ ? "Turn failed; streamed details are incomplete." : "Turn failed; partial response is not complete.");
    }
    active_turn_id_.reset();
    sanitizer_ = TerminalTextSanitizer{};
    event_details_incomplete_ = false;
    dirty_ = true;
    return true;
}

bool TuiEventLoop::handle_key(wint_t key, bool key_code)
{
    if (key_code) {
        if (key == KEY_F(10) || key == 27) { request_exit(); return true; }
        if (key == KEY_F(9)) { submit_input(); return true; }
        if (key == KEY_RESIZE) return true;
        if (key == KEY_PPAGE) { view_.scroll_up(); return true; }
        if (key == KEY_NPAGE) { view_.scroll_down(); return true; }
        if (key == KEY_LEFT && cursor_byte_ > 0) { --cursor_byte_; while (cursor_byte_ > 0 && (static_cast<unsigned char>(input_[cursor_byte_]) & 0xc0) == 0x80) --cursor_byte_; return true; }
        if (key == KEY_RIGHT && cursor_byte_ < input_.size()) { ++cursor_byte_; while (cursor_byte_ < input_.size() && (static_cast<unsigned char>(input_[cursor_byte_]) & 0xc0) == 0x80) ++cursor_byte_; return true; }
        if (key == KEY_BACKSPACE || key == KEY_DC) {
            if (key == KEY_BACKSPACE && cursor_byte_ > 0) { auto start = cursor_byte_ - 1; while (start > 0 && (static_cast<unsigned char>(input_[start]) & 0xc0) == 0x80) --start; input_.erase(start, cursor_byte_ - start); cursor_byte_ = start; }
            else if (key == KEY_DC && cursor_byte_ < input_.size()) { auto end = cursor_byte_ + 1; while (end < input_.size() && (static_cast<unsigned char>(input_[end]) & 0xc0) == 0x80) ++end; input_.erase(cursor_byte_, end - cursor_byte_); }
            return true;
        }
        return false;
    }
    if (key == 27) { request_exit(); return true; }
    if (key == 9) { composer_focused_ = !composer_focused_; view_.set_status(composer_focused_ ? "Composer focused." : "Navigation focused; PgUp/PgDn scroll."); return true; }
    if (!composer_focused_) return false;
    if (key == 10 || key == 13) { input_.insert(cursor_byte_++, 1, '\n'); return true; }
    if (key >= 32 && key <= 0x10ffff) {
        wchar_t wc = static_cast<wchar_t>(key); char bytes[MB_LEN_MAX]{}; std::mbstate_t state{};
        const auto n = std::wcrtomb(bytes, wc, &state);
        if (n != static_cast<std::size_t>(-1)) { input_.insert(cursor_byte_, bytes, n); cursor_byte_ += n; return true; }
    }
    return false;
}

void TuiEventLoop::submit_input()
{
    if (active_turn_) { view_.set_status("A turn is already active."); return; }
    if (input_.empty()) { view_.set_status("Empty message not sent."); return; }
    const auto message = std::move(input_);
    input_.clear(); cursor_byte_ = 0;
    view_.add_user_message(message);
    event_details_incomplete_ = false;
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
    worker_.request_close();
    if (active_turn_) view_.set_status("Waiting for active request/tool; cancellation is unavailable.");
    else view_.set_status("Closing.");
    dirty_ = true;
}

} // namespace agent::ui
