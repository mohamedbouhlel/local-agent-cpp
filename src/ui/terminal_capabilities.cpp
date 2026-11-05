#include "ui/terminal_capabilities.hpp"

#include <cstdlib>
#include <optional>
#include <string>

#include <sys/ioctl.h>
#include <unistd.h>

namespace agent::ui {
namespace {

std::optional<std::string> environment_value(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
}

std::optional<std::string> active_locale()
{
    if (auto value = environment_value("LC_ALL")) {
        return value;
    }
    if (auto value = environment_value("LC_CTYPE")) {
        return value;
    }
    return environment_value("LANG");
}

std::optional<unsigned int> terminal_width(int fd, bool is_tty)
{
    if (!is_tty) {
        return std::nullopt;
    }

    struct winsize size {};
    if (::ioctl(fd, TIOCGWINSZ, &size) != 0 || size.ws_col == 0) {
        return std::nullopt;
    }
    return static_cast<unsigned int>(size.ws_col);
}

bool has_utf8_locale(const std::optional<std::string>& locale)
{
    if (!locale) {
        return false;
    }
    std::string normalized = *locale;
    for (auto& character : normalized) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return normalized.find("utf-8") != std::string::npos || normalized.find("utf8") != std::string::npos;
}

bool term_is_ansi_capable(const std::optional<std::string>& term)
{
    if (!term || term->empty()) {
        return false;
    }
    std::string normalized = *term;
    for (auto& character : normalized) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return normalized != "dumb";
}

} // namespace

TerminalCapabilities terminal_capabilities_from_probe(const TerminalProbe& probe)
{
    TerminalCapabilities result;
    result.stdout_is_tty = probe.stdout_is_tty;
    result.stderr_is_tty = probe.stderr_is_tty;
    result.term = probe.term;
    result.no_color_present = probe.no_color_present;
    result.stdout_columns = probe.stdout_is_tty && probe.stdout_columns && *probe.stdout_columns > 0
        ? probe.stdout_columns
        : std::nullopt;
    result.stderr_columns = probe.stderr_is_tty && probe.stderr_columns && *probe.stderr_columns > 0
        ? probe.stderr_columns
        : std::nullopt;

    const bool term_ansi = term_is_ansi_capable(probe.term);
    result.stdout_ansi = result.stdout_is_tty && term_ansi;
    result.stderr_ansi = result.stderr_is_tty && term_ansi;

    const bool unicode_locale = has_utf8_locale(probe.locale);
    result.stdout_unicode = result.stdout_is_tty && term_ansi && unicode_locale;
    result.stderr_unicode = result.stderr_is_tty && term_ansi && unicode_locale;
    result.stdout_ascii_fallback = !result.stdout_unicode;
    result.stderr_ascii_fallback = !result.stderr_unicode;

    result.stdout_narrow = result.stdout_columns && *result.stdout_columns < narrow_terminal_columns;
    result.stderr_narrow = result.stderr_columns && *result.stderr_columns < narrow_terminal_columns;
    return result;
}

TerminalCapabilities detect_terminal_capabilities()
{
    TerminalProbe probe;
    probe.stdout_is_tty = ::isatty(STDOUT_FILENO) == 1;
    probe.stderr_is_tty = ::isatty(STDERR_FILENO) == 1;
    probe.term = environment_value("TERM");
    probe.no_color_present = std::getenv("NO_COLOR") != nullptr;
    probe.locale = active_locale();
    probe.stdout_columns = terminal_width(STDOUT_FILENO, probe.stdout_is_tty);
    probe.stderr_columns = terminal_width(STDERR_FILENO, probe.stderr_is_tty);
    return terminal_capabilities_from_probe(probe);
}

} // namespace agent::ui
