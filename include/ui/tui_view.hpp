#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <curses.h>

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

    void scroll_up();
    void scroll_down();
    void on_resize(int columns, int chat_height);
    void draw(WINDOW* window, bool unicode_enabled, std::string_view input, std::size_t cursor_byte);

    bool follows_bottom() const noexcept { return follow_bottom_; }
    std::size_t scroll_offset() const noexcept { return scroll_offset_; }
    const std::vector<TuiMessage>& messages() const noexcept { return messages_; }
    std::size_t draw_count() const noexcept { return draw_count_; }

private:
    std::vector<std::string> conversation_rows(int columns, bool unicode_enabled) const;
    int chat_height_for(int rows) const noexcept;
    void preserve_anchor_after_content_change(std::size_t previous_rows);

    std::vector<TuiMessage> messages_;
    std::optional<std::size_t> active_assistant_;
    std::string status_ = "Ready. Type a message; F9 sends it.";
    std::size_t scroll_offset_ = 0;
    bool follow_bottom_ = true;
    int last_columns_ = 80;
    int last_chat_height_ = 1;
    std::size_t draw_count_ = 0;
};

} // namespace agent::ui
