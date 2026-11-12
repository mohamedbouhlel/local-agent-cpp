#include "ui/markdown_presenter.hpp"

#include <algorithm>
#include <cctype>

namespace agent::ui {
namespace {

std::vector<std::string_view> split_lines(std::string_view input)
{
    std::vector<std::string_view> result;
    std::size_t start = 0;
    while (start <= input.size()) {
        const auto end = input.find('\n', start);
        result.push_back(input.substr(start, end == std::string_view::npos ? input.size() - start : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}

std::string lower(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

std::string inline_text(std::string_view input)
{
    std::string result;
    for (std::size_t i = 0; i < input.size();) {
        if (input[i] == '!' && i + 1 < input.size() && input[i + 1] == '[') {
            const auto close = input.find("](", i + 2);
            const auto end = close == std::string_view::npos ? close : input.find(')', close + 2);
            if (close != std::string_view::npos && end != std::string_view::npos) {
                result += "[image: "; result.append(input.substr(i + 2, close - i - 2)); result += "] <";
                result.append(input.substr(close + 2, end - close - 2)); result += ">";
                i = end + 1; continue;
            }
        }
        if (input[i] == '[') {
            const auto close = input.find("](", i + 1);
            const auto end = close == std::string_view::npos ? close : input.find(')', close + 2);
            if (close != std::string_view::npos && end != std::string_view::npos) {
                result.append(input.substr(i + 1, close - i - 1)); result += " <";
                result.append(input.substr(close + 2, end - close - 2)); result += ">";
                i = end + 1; continue;
            }
        }
        if (input[i] == '`' || input[i] == '*' || input[i] == '_') {
            const char marker = input[i];
            std::size_t marker_size = 1;
            if (i + 1 < input.size() && input[i + 1] == marker) marker_size = 2;
            const auto is_word = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
            if (marker == '_' && i > 0 && is_word(input[i - 1])) {
                result.push_back(input[i++]);
                continue;
            }
            const auto closing = input.find(std::string(marker_size, marker), i + marker_size);
            if (closing != std::string_view::npos
                && !(marker == '_' && closing + marker_size < input.size() && is_word(input[closing + marker_size]))) {
                if (marker == '`') result += "[";
                result.append(input.substr(i + marker_size, closing - i - marker_size));
                if (marker == '`') result += "]";
                i = closing + marker_size; continue;
            }
        }
        result.push_back(input[i++]);
    }
    return result;
}

bool is_fence(std::string_view line)
{
    const auto first = line.find_first_not_of(" \t");
    return first != std::string_view::npos && line.substr(first, 3) == "```";
}

MarkdownLineKind inline_kind(std::string_view input)
{
    if (input.find("**") != std::string_view::npos || input.find('*') != std::string_view::npos)
        return MarkdownLineKind::emphasis;
    const auto underscore = input.find('_');
    if (underscore != std::string_view::npos
        && (underscore == 0 || !(std::isalnum(static_cast<unsigned char>(input[underscore - 1])) || input[underscore - 1] == '_')))
        return MarkdownLineKind::emphasis;
    if (input.find('`') != std::string_view::npos) return MarkdownLineKind::inline_code;
    if (input.find("](") != std::string_view::npos) return MarkdownLineKind::link;
    return MarkdownLineKind::paragraph;
}

} // namespace

std::vector<MarkdownLine> present_markdown(std::string_view source, bool enabled)
{
    if (!enabled) {
        std::vector<MarkdownLine> raw;
        for (const auto line : split_lines(source)) raw.push_back({std::string(line), MarkdownLineKind::raw, {}});
        return raw;
    }

    const auto source_lines = split_lines(source);
    std::vector<MarkdownLine> result;
    for (std::size_t i = 0; i < source_lines.size();) {
        const auto line = source_lines[i];
        if (is_fence(line)) {
            std::size_t close = i + 1;
            while (close < source_lines.size() && !is_fence(source_lines[close])) ++close;
            if (close == source_lines.size()) {
                // An unfinished fence remains literal until a matching close arrives.
                for (; i < source_lines.size(); ++i) result.push_back({std::string(source_lines[i]), MarkdownLineKind::raw, {}});
                break;
            }
            const auto first = line.find("```");
            const auto language = lower(line.substr(first + 3));
            for (++i; i < close; ++i) result.push_back({std::string(source_lines[i]), MarkdownLineKind::code, language});
            i = close + 1;
            continue;
        }

        if (line.empty()) { result.push_back({{}, MarkdownLineKind::paragraph, {}}); ++i; continue; }
        const auto first = line.find_first_not_of(" \t");
        const auto trimmed = first == std::string_view::npos ? std::string_view{} : line.substr(first);
        if (!trimmed.empty() && trimmed.size() <= 3
            && std::all_of(trimmed.begin(), trimmed.end(), [](char c) { return c == '-' || c == '*' || c == '_'; })) {
            result.push_back({"────────────────", MarkdownLineKind::separator, {}}); ++i; continue;
        }
        std::size_t hashes = 0;
        while (hashes < trimmed.size() && hashes < 3 && trimmed[hashes] == '#') ++hashes;
        if (hashes > 0 && hashes < trimmed.size() && trimmed[hashes] == ' ') {
            result.push_back({inline_text(trimmed.substr(hashes + 1)), MarkdownLineKind::heading, {}}); ++i; continue;
        }
        if (trimmed.starts_with("> ")) {
            result.push_back({"│ " + inline_text(trimmed.substr(2)), MarkdownLineKind::quote, {}}); ++i; continue;
        }
        if (trimmed.starts_with("- ") || trimmed.starts_with("* ") || trimmed.starts_with("+ ")) {
            result.push_back({"• " + inline_text(trimmed.substr(2)), MarkdownLineKind::list, {}}); ++i; continue;
        }
        std::size_t digits = 0;
        while (digits < trimmed.size() && std::isdigit(static_cast<unsigned char>(trimmed[digits]))) ++digits;
        if (digits > 0 && digits + 1 < trimmed.size() && trimmed[digits] == '.' && trimmed[digits + 1] == ' ') {
            result.push_back({std::string(trimmed.substr(0, digits + 1)) + " "
                                  + inline_text(trimmed.substr(digits + 2)), MarkdownLineKind::list, {}});
            ++i; continue;
        }
        result.push_back({inline_text(line), inline_kind(line), {}});
        ++i;
    }
    return result;
}

} // namespace agent::ui
