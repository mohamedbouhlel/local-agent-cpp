#include "ui/tui_view.hpp"

#include "ui/terminal_text_sanitizer.hpp"

#include <algorithm>
#include <cwchar>
#include <iterator>
#include <limits>
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

struct ComposerLayout {
    std::vector<std::string> rows{1};
    std::vector<std::pair<std::size_t, int>> cursor_positions;
    std::vector<std::vector<std::pair<int, std::size_t>>> boundaries{{{0, 0}}};
};

ComposerLayout layout_composer(std::string_view input, int columns)
{
    const int usable = std::max(1, columns - 1);
    ComposerLayout layout;
    layout.cursor_positions.resize(input.size() + 1);
    std::size_t row = 0;
    int width = 0;
    std::mbstate_t state{};
    layout.cursor_positions[0] = {0, 0};

    for (std::size_t i = 0; i < input.size();) {
        if (input[i] == '\n') {
            layout.cursor_positions[i] = {row, width};
            ++row;
            layout.rows.emplace_back();
            layout.boundaries.push_back({{0, i + 1}});
            width = 0;
            state = {};
            layout.cursor_positions[i + 1] = {row, 0};
            ++i;
            continue;
        }

        wchar_t wc{};
        const auto decoded = std::mbrtowc(&wc, input.data() + i, input.size() - i, &state);
        const bool invalid = decoded == static_cast<std::size_t>(-1) || decoded == static_cast<std::size_t>(-2);
        const std::size_t count = invalid ? 1 : (decoded == 0 ? 1 : decoded);
        const int glyph_width = invalid ? 1 : std::max(0, ::wcwidth(wc));
        if (width > 0 && width + glyph_width > usable) {
            ++row;
            layout.rows.emplace_back();
            layout.boundaries.push_back({{0, i}});
            width = 0;
            layout.cursor_positions[i] = {row, 0};
        }
        layout.rows[row].append(input.substr(i, count));
        width += glyph_width;
        i += count;
        if (invalid) state = {};
        layout.boundaries[row].push_back({width, i});
        layout.cursor_positions[i] = {row, width};
    }
    return layout;
}

std::size_t byte_at_column(const std::vector<std::pair<int, std::size_t>>& boundaries, int column)
{
    std::size_t result = boundaries.empty() ? 0 : boundaries.front().second;
    for (const auto& [cell, byte] : boundaries) {
        if (cell > column) break;
        result = byte;
    }
    return result;
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

std::vector<std::string> wrap_inspection(std::string_view text, int columns)
{
    const int width = std::max(1, columns);
    std::vector<std::string> rows;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        const auto line_end = end == std::string_view::npos ? text.size() : end;
        if (line_end == start) rows.emplace_back();
        for (std::size_t p = start; p < line_end;) {
            const auto count = std::min<std::size_t>(static_cast<std::size_t>(width), line_end - p);
            rows.emplace_back(text.substr(p, count));
            p += count;
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return rows;
}

std::vector<std::string> approval_rows(const WriteApprovalRequest& request, int columns)
{
    std::string source = "Workspace: " + escape_write_content_for_inspection(request.workspace_root.string()) + "\n";
    source += "Target: " + escape_write_content_for_inspection(request.relative_path.generic_string()) + "\n";
    source += "Operation: create new file\nSize: " + std::to_string(request.content_bytes) + " bytes\n";
    source += "Parents to create:";
    if (request.missing_parent_directories.empty()) source += " none";
    for (const auto& parent : request.missing_parent_directories)
        source += " " + escape_write_content_for_inspection(parent.generic_string());
    source += "\nContent (ASCII escaped; line breaks shown as \\n):\n";
    source += escape_write_content_for_inspection(request.content);
    return wrap_inspection(source, std::max(1, columns - 2));
}

std::string retention_warning(std::uint64_t count, std::string_view singular,
                              std::string_view plural, std::string_view singular_suffix,
                              std::string_view plural_suffix)
{
    return "Historique partiel : " + std::to_string(count) + " "
        + std::string(count == 1 ? singular : plural)
        + std::string(count == 1 ? singular_suffix : plural_suffix);
}

std::size_t retention_notice_rows(std::uint64_t evicted, std::uint64_t rejected, int columns)
{
    const int usable = std::max(1, columns - 1);
    std::size_t rows = 0;
    if (evicted != 0) {
        rows += wrap(retention_warning(evicted, "entrée", "entrées", " évincée", " évincées"), "", "", usable).size();
    }
    if (rejected != 0) {
        rows += wrap(retention_warning(rejected, "entrée", "entrées", " trop volumineuse non retenue",
                                       " trop volumineuses non retenues"), "", "", usable).size();
    }
    return rows;
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
    add_panel_history(tool_activity_, TuiPage::tools, sanitize_terminal_line(text));
}
void TuiView::add_diagnostic(std::string_view text)
{
    add_panel_history(diagnostics_, TuiPage::diagnostics, sanitize_terminal_line(text));
}

void TuiView::add_panel_history(BoundedHistory& history, TuiPage page, std::string text)
{
    const int usable = std::max(1, last_columns_ - 1);
    const auto appended_rows = wrap(text, "", "", usable).size();
    const auto old_rows = rendered_page_rows(page, last_columns_);
    const auto index = static_cast<std::size_t>(page);
    const bool follows = page_follows_bottom_[index];
    const auto old_offset = std::min(page_scroll_offsets_[index],
        old_rows.size() > static_cast<std::size_t>(last_chat_height_)
            ? old_rows.size() - static_cast<std::size_t>(last_chat_height_) : std::size_t{0});
    const auto visible_end = old_rows.size() > old_offset ? old_rows.size() - old_offset : std::size_t{0};
    const auto visible_begin = visible_end > static_cast<std::size_t>(last_chat_height_)
        ? visible_end - static_cast<std::size_t>(last_chat_height_) : std::size_t{0};

    std::size_t history_begin = wrap(page == TuiPage::tools ? "Declared tools:" : "Observed diagnostics:",
                                     "", "", usable).size();
    if (page == TuiPage::tools) {
        if (view_data_.tools.empty()) {
            history_begin += wrap("No tools are registered.", "", "", usable).size();
        }
        for (const auto& tool : view_data_.tools) {
            history_begin += wrap(sanitize_terminal_line(tool.name) + " - "
                                      + sanitize_terminal_line(tool.description), "", "", usable).size();
        }
        history_begin += wrap("Observed tool activity:", "", "", usable).size();
    }

    std::vector<std::size_t> old_entry_rows;
    old_entry_rows.reserve(history.size());
    for (const auto& entry : history.entries()) {
        old_entry_rows.push_back(wrap(entry, "", "", usable).size());
    }

    const auto old_evicted = history.evicted_count();
    const auto old_rejected = history.rejected_too_large_count();
    const auto result = history.add(std::move(text));
    if (follows) return;

    bool visible_entry_evicted = false;
    std::size_t evicted_rows = 0;
    for (std::size_t i = 0; i < result.evicted_entries; ++i) evicted_rows += old_entry_rows[i];
    if (evicted_rows != 0) {
        const auto evicted_begin = history_begin;
        const auto evicted_end = history_begin + evicted_rows;
        visible_entry_evicted = visible_begin < evicted_end && evicted_begin < visible_end;
    }

    if (visible_entry_evicted) {
        const auto new_rows = rendered_page_rows(page, last_columns_).size();
        const auto visible = static_cast<std::size_t>(last_chat_height_);
        page_scroll_offsets_[index] = new_rows > visible ? new_rows - visible : 0;
        page_follows_bottom_[index] = false;
        return;
    }

    const auto max = std::numeric_limits<std::size_t>::max();
    const auto old_notice_rows = retention_notice_rows(old_evicted, old_rejected, last_columns_);
    const auto new_notice_rows = retention_notice_rows(history.evicted_count(),
                                                       history.rejected_too_large_count(), last_columns_);
    const std::size_t added_rows = (result.retained ? appended_rows : 0U)
        + (new_notice_rows - old_notice_rows);
    page_scroll_offsets_[index] = added_rows > max - page_scroll_offsets_[index]
        ? max : page_scroll_offsets_[index] + added_rows;
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
        source.insert(source.end(), tool_activity_.entries().begin(), tool_activity_.entries().end());
        if (tool_activity_.evicted_count() != 0) {
            source.push_back(retention_warning(tool_activity_.evicted_count(), "entrée", "entrées", " évincée", " évincées"));
        }
        if (tool_activity_.rejected_too_large_count() != 0) {
            source.push_back(retention_warning(tool_activity_.rejected_too_large_count(), "entrée", "entrées",
                                               " trop volumineuse non retenue", " trop volumineuses non retenues"));
        }
        break;
    case TuiPage::metrics:
        source.emplace_back("Métriques indisponibles — MetricCollector non intégré");
        break;
    case TuiPage::diagnostics:
        source.emplace_back("Observed diagnostics:");
        if (diagnostics_.empty()) source.emplace_back("No diagnostic event observed in this session.");
        source.insert(source.end(), diagnostics_.entries().begin(), diagnostics_.entries().end());
        if (diagnostics_.evicted_count() != 0) {
            source.push_back(retention_warning(diagnostics_.evicted_count(), "entrée", "entrées", " évincée", " évincées"));
        }
        if (diagnostics_.rejected_too_large_count() != 0) {
            source.push_back(retention_warning(diagnostics_.rejected_too_large_count(), "entrée", "entrées",
                                               " trop volumineuse non retenue", " trop volumineuses non retenues"));
        }
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

TuiComposerRender TuiView::rendered_composer(std::string_view input, std::size_t cursor_byte,
                                             int columns, int visible_rows) const
{
    TuiComposerRender result;
    if (visible_rows <= 0) return result;
    const auto layout = layout_composer(input, columns);
    if (layout.rows.empty() || layout.cursor_positions.empty()) return result;
    const auto [cursor_row, cursor_column] = layout.cursor_positions[std::min(cursor_byte, input.size())];
    const auto visible = static_cast<std::size_t>(visible_rows);
    result.first_visual_row = cursor_row + 1 > visible ? cursor_row + 1 - visible : 0;
    const auto end = std::min(layout.rows.size(), result.first_visual_row + visible);
    result.rows.assign(layout.rows.begin() + static_cast<std::ptrdiff_t>(result.first_visual_row),
                       layout.rows.begin() + static_cast<std::ptrdiff_t>(end));
    result.cursor_visual_row = cursor_row - result.first_visual_row;
    result.cursor_column = std::min(cursor_column, std::max(0, columns - 2));
    return result;
}

int TuiView::composer_cursor_column(std::string_view input, std::size_t cursor_byte, int columns) const
{
    const auto layout = layout_composer(input, columns);
    if (layout.cursor_positions.empty()) return 0;
    return layout.cursor_positions[std::min(cursor_byte, input.size())].second;
}

std::size_t TuiView::move_composer_cursor_vertical(std::string_view input, std::size_t cursor_byte,
                                                  int direction, int desired_column, int columns) const
{
    const auto layout = layout_composer(input, columns);
    if (layout.cursor_positions.empty() || direction == 0) return std::min(cursor_byte, input.size());
    const auto row = layout.cursor_positions[std::min(cursor_byte, input.size())].first;
    const auto target = direction < 0 ? (row == 0 ? 0 : row - 1)
                                      : std::min(row + 1, layout.rows.size() - 1);
    return byte_at_column(layout.boundaries[target], std::max(0, desired_column));
}

std::size_t TuiView::move_composer_cursor_to_row_edge(std::string_view input, std::size_t cursor_byte,
                                                     bool to_end, int columns) const
{
    const auto layout = layout_composer(input, columns);
    if (layout.cursor_positions.empty()) return 0;
    const auto row = layout.cursor_positions[std::min(cursor_byte, input.size())].first;
    const auto& boundaries = layout.boundaries[row];
    if (boundaries.empty()) return std::min(cursor_byte, input.size());
    return to_end ? boundaries.back().second : boundaries.front().second;
}

std::string TuiView::status_line(TuiPage page, TuiFocus focus, int columns) const
{
    const BoundedHistory* history = page == TuiPage::tools ? &tool_activity_
        : page == TuiPage::diagnostics ? &diagnostics_ : nullptr;
    std::string label;
    if (history && (history->evicted_count() != 0 || history->rejected_too_large_count() != 0)) {
        label = "PARTIAL";
        if (columns >= 10 && history->evicted_count() != 0) {
            label += " E" + std::to_string(history->evicted_count());
        }
        if (columns >= 10 && history->rejected_too_large_count() != 0) {
            label += " R" + std::to_string(history->rejected_too_large_count());
        }
    }
    const std::string active = std::to_string(static_cast<int>(page) + 1) + "/6 "
        + TuiNavigation::short_page_name(page);
    const char* focus_label = focus == TuiFocus::composer ? "INPUT" :
                              focus == TuiFocus::menu ? "MENU" : "VIEW";
    return label.empty() ? active + " [" + focus_label + "] " + status_
                         : label + " " + active + " [" + focus_label + "] " + status_;
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
    const std::string status = status_line(page, navigation.focus(), cols);
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
        const auto visible = std::max(1, rows - composer_top - 1);
        const auto composer = rendered_composer(input, cursor, cols, visible);
        for (std::size_t i = 0; i < composer.rows.size(); ++i) {
            const int y = composer_top + 1 + static_cast<int>(i);
            if (y < rows) (void)mvwaddstr(window, y, 0, composer.rows[i].c_str());
        }
        const int y = composer_top + 1 + static_cast<int>(composer.cursor_visual_row);
        (void)wmove(window, std::clamp(y, 0, rows - 1), std::clamp(composer.cursor_column, 0, cols - 1));
    } else if (conversation && rows >= 4) {
        const auto composer = rendered_composer(input, cursor, cols, 1);
        if (!composer.rows.empty()) (void)mvwaddstr(window, rows - 1, 0, composer.rows.front().c_str());
        (void)wmove(window, rows - 1, std::clamp(composer.cursor_column, 0, cols - 1));
    }
    (void)wnoutrefresh(window);
    (void)doupdate();
}

bool TuiView::approval_inspectable(int rows, int columns) noexcept
{
    // The full challenge and the complete typed response must remain visible.
    return rows >= 6 && columns >= 48;
}

std::size_t TuiView::approval_content_rows(int columns, const WriteApprovalRequest& request) const
{
    return approval_rows(request, std::max(1, columns)).size();
}

void TuiView::draw_approval(WINDOW* window, const WriteApprovalRequest& request, std::size_t scroll_offset,
                            std::string_view challenge, std::string_view confirmation_input, bool expired)
{
    int rows = 0, cols = 0;
    getmaxyx(window, rows, cols);
    if (rows <= 0 || cols <= 0) return;
    werase(window);
    (void)mvwaddnstr(window, 0, 0, "WRITE APPROVAL | arrows | Enter | F10", std::max(0, cols));
    if (expired) {
        if (rows > 2) (void)mvwaddnstr(window, rows - 2, 0, "Approval expired; no file was created.", std::max(0, cols));
        if (rows > 1) (void)mvwaddnstr(window, rows - 1, 0, "Enter/Esc return; F10 quit.", std::max(0, cols));
        (void)wnoutrefresh(window);
        (void)doupdate();
        return;
    }
    if (!approval_inspectable(rows, cols)) {
        if (rows > 2) (void)mvwaddnstr(window, 2, 0, "Too small: write approval unavailable.", std::max(0, cols));
        if (rows > 3) (void)mvwaddnstr(window, rows - 1, 0, "Resize / F10 cancel", std::max(0, cols));
        (void)wnoutrefresh(window);
        (void)doupdate();
        return;
    }

    const auto content = approval_rows(request, cols);
    const auto available = static_cast<std::size_t>(std::max(0, rows - 3));
    const auto max_scroll = content.size() > available ? content.size() - available : 0;
    const auto first = std::min(scroll_offset, max_scroll);
    const auto last = std::min(content.size(), first + available);
    for (std::size_t i = first; i < last; ++i) {
        (void)mvwaddnstr(window, 1 + static_cast<int>(i - first), 0, content[i].c_str(), std::max(0, cols));
    }
    const auto challenge_line = "Confirm: " + approval_confirmation(challenge);
    (void)mvwaddnstr(window, rows - 2, 0, challenge_line.c_str(), std::max(0, cols));
    const auto input_line = "Input: " + std::string(confirmation_input);
    (void)mvwaddnstr(window, rows - 1, 0, input_line.c_str(), std::max(0, cols));
    (void)wnoutrefresh(window);
    (void)doupdate();
}

} // namespace agent::ui
