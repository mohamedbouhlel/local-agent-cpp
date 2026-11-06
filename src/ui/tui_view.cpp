#include "ui/tui_view.hpp"

#include <algorithm>
#include <cwchar>
#include <iterator>
#include <string>

namespace agent::ui {
namespace {
std::vector<std::string> lines(std::string_view text)
{
    std::vector<std::string> out;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const auto end = text.find('\n', begin);
        out.emplace_back(text.substr(begin, end == std::string_view::npos ? text.size() - begin : end - begin));
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return out;
}

int width_of(std::string_view text)
{
    std::mbstate_t state{};
    int width = 0;
    for (std::size_t i = 0; i < text.size();) {
        wchar_t wc{};
        const auto n = std::mbrtowc(&wc, text.data() + i, text.size() - i, &state);
        if (n == static_cast<std::size_t>(-1) || n == static_cast<std::size_t>(-2)) { ++i; ++width; state = {}; }
        else { width += std::max(0, ::wcwidth(wc)); i += n == 0 ? 1 : n; }
    }
    return width;
}

std::vector<std::string> wrap(std::string text, int columns)
{
    std::vector<std::string> result;
    if (columns <= 0) return result;
    std::mbstate_t state{};
    int width = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size();) {
        wchar_t wc{};
        const auto n = std::mbrtowc(&wc, text.data() + i, text.size() - i, &state);
        const std::size_t count = n == static_cast<std::size_t>(-1) || n == static_cast<std::size_t>(-2) ? 1 : (n == 0 ? 1 : n);
        const int glyph_width = n == static_cast<std::size_t>(-1) || n == static_cast<std::size_t>(-2) ? 1 : std::max(0, ::wcwidth(wc));
        if (width > 0 && width + glyph_width > columns) {
            result.emplace_back(text.substr(start, i - start));
            start = i;
            width = 0;
        }
        width += glyph_width;
        i += count;
    }
    result.emplace_back(text.substr(start));
    return result;
}
}

void TuiView::set_status(std::string_view status) { status_ = status; }
void TuiView::add_user_message(std::string text)
{
    const auto old_rows = conversation_rows(last_columns_, true).size();
    messages_.push_back({TuiMessageKind::user, std::move(text), false});
    active_assistant_.reset();
    preserve_anchor_after_content_change(old_rows);
}
void TuiView::append_assistant_delta(std::string_view text)
{
    if (!active_assistant_) {
        messages_.push_back({TuiMessageKind::assistant, {}, false});
        active_assistant_ = messages_.size() - 1;
    }
    const auto old_rows = conversation_rows(last_columns_, true).size();
    messages_[*active_assistant_].text.append(text);
    preserve_anchor_after_content_change(old_rows);
}
void TuiView::replace_assistant_response(std::string text)
{
    const auto old_rows = conversation_rows(last_columns_, true).size();
    if (!active_assistant_) {
        messages_.push_back({TuiMessageKind::assistant, std::move(text), false});
        active_assistant_ = messages_.size() - 1;
    } else {
        messages_[*active_assistant_].text = std::move(text);
    }
    messages_[*active_assistant_].incomplete = false;
    preserve_anchor_after_content_change(old_rows);
}
void TuiView::finish_assistant(bool complete)
{
    if (active_assistant_) messages_[*active_assistant_].incomplete = !complete;
    active_assistant_.reset();
}
void TuiView::add_notice(std::string_view text)
{
    const auto old_rows = conversation_rows(last_columns_, true).size();
    messages_.push_back({TuiMessageKind::notice, std::string(text), false});
    preserve_anchor_after_content_change(old_rows);
}
void TuiView::scroll_up()
{
    const auto row_count = conversation_rows(last_columns_, true).size();
    const auto maximum = row_count > static_cast<std::size_t>(last_chat_height_)
        ? row_count - static_cast<std::size_t>(last_chat_height_) : 0;
    if (maximum == 0) return;
    follow_bottom_ = false;
    scroll_offset_ = std::min(maximum, scroll_offset_ + 5);
}
void TuiView::scroll_down()
{
    scroll_offset_ = scroll_offset_ > 5 ? scroll_offset_ - 5 : 0;
    follow_bottom_ = scroll_offset_ == 0;
}
void TuiView::on_resize(int columns, int height)
{
    last_columns_ = std::max(1, columns);
    last_chat_height_ = std::max(1, height);
}
int TuiView::chat_height_for(int rows) const noexcept { return std::max(1, rows - 5); }
void TuiView::preserve_anchor_after_content_change(std::size_t previous_rows)
{
    if (follow_bottom_) return;
    const auto now = conversation_rows(last_columns_, true).size();
    scroll_offset_ += now > previous_rows ? now - previous_rows : 0;
}
std::vector<std::string> TuiView::conversation_rows(int columns, bool) const
{
    std::vector<std::string> result;
    const int usable = std::max(1, columns - 2);
    for (const auto& message : messages_) {
        const char* prefix = message.kind == TuiMessageKind::user ? "You: " :
                            message.kind == TuiMessageKind::assistant ? "Agent: " : "* ";
        const std::string label(prefix);
        for (const auto& line : lines(message.text)) {
            std::string row = label + line;
            if (message.incomplete) row += " [incomplete]";
            auto wrapped = wrap(std::move(row), usable);
            result.insert(result.end(), std::make_move_iterator(wrapped.begin()), std::make_move_iterator(wrapped.end()));
        }
    }
    return result;
}
void TuiView::draw(WINDOW* window, bool unicode, std::string_view input, std::size_t cursor)
{
    ++draw_count_;
    int rows = 0, cols = 0;
    getmaxyx(window, rows, cols);
    if (rows <= 0 || cols <= 0) return;
    werase(window);
    const std::string header = unicode ? " local-agent-cpp | F9 send  F10 quit  Tab focus  PgUp/PgDn scroll" : " local-agent-cpp | F9 send  F10 quit  Tab focus";
    (void)mvwaddstr(window, 0, 0, header.c_str());
    if (rows > 1) (void)mvwaddstr(window, 1, 0, status_.c_str());
    const int chat_top = std::min(2, rows - 1);
    const int composer_top = std::max(chat_top, rows - (rows >= 7 ? 4 : 1));
    const int chat_height = std::max(0, composer_top - chat_top);
    const auto content = conversation_rows(cols, unicode);
    if (follow_bottom_) scroll_offset_ = 0;
    else scroll_offset_ = std::min(scroll_offset_, content.size() > static_cast<std::size_t>(chat_height)
        ? content.size() - static_cast<std::size_t>(chat_height) : 0);
    const auto end = content.size() > scroll_offset_ ? content.size() - scroll_offset_ : 0;
    const auto begin = end > static_cast<std::size_t>(chat_height) ? end - static_cast<std::size_t>(chat_height) : 0;
    for (std::size_t i = begin; i < end && static_cast<int>(i - begin) < chat_height; ++i) {
        (void)mvwaddstr(window, chat_top + static_cast<int>(i - begin), 0, content[i].c_str());
    }
    if (rows >= 7) {
        (void)mvwaddstr(window, composer_top, 0, " Input (Enter newline, F9 submit):");
        const auto all = lines(input);
        const auto visible = std::max(1, rows - composer_top - 1);
        const auto first = all.size() > static_cast<std::size_t>(visible) ? all.size() - static_cast<std::size_t>(visible) : 0;
        for (std::size_t i = first; i < all.size() && composer_top + 1 + static_cast<int>(i - first) < rows; ++i)
            (void)mvwaddstr(window, composer_top + 1 + static_cast<int>(i - first), 0, all[i].c_str());
        const auto prefix = input.substr(0, std::min(cursor, input.size()));
        const auto line_start = prefix.rfind('\n');
        const auto current = line_start == std::string_view::npos ? prefix : prefix.substr(line_start + 1);
        const int y = std::min(rows - 1, composer_top + 1 + static_cast<int>(std::count(prefix.begin(), prefix.end(), '\n')) - static_cast<int>(first));
        (void)wmove(window, std::max(0, y), std::min(cols - 1, width_of(current)));
    } else if (rows >= 4) {
        const auto prefix = input.substr(0, std::min(cursor, input.size()));
        const auto prefix_lines = lines(prefix);
        const auto all = lines(input);
        const auto line_index = std::min(prefix_lines.size() - 1, all.size() - 1);
        (void)mvwaddstr(window, rows - 1, 0, all[line_index].c_str());
        (void)wmove(window, rows - 1, std::min(cols - 1, width_of(prefix_lines.back())));
    }
    (void)wnoutrefresh(window);
    (void)doupdate();
}

} // namespace agent::ui
