#include "ui/tui_view.hpp"

#include "ui/terminal_text_sanitizer.hpp"

#include <algorithm>
#include <cwchar>
#include <cstdlib>
#include <iterator>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace agent::ui {
namespace {
enum class VisualStyle { normal, muted, brand, selected, peach, mint, pink, status, heading, quote, list, code, emphasis };

int width_of(std::string_view text);

bool initialize_colors()
{
    if (std::getenv("NO_COLOR") != nullptr || !has_colors()) return false;
    if (start_color() == ERR) return false;
    static_cast<void>(use_default_colors());
    const short background = COLOR_BLACK;
    static_cast<void>(init_pair(1, COLOR_WHITE, background));
    static_cast<void>(init_pair(2, COLOR_CYAN, background));
    static_cast<void>(init_pair(3, COLOR_MAGENTA, background));
    static_cast<void>(init_pair(4, COLOR_YELLOW, background));
    static_cast<void>(init_pair(5, COLOR_GREEN, background));
    static_cast<void>(init_pair(6, COLOR_WHITE, COLOR_RED));
    static_cast<void>(init_pair(7, COLOR_CYAN, background));
    return true;
}

int style_attributes(VisualStyle style, bool colors)
{
    if (!colors) {
        if (style == VisualStyle::selected || style == VisualStyle::brand) return A_REVERSE | A_BOLD;
        if (style == VisualStyle::status || style == VisualStyle::pink || style == VisualStyle::heading) return A_BOLD;
        return A_NORMAL;
    }
    switch (style) {
    case VisualStyle::normal: return COLOR_PAIR(1);
    case VisualStyle::muted: return COLOR_PAIR(2);
    case VisualStyle::brand: return COLOR_PAIR(3) | A_BOLD;
    case VisualStyle::selected: return COLOR_PAIR(3) | A_REVERSE | A_BOLD;
    case VisualStyle::peach: return COLOR_PAIR(4) | A_BOLD;
    case VisualStyle::mint: return COLOR_PAIR(5) | A_BOLD;
    case VisualStyle::pink: return COLOR_PAIR(6) | A_BOLD;
    case VisualStyle::status: return COLOR_PAIR(7) | A_BOLD;
    case VisualStyle::heading: return COLOR_PAIR(4) | A_BOLD;
    case VisualStyle::quote: return COLOR_PAIR(2) | A_ITALIC;
    case VisualStyle::list: return COLOR_PAIR(5);
    case VisualStyle::code: return COLOR_PAIR(1) | A_DIM;
    case VisualStyle::emphasis: return COLOR_PAIR(3) | A_BOLD;
    }
    return A_NORMAL;
}

std::string clip_to_cells(std::string_view text, int max_cells)
{
    std::string result;
    if (max_cells <= 0) return result;
    std::mbstate_t state{};
    int cells = 0;
    for (std::size_t i = 0; i < text.size();) {
        wchar_t wc{};
        const auto count = std::mbrtowc(&wc, text.data() + i, text.size() - i, &state);
        if (count == static_cast<std::size_t>(-1) || count == static_cast<std::size_t>(-2)) {
            if (cells + 1 > max_cells) break;
            result.push_back('?');
            ++cells;
            ++i;
            state = {};
            continue;
        }
        const auto bytes = count == 0 ? std::size_t{1} : count;
        const int width = std::max(0, ::wcwidth(wc));
        if (cells + width > max_cells) break;
        result.append(text.substr(i, bytes));
        cells += width;
        i += bytes;
    }
    return result;
}

void draw_at(WINDOW* window, int row, int column, std::string_view text, int max_cells,
             VisualStyle style, bool colors)
{
    if (row < 0 || column < 0 || max_cells <= 0) return;
    const auto clipped = clip_to_cells(text, max_cells);
    static_cast<void>(wattron(window, style_attributes(style, colors)));
    static_cast<void>(mvwaddstr(window, row, column, clipped.c_str()));
    static_cast<void>(wattroff(window, A_ATTRIBUTES));
}

void draw_line(WINDOW* window, int row, std::string_view text, int columns,
               VisualStyle style, bool colors)
{
    draw_at(window, row, 0, text, columns - 1, style, colors);
}

VisualStyle markdown_style(MarkdownLineKind kind)
{
    switch (kind) {
    case MarkdownLineKind::heading: return VisualStyle::heading;
    case MarkdownLineKind::quote: return VisualStyle::quote;
    case MarkdownLineKind::list: return VisualStyle::list;
    case MarkdownLineKind::separator: return VisualStyle::muted;
    case MarkdownLineKind::emphasis: return VisualStyle::emphasis;
    case MarkdownLineKind::inline_code: return VisualStyle::code;
    case MarkdownLineKind::link: return VisualStyle::brand;
    case MarkdownLineKind::code: return VisualStyle::code;
    case MarkdownLineKind::paragraph:
    case MarkdownLineKind::raw: return VisualStyle::normal;
    }
    return VisualStyle::normal;
}

bool language_supported(std::string_view language)
{
    return language == "cpp" || language == "c++" || language == "python" || language == "py"
        || language == "bash" || language == "sh" || language == "json"
        || language == "yaml" || language == "yml" || language == "diff";
}

bool syntax_keyword(std::string_view word, std::string_view language)
{
    static constexpr std::string_view common[] = {
        "if", "else", "for", "while", "return", "class", "struct", "const", "auto",
        "void", "int", "bool", "true", "false", "null", "nullptr", "def", "import",
        "from", "as", "in", "not", "and", "or", "function", "let", "var", "echo",
        "select", "include", "namespace", "public", "private", "static", "try", "catch",
    };
    if (!language_supported(language)) return false;
    return std::find(std::begin(common), std::end(common), word) != std::end(common);
}

void draw_code_line(WINDOW* window, int row, int column, std::string_view text, int max_cells,
                    std::string_view language, bool colors)
{
    if (!language_supported(language) || language == "diff") {
        const auto style = language == "diff" && !text.empty() && text.front() == '+' ? VisualStyle::mint
            : language == "diff" && !text.empty() && text.front() == '-' ? VisualStyle::pink : VisualStyle::code;
        draw_at(window, row, column, text, max_cells, style, colors);
        return;
    }
    std::size_t i = 0;
    int cell = column;
    while (i < text.size() && cell < column + max_cells) {
        const auto start = i;
        VisualStyle style = VisualStyle::code;
        if ((text[i] == '#' && (language == "python" || language == "bash" || language == "sh" || language == "yaml" || language == "yml"))
            || (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/')) {
            i = text.size(); style = VisualStyle::muted;
        } else if (text[i] == '"' || text[i] == '\'') {
            const char quote = text[i++];
            while (i < text.size()) {
                if (text[i] == '\\' && i + 1 < text.size()) { i += 2; continue; }
                if (text[i++] == quote) break;
            }
            style = VisualStyle::mint;
        } else if (std::isdigit(static_cast<unsigned char>(text[i]))) {
            while (i < text.size() && (std::isdigit(static_cast<unsigned char>(text[i])) || text[i] == '.')) ++i;
            style = VisualStyle::peach;
        } else if (std::isalpha(static_cast<unsigned char>(text[i])) || text[i] == '_') {
            while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) ++i;
            if (syntax_keyword(text.substr(start, i - start), language)) style = VisualStyle::brand;
        } else {
            ++i;
            if (std::string_view("{}[]():,.;=+-*/<>!").find(text[start]) != std::string_view::npos)
                style = VisualStyle::pink;
        }
        const auto byte = static_cast<unsigned char>(text[start]);
        if (byte >= 0x80) {
            std::mbstate_t state{};
            wchar_t wc{};
            const auto count = std::mbrtowc(&wc, text.data() + start, text.size() - start, &state);
            if (count != static_cast<std::size_t>(-1) && count != static_cast<std::size_t>(-2) && count != 0) {
                i = start + count;
                style = VisualStyle::code;
            }
        }
        const auto token = text.substr(start, i - start);
        const int width = width_of(token);
        if (width <= 0) continue;
        draw_at(window, row, cell, token, column + max_cells - cell, style, colors);
        cell += width;
    }
}

std::string page_title(TuiPage page)
{
    switch (page) {
    case TuiPage::conversation: return "CONVERSATION";
    case TuiPage::tools: return "TOOLS";
    case TuiPage::metrics: return "ACTIVITY";
    case TuiPage::diagnostics: return "DIAGNOSTICS";
    case TuiPage::configuration: return "CONFIGURATION";
    case TuiPage::help: return "HELP";
    case TuiPage::count: return "LOFITERM";
    }
    return "LOFITERM";
}

std::string ascii_markdown_fallback(std::string_view text)
{
    std::string result;
    for (std::size_t i = 0; i < text.size();) {
        if (text.substr(i).starts_with("─")) { result.push_back('-'); i += std::string_view("─").size(); }
        else if (text.substr(i).starts_with("•")) { result.push_back('*'); i += std::string_view("•").size(); }
        else if (text.substr(i).starts_with("│")) { result.push_back('|'); i += std::string_view("│").size(); }
        else result.push_back(text[i++]);
    }
    return result;
}

std::string navigation_label(int key, std::string_view label, bool selected, bool compact)
{
    const std::string short_label = compact ? std::string(label.substr(0, std::min<std::size_t>(4, label.size())))
                                             : std::string(label);
    return selected ? "[F" + std::to_string(key) + " " + short_label + "]"
                    : " F" + std::to_string(key) + " " + short_label + " ";
}

struct TabGeometry { int page; int column; int width; std::string label; };

std::vector<TabGeometry> tab_geometry(int columns, TuiPage active, bool menu_focus)
{
    const bool compact = columns < 100;
    const std::array<std::string_view, 6> labels = {"CHAT", "TOOLS", "ACTIVITY", "DIAG", "CONFIG", "HELP"};
    int column = menu_focus ? width_of("MENU") : 0;
    std::vector<TabGeometry> tabs;
    for (int i = 0; i < 6 && column < columns - 1; ++i) {
        if (column > 0) ++column;
        auto label = navigation_label(i + 1, labels[static_cast<std::size_t>(i)],
                                      static_cast<int>(active) == i, compact);
        const int width = std::min(width_of(label), columns - 1 - column);
        if (width <= 0) break;
        column += width;
        tabs.push_back({i, column - width, width, std::move(label)});
    }
    return tabs;
}

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

std::string metric_duration(const std::optional<std::chrono::steady_clock::duration>& duration)
{
    if (!duration) return "unavailable";
    std::ostringstream output;
    output << std::fixed << std::setprecision(3)
           << std::chrono::duration<double, std::milli>(*duration).count() << " ms";
    return output.str();
}

const char* tool_outcome(const std::optional<ToolMetricOutcome>& outcome)
{
    if (!outcome) return "in progress / unavailable";
    return *outcome == ToolMetricOutcome::succeeded ? "succeeded" : "failed";
}

const char* http_outcome(const std::optional<HttpRequestOutcome>& outcome)
{
    if (!outcome) return "in progress / unavailable";
    switch (*outcome) {
    case HttpRequestOutcome::completed: return "transport completed";
    case HttpRequestOutcome::failed: return "transport failed";
    case HttpRequestOutcome::legacy_callback_aborted: return "legacy callback aborted";
    }
    return "unavailable";
}

std::string exact_count_or_unavailable(std::uint64_t value, bool overflowed)
{
    return overflowed ? "unavailable (counter overflow)" : std::to_string(value);
}
}

void TuiView::set_status(std::string_view status) { status_ = status; }
void TuiView::set_markdown_enabled(bool enabled) noexcept
{
    if (markdown_enabled_ == enabled) return;
    markdown_enabled_ = enabled;
    invalidate_conversation_cache();
}

void TuiView::invalidate_conversation_cache() noexcept
{
    cached_conversation_columns_ = -1;
    conversation_cache_.clear();
}

void TuiView::invalidate_conversation_message(std::size_t index) noexcept
{
    if (index < conversation_cache_.size()) conversation_cache_[index].valid = false;
}

void TuiView::add_user_message(std::string text)
{
    const auto old_rows = follow_bottom_ ? 0 : rendered_conversation_rows(last_columns_).size();
    messages_.push_back({TuiMessageKind::user, std::move(text), false});
    active_assistant_.reset();
    if (!follow_bottom_) preserve_anchor_after_content_change(old_rows);
}
void TuiView::append_assistant_delta(std::string_view text)
{
    if (!active_assistant_) {
        messages_.push_back({TuiMessageKind::assistant, {}, false});
        active_assistant_ = messages_.size() - 1;
    }
    const auto old_rows = follow_bottom_ ? 0 : rendered_conversation_rows(last_columns_).size();
    messages_[*active_assistant_].text.append(text);
    invalidate_conversation_message(*active_assistant_);
    if (follow_bottom_) scroll_offset_ = 0;
    else preserve_anchor_after_content_change(old_rows);
}
void TuiView::replace_assistant_response(std::string text)
{
    const auto old_rows = follow_bottom_ ? 0 : rendered_conversation_rows(last_columns_).size();
    if (!active_assistant_) {
        messages_.push_back({TuiMessageKind::assistant, std::move(text), false});
        active_assistant_ = messages_.size() - 1;
    } else {
        messages_[*active_assistant_].text = std::move(text);
    }
    messages_[*active_assistant_].incomplete = false;
    invalidate_conversation_message(*active_assistant_);
    if (follow_bottom_) scroll_offset_ = 0;
    else preserve_anchor_after_content_change(old_rows);
}
void TuiView::finish_assistant(bool complete)
{
    if (active_assistant_) {
        messages_[*active_assistant_].incomplete = !complete;
        invalidate_conversation_message(*active_assistant_);
    }
    active_assistant_.reset();
}
void TuiView::add_notice(std::string_view text)
{
    const auto old_rows = follow_bottom_ ? 0 : rendered_conversation_rows(last_columns_).size();
    messages_.push_back({TuiMessageKind::notice, std::string(text), false});
    if (!follow_bottom_) preserve_anchor_after_content_change(old_rows);
}
void TuiView::set_view_data(TuiViewData data) { view_data_ = std::move(data); }
void TuiView::set_metrics(TurnMetrics metrics)
{
    const auto index = static_cast<std::size_t>(TuiPage::metrics);
    const auto old_rows = rendered_page_rows(TuiPage::metrics, last_columns_);
    const auto height = static_cast<std::size_t>(std::max(1, last_chat_height_));
    const auto old_offset = std::min(page_scroll_offsets_[index],
        old_rows.size() > height ? old_rows.size() - height : 0);
    const auto old_end = old_rows.size() > old_offset ? old_rows.size() - old_offset : 0;
    const auto old_begin = old_end > height ? old_end - height : 0;
    const std::string anchor = old_begin < old_rows.size() ? old_rows[old_begin] : std::string{};
    const bool follows = page_follows_bottom_[index];

    metrics_ = std::move(metrics);
    const auto new_rows = rendered_page_rows(TuiPage::metrics, last_columns_);
    if (follows) {
        page_scroll_offsets_[index] = 0;
        return;
    }
    const auto anchor_it = anchor.empty() ? new_rows.end()
        : std::find(new_rows.begin(), new_rows.end(), anchor);
    if (anchor_it != new_rows.end()) {
        const auto anchor_row = static_cast<std::size_t>(std::distance(new_rows.begin(), anchor_it));
        const auto end = std::min(new_rows.size(), anchor_row + height);
        page_scroll_offsets_[index] = new_rows.size() - end;
    } else {
        page_scroll_offsets_[index] = new_rows.size() > height ? new_rows.size() - height : 0;
    }
}
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
bool TuiView::scroll_up()
{
    const auto old_offset = scroll_offset_;
    const bool old_follow = follow_bottom_;
    const auto row_count = rendered_conversation_rows(last_columns_).size();
    const auto maximum = row_count > static_cast<std::size_t>(last_chat_height_)
        ? row_count - static_cast<std::size_t>(last_chat_height_) : 0;
    if (maximum == 0) return false;
    follow_bottom_ = false;
    scroll_offset_ = std::min(maximum, scroll_offset_ + 5);
    return old_offset != scroll_offset_ || old_follow != follow_bottom_;
}
bool TuiView::scroll_down()
{
    const auto old_offset = scroll_offset_;
    const bool old_follow = follow_bottom_;
    scroll_offset_ = scroll_offset_ > 5 ? scroll_offset_ - 5 : 0;
    follow_bottom_ = scroll_offset_ == 0;
    return old_offset != scroll_offset_ || old_follow != follow_bottom_;
}
bool TuiView::scroll_page_up(TuiPage page)
{
    if (page == TuiPage::conversation) return scroll_up();
    const auto index = static_cast<std::size_t>(page);
    const auto old_offset = page_scroll_offsets_[index];
    const bool old_follow = page_follows_bottom_[index];
    const auto rows = rendered_page_rows(page, last_columns_).size();
    const auto visible = static_cast<std::size_t>(std::max(1, last_chat_height_));
    const auto maximum = rows > visible ? rows - visible : 0;
    page_scroll_offsets_[index] = std::min(maximum, page_scroll_offsets_[index] + 5);
    page_follows_bottom_[index] = page_scroll_offsets_[index] == 0;
    return old_offset != page_scroll_offsets_[index] || old_follow != page_follows_bottom_[index];
}
bool TuiView::scroll_page_down(TuiPage page)
{
    if (page == TuiPage::conversation) return scroll_down();
    const auto index = static_cast<std::size_t>(page);
    const auto old_offset = page_scroll_offsets_[index];
    const bool old_follow = page_follows_bottom_[index];
    page_scroll_offsets_[index] = page_scroll_offsets_[index] > 5 ? page_scroll_offsets_[index] - 5 : 0;
    page_follows_bottom_[index] = page_scroll_offsets_[index] == 0;
    return old_offset != page_scroll_offsets_[index] || old_follow != page_follows_bottom_[index];
}
void TuiView::on_resize(int columns, int height)
{
    if (last_columns_ != std::max(1, columns)) invalidate_conversation_cache();
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
std::vector<TuiConversationRow> TuiView::rendered_styled_conversation_rows(int columns) const
{
    const int normalized_columns = std::max(1, columns);
    if (cached_conversation_columns_ != normalized_columns
        || cached_conversation_markdown_ != markdown_enabled_) {
        conversation_cache_.clear();
        cached_conversation_columns_ = normalized_columns;
        cached_conversation_markdown_ = markdown_enabled_;
    }
    if (messages_.empty()) return {{"No messages yet. Type a message below.", MarkdownLineKind::paragraph, {}}};
    conversation_cache_.resize(messages_.size());
    const int usable = std::max(1, columns - 2);
    for (std::size_t message_index = 0; message_index < messages_.size(); ++message_index) {
        auto& cached = conversation_cache_[message_index];
        if (cached.valid) continue;
        cached.rows.clear();
        const auto& message = messages_[message_index];
        const std::string full_prefix = message.kind == TuiMessageKind::user ? "You: " :
                                        message.kind == TuiMessageKind::assistant ? "LOFITERM Bot: " : "* ";
        const std::string prefix = message.kind == TuiMessageKind::assistant && width_of(full_prefix) + 1 >= usable
            ? "Bot: " : full_prefix;
        const std::string continuation(prefix.size(), ' ');
        const auto parsed = present_markdown(message.text,
            message.kind == TuiMessageKind::assistant && markdown_enabled_);
        bool first_line = true;
        for (const auto& line : parsed) {
            const auto first_prefix = first_line ? std::string_view(prefix) : std::string_view(continuation);
            auto wrapped = wrap(line.text, first_prefix, continuation, usable);
            for (auto& row : wrapped) cached.rows.push_back({std::move(row), line.kind, line.language});
            first_line = false;
        }
        if (message.incomplete) cached.rows.push_back({continuation + "[incomplete]", MarkdownLineKind::raw, {}});
        cached.valid = true;
    }
    std::vector<TuiConversationRow> result;
    std::size_t row_count = 0;
    for (const auto& cached : conversation_cache_) row_count += cached.rows.size();
    result.reserve(row_count);
    for (const auto& cached : conversation_cache_) result.insert(result.end(), cached.rows.begin(), cached.rows.end());
    return result;
}

std::vector<std::string> TuiView::rendered_conversation_rows(int columns) const
{
    std::vector<std::string> result;
    for (const auto& row : rendered_styled_conversation_rows(columns)) result.push_back(row.text);
    return result;
}
std::vector<std::string> TuiView::rendered_page_rows(TuiPage page, int columns) const
{
    return rendered_page_rows(page, columns, 0, false);
}

std::vector<std::string> TuiView::rendered_page_rows(TuiPage page, int columns,
                                                      int subsection, bool subsection_open) const
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
        source.emplace_back("Observed metrics for current/last turn:");
        if (!metrics_.event_details_complete) {
            source.emplace_back("PARTIAL: event loss or inconsistency; observed metrics are incomplete.");
        }
        if (!metrics_.turn_id) {
            source.emplace_back("Unavailable: no turn metrics observed yet.");
            break;
        }
        source.push_back("Turn ID: " + std::to_string(metrics_.turn_id->value));
        source.push_back(std::string("Technical outcome: ") + (metrics_.outcome
            ? (*metrics_.outcome == TurnOutcome::completed ? "completed" : "failed") : "unavailable"));
        source.push_back("Turn duration: " + metric_duration(metrics_.duration));
        source.push_back("First client-observed delta: " + metric_duration(metrics_.first_delta_latency)
                         + " (not server TTFT)");
        source.push_back("HTTP requests started: " + exact_count_or_unavailable(metrics_.http_requests_started,
                                                                                 metrics_.counter_overflowed)
                         + "; finished: " + exact_count_or_unavailable(metrics_.http_requests_finished,
                                                                         metrics_.counter_overflowed));
        source.push_back("Response body bytes received: "
                         + (metrics_.response_body_bytes_received()
                                ? std::to_string(*metrics_.response_body_bytes_received()) : "unavailable"));
        source.push_back("Tool calls started: " + exact_count_or_unavailable(metrics_.tools_started,
                                                                               metrics_.counter_overflowed)
                         + "; succeeded: " + exact_count_or_unavailable(metrics_.successful_tools(),
                                                                          metrics_.counter_overflowed)
                         + "; failed: " + exact_count_or_unavailable(metrics_.failed_tools(),
                                                                        metrics_.counter_overflowed));
        source.push_back("Text fragments: " + exact_count_or_unavailable(metrics_.text_fragments,
                                                                           metrics_.counter_overflowed)
                         + " (fragments, not tokens)");
        if (metrics_.detail_history_truncated) {
            source.push_back("Detail history limited: HTTP evicted " + std::to_string(metrics_.evicted_http_details)
                             + ", tools evicted " + std::to_string(metrics_.evicted_tool_details)
                             + ", tool details rejected " + std::to_string(metrics_.rejected_tool_details) + ".");
        }
        for (const auto& request : metrics_.http_requests) {
            std::string row = "HTTP request " + std::to_string(request.request_id.value) + ": "
                + http_outcome(request.outcome) + ", duration " + metric_duration(request.duration)
                + ", body bytes ";
            row += request.response_body_bytes ? std::to_string(*request.response_body_bytes) : "unavailable";
            source.push_back(std::move(row));
        }
        for (const auto& tool : metrics_.tools) {
            source.push_back("Tool " + sanitize_terminal_line(tool.name) + " (request "
                             + std::to_string(tool.request_id.value) + "): " + tool_outcome(tool.outcome)
                             + ", duration " + metric_duration(tool.duration));
        }
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
        source.emplace_back("Configuration — read only");
        source.emplace_back(std::string(subsection == 0 ? "> " : "  ") + "Model / connection");
        source.emplace_back(std::string(subsection == 1 ? "> " : "  ") + "Security / approvals");
        source.emplace_back(std::string(subsection == 2 ? "> " : "  ") + "Display preferences");
        if (subsection_open && subsection == 0) {
            source.push_back("Model: " + sanitize_terminal_line(view_data_.model));
            source.push_back("Endpoint: " + sanitize_terminal_line(view_data_.endpoint_origin));
            source.push_back("Timeout: " + std::to_string(view_data_.timeout_seconds) + " seconds");
            source.push_back(std::string("API key: ") + (view_data_.api_key_configured ? "configured (value hidden)" : "not configured"));
            source.emplace_back("Workspace: unavailable in this view.");
        } else if (subsection_open && subsection == 1) {
            source.emplace_back("write_file creates only after an exact per-request approval challenge.");
            source.emplace_back("Existing files cannot be overwritten. Landlock confines this process.");
        } else if (subsection_open) {
            source.emplace_back("Chat Markdown: formatted by default; M toggles plain text when content is focused.");
            source.emplace_back(std::getenv("NO_COLOR") ? "Colors: disabled by NO_COLOR." : "Colors: terminal palette when available.");
        }
        break;
    case TuiPage::help:
        source.emplace_back("Help");
        source.emplace_back(std::string(subsection == 0 ? "> " : "  ") + "Getting started");
        source.emplace_back(std::string(subsection == 1 ? "> " : "  ") + "Keyboard and mouse");
        source.emplace_back(std::string(subsection == 2 ? "> " : "  ") + "About / limits");
        if (subsection_open && subsection == 0) {
            source.emplace_back("1. Type a message in Chat.");
            source.emplace_back("2. Press F9 to send; Enter inserts a newline.");
            source.emplace_back("3. Inspect Tools and Activity for observed events.");
        } else if (subsection_open && subsection == 1) {
            source.emplace_back("F1-F6 switch views; Tab changes focus; Escape returns/back.");
            source.emplace_back("Arrows edit the composer or move a focused menu; PgUp/PgDn scroll.");
            source.emplace_back("Enter inserts a newline in the composer or opens a focused section.");
            source.emplace_back("F9 sends once from Chat; F10 requests exit.");
            source.emplace_back("Mouse if supported: click tabs/composer; wheel scrolls.");
            source.emplace_back("Sélectionner du texte : Maj + clic-glisser.");
            source.emplace_back("M toggles Markdown/plain text with Chat content focused.");
        } else if (subsection_open) {
            source.emplace_back("LOFITERM — local AI terminal");
            source.emplace_back("Version: unavailable (not embedded in this build).");
            source.emplace_back("Commit: unavailable (not embedded in this build).");
            source.emplace_back("Platform: Linux; Landlock is required.");
            source.emplace_back("Project: https://github.com/mohamedbouhlel/local-agent-cpp");
            source.emplace_back("Docs: README.md and USAGE.md in the source repository.");
            source.emplace_back("The configured LLM server may be remote; transport completion does not verify task success.");
            source.emplace_back("write_file requires explicit approval; existing files cannot be overwritten.");
        }
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

int TuiView::hit_test_tab(int x, int columns, TuiPage active, bool menu_focus) const
{
    if (x < 0 || columns < 1) return -1;
    for (const auto& tab : tab_geometry(columns, active, menu_focus)) {
        if (x >= tab.column && x < tab.column + tab.width) return tab.page;
    }
    return -1;
}

bool TuiView::hit_test_composer(int y, int rows) const noexcept
{
    return y >= rows - 4 && y < rows;
}

int TuiView::hit_test_subsection(TuiPage page, int y) const noexcept
{
    const int index = y - 5;
    if ((page == TuiPage::configuration || page == TuiPage::help) && index >= 0 && index < 3) return index;
    return -1;
}

bool TuiView::hit_test_content(int x, int y, int rows, int columns, TuiPage page,
                               int subsection, bool subsection_open) const
{
    constexpr int chat_top = 4;
    if (x < 0 || y < chat_top || rows <= 0 || columns <= 0 || x >= columns - 1) return false;
    const bool conversation = page == TuiPage::conversation;
    if (conversation && messages_.empty()) return false; // The empty-state prompt is informative, not focusable content.
    const int composer_top = conversation ? rows - 4 : rows;
    const int chat_height = std::max(0, composer_top - chat_top);
    if (y >= composer_top || chat_height == 0) return false;

    std::vector<std::string> lines;
    if (conversation) {
        const auto projected = rendered_styled_conversation_rows(columns);
        lines.reserve(projected.size());
        for (const auto& row : projected) lines.push_back(row.text);
    } else {
        lines = rendered_page_rows(page, columns, subsection, subsection_open);
    }

    const auto content_size = lines.size();
    const auto offset = conversation ? scroll_offset_ : page_scroll_offsets_[static_cast<std::size_t>(page)];
    const bool follows = conversation ? follow_bottom_ : page_follows_bottom_[static_cast<std::size_t>(page)];
    const auto end = follows ? content_size : content_size > offset ? content_size - offset : 0;
    const auto begin = end > static_cast<std::size_t>(chat_height)
        ? end - static_cast<std::size_t>(chat_height) : 0;
    const auto line_index = begin + static_cast<std::size_t>(y - chat_top);
    if (line_index >= end || line_index >= content_size) return false;
    return width_of(lines[line_index]) > x;
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
    if (page == TuiPage::metrics && !metrics_.event_details_complete) {
        label = "PARTIAL";
    } else if (page == TuiPage::metrics && metrics_.detail_history_truncated) {
        label = "DETAILS LIMITED";
    } else if (history && (history->evicted_count() != 0 || history->rejected_too_large_count() != 0)) {
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
    const char* contextual = page == TuiPage::conversation && focus == TuiFocus::composer
        ? "Enter newline | F9 send" : focus == TuiFocus::menu ? "Arrows select | Enter open | Esc back"
        : "PgUp/PgDn scroll | F1-F6 views";
    const std::string base = label.empty() ? active + " [" + focus_label + "] " + status_
                                           : label + " " + active + " [" + focus_label + "] " + status_;
    return base + " | " + contextual;
}

bool TuiView::layout_supported(int rows, int columns) noexcept
{
    return rows >= 20 && columns >= 60;
}

void TuiView::draw(WINDOW* window, bool unicode, const TuiNavigation& navigation,
                   std::string_view input, std::size_t cursor)
{
    ++draw_count_;
    int rows = 0, cols = 0;
    getmaxyx(window, rows, cols);
    if (rows <= 0 || cols <= 0) return;
    const bool colors = initialize_colors();
    static_cast<void>(wbkgd(window, colors ? COLOR_PAIR(1) : A_NORMAL));
    werase(window);
    const auto page = navigation.page();
    if (!layout_supported(rows, cols)) {
        static_cast<void>(curs_set(0));
        draw_line(window, 0, "LOFITERM", cols, VisualStyle::brand, colors);
        if (rows <= 5 || cols < 20) {
            if (rows > 1) draw_line(window, 1, "MIN 60x20", cols, VisualStyle::normal, colors);
            if (rows > 2) draw_line(window, 2, "TOO SMALL", cols, VisualStyle::pink, colors);
            if (rows > 3) draw_line(window, 3, "F10 QUIT", cols, VisualStyle::peach, colors);
            (void)wnoutrefresh(window);
            (void)doupdate();
            return;
        }
        if (rows > 2) draw_line(window, 2, "Terminal too small for the TUI.", cols, VisualStyle::pink, colors);
        if (rows > 3) draw_line(window, 3, "Minimum: 60 columns x 20 rows.", cols, VisualStyle::normal, colors);
        if (rows > 4) draw_line(window, 4, "Resize terminal; draft is preserved.", cols, VisualStyle::muted, colors);
        if (rows > 6) draw_line(window, rows - 2, "F10 Quit", cols, VisualStyle::peach, colors);
        (void)wnoutrefresh(window);
        (void)doupdate();
        return;
    }
    const bool compact = cols < 100 || rows < 30;
    draw_line(window, 0, compact ? "LOFITERM  |  LOCAL AI TERMINAL" : "LOFITERM  |  LOCAL AI TERMINAL  -  PEACH FOG",
              cols, VisualStyle::brand, colors);
    int nav_column = 0;
    if (navigation.focus() == TuiFocus::menu) {
        draw_at(window, 1, nav_column, "MENU", cols - 1, VisualStyle::peach, colors);
        nav_column = width_of("MENU");
    }
    for (const auto& tab : tab_geometry(cols, page, navigation.focus() == TuiFocus::menu)) {
        const auto style = tab.page == static_cast<int>(page) ? VisualStyle::selected : VisualStyle::muted;
        draw_at(window, 1, tab.column, tab.label, tab.width, style, colors);
    }
    std::string status = status_line(page, navigation.focus(), cols);
    const bool partial = status.find("PARTIAL") != std::string::npos;
    draw_line(window, 2, status, cols, partial ? VisualStyle::pink : VisualStyle::status, colors);
    const auto title_style = page == TuiPage::metrics ? VisualStyle::mint
        : page == TuiPage::diagnostics ? VisualStyle::pink : VisualStyle::peach;
    draw_line(window, 3, std::string(unicode ? "─ " : "+-") + page_title(page)
              + " ----------------", cols, title_style, colors);
    const int chat_top = 4;
    const bool conversation = page == TuiPage::conversation;
    const int composer_top = conversation ? rows - 4 : rows;
    const int chat_height = std::max(0, composer_top - chat_top);
    std::vector<std::string> content;
    std::vector<TuiConversationRow> conversation_rows;
    if (conversation) conversation_rows = rendered_styled_conversation_rows(cols);
    else content = rendered_page_rows(page, cols, navigation.subsection_index(), navigation.subsection_open());
    const std::size_t content_size = conversation ? conversation_rows.size() : content.size();
    auto& offset = conversation ? scroll_offset_ : page_scroll_offsets_[static_cast<std::size_t>(page)];
    const bool follows = conversation ? follow_bottom_ : page_follows_bottom_[static_cast<std::size_t>(page)];
    if (follows) offset = 0;
    else offset = std::min(offset, content_size > static_cast<std::size_t>(chat_height)
        ? content_size - static_cast<std::size_t>(chat_height) : 0);
    const auto end = content_size > offset ? content_size - offset : 0;
    const auto begin = end > static_cast<std::size_t>(chat_height) ? end - static_cast<std::size_t>(chat_height) : 0;
    for (std::size_t i = begin; i < end && static_cast<int>(i - begin) < chat_height; ++i) {
        const int y = chat_top + static_cast<int>(i - begin);
        if (conversation) {
            const auto& item = conversation_rows[i];
            if (item.kind == MarkdownLineKind::code) draw_code_line(window, y, 0, item.text, cols - 1, item.language, colors);
            else draw_line(window, y, unicode ? std::string_view(item.text)
                                              : std::string_view(ascii_markdown_fallback(item.text)),
                           cols, markdown_style(item.kind), colors);
        } else {
            draw_line(window, y, content[i], cols, VisualStyle::normal, colors);
        }
    }
    if (conversation) {
        draw_line(window, composer_top, markdown_enabled_ ? "MESSAGE  |  Enter newline  |  F9 send  |  M plain/formatted"
                                                          : "MESSAGE  |  Enter newline  |  F9 send  |  M formatted/plain", cols,
                  navigation.focus() == TuiFocus::composer ? VisualStyle::peach : VisualStyle::muted, colors);
        const auto visible = std::max(1, rows - composer_top - 1);
        const auto composer = rendered_composer(input, cursor, cols, visible);
        for (std::size_t i = 0; i < composer.rows.size(); ++i) {
            const int y = composer_top + 1 + static_cast<int>(i);
            if (y < rows) draw_line(window, y, composer.rows[i], cols, VisualStyle::normal, colors);
        }
        const int y = composer_top + 1 + static_cast<int>(composer.cursor_visual_row);
        (void)wmove(window, std::clamp(y, 0, rows - 1), std::clamp(composer.cursor_column, 0, cols - 1));
    }
    static_cast<void>(curs_set(conversation && navigation.focus() == TuiFocus::composer ? 1 : 0));
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
