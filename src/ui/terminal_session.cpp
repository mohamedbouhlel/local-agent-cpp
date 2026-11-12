#include "ui/terminal_session.hpp"

#include <clocale>
#include <cstdlib>
#include <stdexcept>
#include <unistd.h>

namespace agent::ui {

TerminalSession::TerminalSession()
    : capabilities_(detect_terminal_capabilities())
{
    if (!::isatty(STDIN_FILENO) || !capabilities_.stdout_is_tty) {
        throw std::runtime_error("TUI requires interactive stdin and stdout terminals");
    }
    if (!capabilities_.stdout_ansi) {
        throw std::runtime_error("TUI requires a terminal with screen support (TERM is unset or dumb)");
    }
    if (::tcgetattr(STDIN_FILENO, &original_termios_) != 0) {
        throw std::runtime_error("cannot read terminal settings");
    }
    have_original_termios_ = true;
    (void)std::setlocale(LC_CTYPE, "");
    screen_ = newterm(nullptr, stdout, stdin);
    if (screen_ == nullptr) {
        restore();
        throw std::runtime_error("ncursesw could not initialize this terminal");
    }
    (void)set_term(screen_);
    window_ = stdscr;
    if (cbreak() == ERR || noecho() == ERR || keypad(window_, TRUE) == ERR) {
        restore();
        throw std::runtime_error("ncursesw could not configure terminal input");
    }
    (void)curs_set(1);
    (void)mouseinterval(0);
    requested_mouse_mask_ = BUTTON1_PRESSED | BUTTON4_PRESSED | BUTTON5_PRESSED;
    mouse_available_ = mousemask(requested_mouse_mask_, &previous_mouse_mask_) != 0;
    mouse_reporting_enabled_ = mouse_available_;
    wtimeout(window_, 40);
}

TerminalSession::~TerminalSession() { restore(); }

void TerminalSession::set_mouse_reporting_enabled(bool enabled) noexcept
{
    if (!mouse_available_) return;
    (void)mousemask(enabled ? requested_mouse_mask_ : 0, nullptr);
    mouse_reporting_enabled_ = enabled;
}

void TerminalSession::restore() noexcept
{
    if (screen_ != nullptr) {
        (void)set_term(screen_);
        (void)mousemask(previous_mouse_mask_, nullptr);
        (void)endwin();
        delscreen(screen_);
        screen_ = nullptr;
        window_ = nullptr;
    }
    if (have_original_termios_) {
        (void)::tcsetattr(STDIN_FILENO, TCSANOW, &original_termios_);
        have_original_termios_ = false;
    }
}

} // namespace agent::ui
