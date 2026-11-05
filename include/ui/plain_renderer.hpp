#pragma once

#include "ui/console_renderer.hpp"
#include "ui/terminal_text_sanitizer.hpp"

#include <iosfwd>

namespace agent::ui {

class PlainRenderer final : public ConsoleRenderer {
public:
    PlainRenderer(std::ostream& output, std::ostream& diagnostics) noexcept;

    RendererMode mode() const noexcept override { return RendererMode::plain; }
    void render_text(std::string_view text) override;
    void render_status(StatusKind kind, std::string_view message) override;
    void flush_text() override;

private:
    std::ostream& output_;
    std::ostream& diagnostics_;
    TerminalTextSanitizer sanitizer_;
};

} // namespace agent::ui
