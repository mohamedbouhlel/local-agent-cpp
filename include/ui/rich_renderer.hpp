#pragma once

#include "ui/console_renderer.hpp"
#include "ui/terminal_text_sanitizer.hpp"

#include <iosfwd>

namespace agent::ui {

class RichRenderer final : public ConsoleRenderer {
public:
    RichRenderer(std::ostream& output,
                 std::ostream& diagnostics,
                 bool diagnostics_color,
                 bool diagnostics_unicode,
                 bool narrow) noexcept;

    RendererMode mode() const noexcept override { return RendererMode::rich; }
    void render_text(std::string_view text) override;
    void render_status(StatusKind kind, std::string_view message) override;
    void flush_text() override;

private:
    std::ostream& output_;
    std::ostream& diagnostics_;
    bool diagnostics_color_;
    bool diagnostics_unicode_;
    bool narrow_;
    TerminalTextSanitizer sanitizer_;
};

} // namespace agent::ui
