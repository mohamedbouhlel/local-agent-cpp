#include "ui/tui_view.hpp"

#include "ui/terminal_text_sanitizer.hpp"

#include <algorithm>
#include <cwchar>
#include <iterator>
#include <string>
#include <utility>

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

std::vector<std::string> wrap(std::string_view text,
                              std::string_view first_prefix,
                              std::string_view continuation_prefix,
                              int columns)
{
    std::vector<std::string> result;
    if (columns <= 0) return result;
    std::mbstate_t state{};
    std::string row(first_prefix);
    int width = width_of(first_prefix);
    for (std::size_t i = 0; i < text.size();) {
        wchar_t wc{};
        const auto n = std::mbrtowc(&wc, text.data() + i, text.size() - i, &state);
        const std::size_t count = n == static_cast<std::size_t>(-1) || n == static_cast<std::size_t>(-2) ? 1 : (n == 0 ? 1 : n);
        const int glyph_width = n == static_cast<std::size_t>(-1) || n == static_cast<std::size_t>(-2) ? 1 : std::max(0, ::wcwidth(wc));
        if (width > 0 && width + glyph_width > columns) {
            result.push_back(std::move(row));
            row.assign(continuation_prefix);
            width = width_of(continuation_prefix);
        }
        width += glyph_width;
        row.append(text.substr(i, count));
        i += count;
    }
    result.push_back(std::move(row));
    return result;
}
}

void TuiView::set_status(std::string_view status) { status_ = status; }
void TuiView::add_user_message(std::string text)
{
    const auto old_rows = rendered_conversation_rows(last_columns_).size();
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
    const auto old_rows = rendered_conversation_rows(last_columns_).size();
    messages_[*active_assistant_].text.append(text);
    preserve_anchor_after_content_change(old_rows);
}
void TuiView::replace_assistant_response(std::string text)
{
    const auto old_rows = rendered_conversation_rows(last_columns_).size();
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
    const auto old_rows = rendered_conversation_rows(last_columns_).size();
    messages_.push_back({TuiMessageKind::notice, std::string(text), false});
    preserve_anchor_after_content_change(old_rows);
}
void TuiView::set_view_data(TuiViewData data) { view_data_ = std::move(data); }
void TuiView::add_tool_activity(std::string_view text)
{
    tool_activity_.push_back(sanitize_terminal_line(text));
}
void TuiView::add_diagnostic(std::string_view text)
{
    diagnostics_.push_back(sanitize_terminal_line(text));
}
void TuiView::scroll_up()
{
    const auto row_count = rendered_conversation_rows(last_columns_).size();
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
void TuiView::scroll_page_up(TuiPage page)
{
    if (page == TuiPage::conversation) { scroll_up(); return; }
    const auto index = static_cast<std::size_t>(page);
    const auto rows = rendered_page_rows(page, last_columns_).size();
    const auto visible = static_cast<std::size_t>(std::max(1, last_chat_height_));
    const auto maximum = rows > visible ? rows - visible : 0;
    page_scroll_offsets_[index] = std::min(maximum, page_scroll_offsets_[index] + 5);
    page_follows_bottom_[index] = page_scroll_offsets_[index] == 0;
}
void TuiView::scroll_page_down(TuiPage page)
{
    if (page == TuiPage::conversation) { scroll_down(); return; }
    const auto index = static_cast<std::size_t>(page);
    page_scroll_offsets_[index] = page_scroll_offsets_[index] > 5 ? page_scroll_offsets_[index] - 5 : 0;
    page_follows_bottom_[index] = page_scroll_offsets_[index] == 0;
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
    const auto now = rendered_conversation_rows(last_columns_).size();
    scroll_offset_ += now > previous_rows ? now - previous_rows : 0;
}
std::vector<std::string> TuiView::rendered_conversation_rows(int columns) const
{
    std::vector<std::string> result;
    if (messages_.empty()) return {"No messages yet. Type a message below."};
    const int usable = std::max(1, columns - 2);
    for (const auto& message : messages_) {
        const std::string prefix = message.kind == TuiMessageKind::user ? "You: " :
                                   message.kind == TuiMessageKind::assistant ? "Agent: " : "* ";
        const std::string continuation(prefix.size(), ' ');
        const auto message_lines = lines(message.text);
        for (std::size_t line_index = 0; line_index < message_lines.size(); ++line_index) {
            const auto first_prefix = line_index == 0 ? std::string_view(prefix)
                                                       : std::string_view(continuation);
            auto wrapped = wrap(message_lines[line_index], first_prefix, continuation, usable);
            result.insert(result.end(), std::make_move_iterator(wrapped.begin()), std::make_move_iterator(wrapped.end()));
        }
        if (message.incomplete) result.push_back(continuation + "[incomplete]");
    }
    return result;
}
std::vector<std::string> TuiView::rendered_page_rows(TuiPage page, int columns) const
{
    if (page == TuiPage::conversation) return rendered_conversation_rows(columns);
    std::vector<std::string> source;
    switch (page) {
    case TuiPage::tools:
        source.emplace_back("Declared tools:");
        if (view_data_.tools.empty()) source.emplace_back("No tools are registered.");
        for (const auto& tool : view_data_.tools) {
            source.push_back(sanitize_terminal_line(tool.name) + " - " + sanitize_terminal_line(tool.description));
        }
        source.emplace_back("Observed tool activity:");
        if (tool_activity_.empty()) source.emplace_back("No tool execution observed in this session.");
        source.insert(source.end(), tool_activity_.begin(), tool_activity_.end());
        break;
    case TuiPage::metrics:
        source.emplace_back("Métriques indisponibles — MetricCollector non intégré");
        break;
    case TuiPage::diagnostics:
        source.emplace_back("Observed diagnostics:");
        if (diagnostics_.empty()) source.emplace_back("No diagnostic event observed in this session.");
        source.insert(source.end(), diagnostics_.begin(), diagnostics_.end());
        break;
    case TuiPage::configuration:
        source.emplace_back("Read-only configuration:");
        source.push_back("Model: " + sanitize_terminal_line(view_data_.model));
        source.push_back("Endpoint: " + sanitize_terminal_line(view_data_.endpoint_origin));
        source.push_back("Timeout: " + std::to_string(view_data_.timeout_seconds) + " seconds");
        source.push_back(std::string("API key: ") + (view_data_.api_key_configured ? "configured (value hidden)" : "not configured"));
        source.emplace_back("Workspace: unavailable in this view.");
        break;
    case TuiPage::help:
        source.emplace_back("F1-F6  Select Conversation, Tools, Metrics, Diagnostics, Configuration, Help");
        source.emplace_back("Escape Return to Conversation");
        source.emplace_back("Tab    Cycle focus");
        source.emplace_back("F9     Send from Conversation only");
        source.emplace_back("F10    Quit; active request may finish first");
        source.emplace_back("Enter  Insert newline in the composer");
        source.emplace_back("PgUp/PgDn Scroll the current view");
        break;
    case TuiPage::conversation:
    case TuiPage::count:
        break;
    }

    std::vector<std::string> result;
    const int usable = std::max(1, columns - 1);
    for (const auto& line : source) {
        auto wrapped = wrap(line, "", "", usable);
        result.insert(result.end(), std::make_move_iterator(wrapped.begin()), std::make_move_iterator(wrapped.end()));
    }
    return result;
}

void TuiView::draw(WINDOW* window, bool unicode, const TuiNavigation& navigation,
                   std::string_view input, std::size_t cursor)
{
    ++draw_count_;
    int rows = 0, cols = 0;
    getmaxyx(window, rows, cols);
    if (rows <= 0 || cols <= 0) return;
    werase(window);
    const auto page = navigation.page();
    const std::string active = std::to_string(static_cast<int>(page) + 1) + "/6 " + TuiNavigation::short_page_name(page);
    std::string menu;
    if (cols >= 74) {
        menu = "F1 Chat  F2 Tools  F3 Metrics  F4 Diagnostics  F5 Config  F6 Help";
    } else if (cols >= 48) {
        menu = "F1 Chat F2 Tools F3 Metrics F4 Diag F5 Config F6 Help";
    } else {
        menu = "F1-F6 " + active;
    }
    if (navigation.focus() == TuiFocus::menu) menu = "> " + menu;
    (void)mvwaddnstr(window, 0, 0, menu.c_str(), std::max(0, cols));
    const char* focus_label = navigation.focus() == TuiFocus::composer ? "INPUT" :
                              navigation.focus() == TuiFocus::menu ? "MENU" : "VIEW";
    const std::string status = active + " [" + focus_label + "] " + status_;
    if (rows > 1) (void)mvwaddnstr(window, 1, 0, status.c_str(), std::max(0, cols));
    const int chat_top = std::min(2, rows - 1);
    const bool conversation = page == TuiPage::conversation;
    const int composer_top = conversation ? std::max(chat_top, rows - (rows >= 7 ? 4 : 1)) : rows;
    const int chat_height = std::max(0, composer_top - chat_top);
    auto content = rendered_page_rows(page, cols);
    if (!unicode && page == TuiPage::metrics) {
        content = {"Metriques indisponibles - MetricCollector non integre"};
    }
    auto& offset = conversation ? scroll_offset_ : page_scroll_offsets_[static_cast<std::size_t>(page)];
    const bool follows = conversation ? follow_bottom_ : page_follows_bottom_[static_cast<std::size_t>(page)];
    if (follows) offset = 0;
    else offset = std::min(offset, content.size() > static_cast<std::size_t>(chat_height)
        ? content.size() - static_cast<std::size_t>(chat_height) : 0);
    const auto end = content.size() > offset ? content.size() - offset : 0;
    const auto begin = end > static_cast<std::size_t>(chat_height) ? end - static_cast<std::size_t>(chat_height) : 0;
    for (std::size_t i = begin; i < end && static_cast<int>(i - begin) < chat_height; ++i) {
        (void)mvwaddstr(window, chat_top + static_cast<int>(i - begin), 0, content[i].c_str());
    }
    if (conversation && rows >= 7) {
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
    } else if (conversation && rows >= 4) {
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
