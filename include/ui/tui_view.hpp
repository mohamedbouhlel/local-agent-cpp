#pragma once

#include <cstddef>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <curses.h>
#include "ui/bounded_history.hpp"
#include "ui/tui_navigation.hpp"
#include "ui/tui_view_data.hpp"

namespace agent::ui {

enum class TuiMessageKind {
    user,
    assistant,
    notice,
};

struct TuiMessage {
    TuiMessageKind kind = TuiMessageKind::notice;
    std::string text;
    bool incomplete = false;
};

class TuiView final {
public:
    void set_status(std::string_view status);
    void add_user_message(std::string text);
    void append_assistant_delta(std::string_view safe_text);
    void replace_assistant_response(std::string safe_text);
    void finish_assistant(bool complete);
    void add_notice(std::string_view safe_text);
    void set_view_data(TuiViewData data);
    void add_tool_activity(std::string_view safe_text);
    void add_diagnostic(std::string_view safe_text);

    // Formatted transcript rows at the requested terminal width. Exposed so
    // wrapping and multiline role-label behavior can be tested deterministically.
    std::vector<std::string> rendered_conversation_rows(int columns) const;
    std::vector<std::string> rendered_page_rows(TuiPage page, int columns) const;
    std::string status_line(TuiPage page, TuiFocus focus, int columns) const;

    void scroll_up();
    void scroll_down();
    void scroll_page_up(TuiPage page);
    void scroll_page_down(TuiPage page);
    void on_resize(int columns, int chat_height);
    void draw(WINDOW* window, bool unicode_enabled, const TuiNavigation& navigation,
              std::string_view input, std::size_t cursor_byte);

    bool follows_bottom() const noexcept { return follow_bottom_; }
    std::size_t scroll_offset() const noexcept { return scroll_offset_; }
    std::size_t page_scroll_offset(TuiPage page) const noexcept { return page_scroll_offsets_[static_cast<std::size_t>(page)]; }
    const std::vector<TuiMessage>& messages() const noexcept { return messages_; }
    std::size_t draw_count() const noexcept { return draw_count_; }
    const BoundedHistory& tool_activity() const noexcept { return tool_activity_; }
    const BoundedHistory& diagnostics() const noexcept { return diagnostics_; }

private:
    int chat_height_for(int rows) const noexcept;
    void preserve_anchor_after_content_change(std::size_t previous_rows);
    void add_panel_history(BoundedHistory& history, TuiPage page, std::string text);

    std::vector<TuiMessage> messages_;
    TuiViewData view_data_;
    BoundedHistory tool_activity_;
    BoundedHistory diagnostics_;
    std::array<std::size_t, static_cast<std::size_t>(TuiPage::count)> page_scroll_offsets_{};
    std::array<bool, static_cast<std::size_t>(TuiPage::count)> page_follows_bottom_ = {true, true, true, true, true, true};
    std::optional<std::size_t> active_assistant_;
    std::string status_ = "Ready. Type a message; F9 sends it.";
    std::size_t scroll_offset_ = 0;
    bool follow_bottom_ = true;
    int last_columns_ = 80;
    int last_chat_height_ = 1;
    std::size_t draw_count_ = 0;
};

} // namespace agent::ui
