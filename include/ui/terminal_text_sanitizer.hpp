#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace agent::ui {

// Incremental terminal-control filter. It may be fed arbitrary stream
// fragments; escape-sequence and UTF-8 state is retained between calls.
class TerminalTextSanitizer {
public:
    explicit TerminalTextSanitizer(bool allow_newlines = true) noexcept
        : allow_newlines_(allow_newlines)
    {
    }

    std::string feed(std::string_view fragment);
    std::string finish();

private:
    enum class EscapeState {
        text,
        escape,
        escape_intermediate,
        csi,
        control_string,
        control_string_escape,
    };

    void consume_text_byte(unsigned char byte, std::string& output);
    void consume_codepoint(const std::string& bytes, std::uint32_t codepoint, std::string& output);
    void flush_invalid_utf8(std::string& output);

    EscapeState escape_state_ = EscapeState::text;
    std::string pending_utf8_;
    std::size_t pending_utf8_size_ = 0;
    bool allow_newlines_ = true;
};

// One-shot sanitizer for names and diagnostics which must stay on one line.
std::string sanitize_terminal_line(std::string_view text);

} // namespace agent::ui
