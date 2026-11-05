#pragma once

#include "ui/terminal_capabilities.hpp"

#include <iosfwd>
#include <memory>
#include <string_view>

namespace agent::ui {

enum class StatusKind {
    info,
    model,
    tool,
    success,
    warning,
    error,
    secondary,
};

enum class RendererMode {
    plain,
    rich,
};

class ConsoleRenderer {
public:
    virtual ~ConsoleRenderer() = default;

    virtual RendererMode mode() const noexcept = 0;
    virtual void render_text(std::string_view text) = 0;
    virtual void render_status(StatusKind kind, std::string_view message) = 0;
    virtual void flush_text() = 0;
};

// Automatic selection uses stdout as the conversation surface. stderr keeps
// its own ANSI/Unicode observations for status rendering.
std::unique_ptr<ConsoleRenderer> make_console_renderer(std::ostream& output,
                                                       std::ostream& diagnostics,
                                                       const TerminalCapabilities& capabilities);

} // namespace agent::ui
