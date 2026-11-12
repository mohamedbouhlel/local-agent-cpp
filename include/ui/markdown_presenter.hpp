#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace agent::ui {

enum class MarkdownLineKind { paragraph, heading, quote, list, separator, emphasis, inline_code, link, code, raw };

struct MarkdownLine {
    std::string text;
    MarkdownLineKind kind = MarkdownLineKind::paragraph;
    std::string language;
};

// Presentation-only, deliberately small Markdown subset. Input is never
// modified and is expected to have terminal controls sanitized by the caller.
std::vector<MarkdownLine> present_markdown(std::string_view source, bool enabled);

} // namespace agent::ui
