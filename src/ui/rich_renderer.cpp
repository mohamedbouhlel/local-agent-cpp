#include "ui/rich_renderer.hpp"

#include <ostream>
#include <stdexcept>

namespace agent::ui {
namespace {

struct Style {
    const char* color;
    const char* ascii_label;
    const char* unicode_label;
};

Style style_for(StatusKind kind)
{
    switch (kind) {
    case StatusKind::info:
        return {"34", "[i]", "ℹ"};
    case StatusKind::model:
        return {"35", "[model]", "◆"};
    case StatusKind::tool:
        return {"33", "[tool]", "⚙"};
    case StatusKind::success:
        return {"32", "[ok]", "✓"};
    case StatusKind::warning:
        return {"33", "[!]", "⚠"};
    case StatusKind::error:
        return {"31", "[error]", "✗"};
    case StatusKind::secondary:
        return {"90", "[-]", "·"};
    }
    return {"90", "[-]", "·"};
}

} // namespace

RichRenderer::RichRenderer(std::ostream& output,
                           std::ostream& diagnostics,
                           bool diagnostics_color,
                           bool diagnostics_unicode,
                           bool narrow) noexcept
    : output_(output)
    , diagnostics_(diagnostics)
    , diagnostics_color_(diagnostics_color)
    , diagnostics_unicode_(diagnostics_unicode)
    , narrow_(narrow)
{
}

void RichRenderer::render_text(std::string_view text)
{
    // Conversation text remains in the terminal's normal color. All external
    // bytes pass through a stateful sanitizer before reaching either stream.
    output_ << sanitizer_.feed(text);
    output_.flush();
    if (!output_) {
        throw std::ios_base::failure("rich console output failed");
    }
}

void RichRenderer::render_status(StatusKind kind, std::string_view message)
{
    flush_text();
    const auto style = style_for(kind);
    const auto label = diagnostics_unicode_ ? style.unicode_label : style.ascii_label;
    if (diagnostics_color_) {
        diagnostics_ << "\033[" << style.color << 'm' << label << "\033[0m";
    } else {
        diagnostics_ << label;
    }
    diagnostics_ << (narrow_ ? " " : "  ") << sanitize_terminal_line(message) << '\n';
    diagnostics_.flush();
    if (!diagnostics_) {
        throw std::ios_base::failure("rich console diagnostics output failed");
    }
}

void RichRenderer::flush_text()
{
    output_ << sanitizer_.finish();
    output_.flush();
    if (!output_) {
        throw std::ios_base::failure("rich console output failed");
    }
}

} // namespace agent::ui
