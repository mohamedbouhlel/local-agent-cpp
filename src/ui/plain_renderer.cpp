#include "ui/plain_renderer.hpp"

#include <ostream>
#include <stdexcept>

namespace agent::ui {
namespace {

const char* label_for(StatusKind kind)
{
    switch (kind) {
    case StatusKind::info:
        return "[info]";
    case StatusKind::model:
        return "[model]";
    case StatusKind::tool:
        return "[tool]";
    case StatusKind::success:
        return "[ok]";
    case StatusKind::warning:
        return "[warning]";
    case StatusKind::error:
        return "[error]";
    case StatusKind::secondary:
        return "[status]";
    }
    return "[status]";
}

} // namespace

PlainRenderer::PlainRenderer(std::ostream& output, std::ostream& diagnostics) noexcept
    : output_(output)
    , diagnostics_(diagnostics)
{
}

void PlainRenderer::render_text(std::string_view text)
{
    output_ << sanitizer_.feed(text);
    output_.flush();
    if (!output_) {
        throw std::ios_base::failure("plain console output failed");
    }
}

void PlainRenderer::render_status(StatusKind kind, std::string_view message)
{
    flush_text();
    diagnostics_ << label_for(kind) << ' ' << sanitize_terminal_line(message) << '\n';
    diagnostics_.flush();
    if (!diagnostics_) {
        throw std::ios_base::failure("plain console diagnostics output failed");
    }
}

void PlainRenderer::flush_text()
{
    output_ << sanitizer_.finish();
    output_.flush();
    if (!output_) {
        throw std::ios_base::failure("plain console output failed");
    }
}

} // namespace agent::ui
