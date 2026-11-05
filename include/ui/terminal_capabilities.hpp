#pragma once

#include <optional>
#include <string>

namespace agent::ui {

inline constexpr unsigned int narrow_terminal_columns = 80;

// Raw observations are injectable so tests do not depend on the invoking TTY.
struct TerminalProbe {
    bool stdout_is_tty = false;
    bool stderr_is_tty = false;
    std::optional<std::string> term;
    bool no_color_present = false;
    std::optional<std::string> locale;
    std::optional<unsigned int> stdout_columns;
    std::optional<unsigned int> stderr_columns;
};

struct TerminalCapabilities {
    bool stdout_is_tty = false;
    bool stderr_is_tty = false;
    std::optional<std::string> term;
    bool no_color_present = false;
    std::optional<unsigned int> stdout_columns;
    std::optional<unsigned int> stderr_columns;
    bool stdout_ansi = false;
    bool stderr_ansi = false;
    bool stdout_unicode = false;
    bool stderr_unicode = false;
    bool stdout_narrow = false;
    bool stderr_narrow = false;
    bool stdout_ascii_fallback = true;
    bool stderr_ascii_fallback = true;
};

// Combines explicit observations only; this does not select renderer behavior.
TerminalCapabilities terminal_capabilities_from_probe(const TerminalProbe& probe);

// Reads TERM, NO_COLOR, locale and the current stdout/stderr descriptors. It
// performs no network probe and does not modify the process environment.
TerminalCapabilities detect_terminal_capabilities();

} // namespace agent::ui
