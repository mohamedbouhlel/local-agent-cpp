#include "ui/console_renderer.hpp"

#include "ui/plain_renderer.hpp"
#include "ui/rich_renderer.hpp"

#include <algorithm>
#include <cctype>
#include <ostream>

namespace agent::ui {
namespace {

bool term_is_dumb(const std::optional<std::string>& term)
{
    if (!term) {
        return false;
    }
    std::string normalized = *term;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return normalized == "dumb";
}

} // namespace

std::unique_ptr<ConsoleRenderer> make_console_renderer(std::ostream& output,
                                                       std::ostream& diagnostics,
                                                       const TerminalCapabilities& capabilities)
{
    if (!capabilities.stdout_is_tty || !capabilities.stdout_ansi || term_is_dumb(capabilities.term)) {
        return std::make_unique<PlainRenderer>(output, diagnostics);
    }

    return std::make_unique<RichRenderer>(output,
                                          diagnostics,
                                          capabilities.stderr_ansi && !capabilities.no_color_present,
                                          capabilities.stderr_unicode,
                                          capabilities.stderr_narrow);
}

} // namespace agent::ui
