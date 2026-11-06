#pragma once

#include "ui/terminal_capabilities.hpp"

#include <termios.h>

#include <cstdio>

#include <curses.h>

namespace agent::ui {

class TerminalSession final {
public:
    TerminalSession();
    ~TerminalSession();

    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;

    WINDOW* window() const noexcept { return window_; }
    bool unicode_enabled() const noexcept { return capabilities_.stdout_unicode; }

private:
    void restore() noexcept;

    TerminalCapabilities capabilities_;
    termios original_termios_{};
    bool have_original_termios_ = false;
    SCREEN* screen_ = nullptr;
    SCREEN* previous_screen_ = nullptr;
    WINDOW* window_ = nullptr;
};

} // namespace agent::ui
