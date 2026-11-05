#include "ui/terminal_text_sanitizer.hpp"

namespace agent::ui {
namespace {

void append_hex_byte(unsigned char byte, std::string& output)
{
    constexpr char digits[] = "0123456789ABCDEF";
    output += "\\x";
    output += digits[(byte >> 4U) & 0x0FU];
    output += digits[byte & 0x0FU];
}

void append_unicode_escape(std::uint32_t codepoint, std::string& output)
{
    constexpr char digits[] = "0123456789ABCDEF";
    output += "\\u";
    for (int shift = 12; shift >= 0; shift -= 4) {
        output += digits[(codepoint >> static_cast<unsigned int>(shift)) & 0x0FU];
    }
}

bool is_bidi_control(std::uint32_t codepoint)
{
    return codepoint == 0x061CU || (codepoint >= 0x200EU && codepoint <= 0x200FU)
        || (codepoint >= 0x202AU && codepoint <= 0x202EU)
        || (codepoint >= 0x2066U && codepoint <= 0x206FU);
}

} // namespace

std::string TerminalTextSanitizer::feed(std::string_view fragment)
{
    std::string output;
    output.reserve(fragment.size());

    for (const unsigned char byte : fragment) {
        switch (escape_state_) {
        case EscapeState::text:
            consume_text_byte(byte, output);
            break;
        case EscapeState::escape:
            if (byte == '[') {
                escape_state_ = EscapeState::csi;
            } else if (byte == ']' || byte == 'P' || byte == 'X' || byte == '^' || byte == '_') {
                escape_state_ = EscapeState::control_string;
            } else if (byte >= 0x20U && byte <= 0x2FU) {
                escape_state_ = EscapeState::escape_intermediate;
            } else {
                escape_state_ = EscapeState::text;
            }
            break;
        case EscapeState::escape_intermediate:
            if (byte >= 0x30U && byte <= 0x7EU) {
                escape_state_ = EscapeState::text;
            } else if (byte == 0x1BU) {
                escape_state_ = EscapeState::escape;
            }
            break;
        case EscapeState::csi:
            if (byte >= 0x40U && byte <= 0x7EU) {
                escape_state_ = EscapeState::text;
            } else if (byte == 0x1BU) {
                escape_state_ = EscapeState::escape;
            } else if (byte == 0x9CU) {
                escape_state_ = EscapeState::text;
            }
            break;
        case EscapeState::control_string:
            if (byte == 0x07U || byte == 0x9CU) {
                escape_state_ = EscapeState::text;
            } else if (byte == 0x1BU) {
                escape_state_ = EscapeState::control_string_escape;
            }
            break;
        case EscapeState::control_string_escape:
            if (byte == '\\' || byte == 0x07U || byte == 0x9CU) {
                escape_state_ = EscapeState::text;
            } else if (byte != 0x1BU) {
                escape_state_ = EscapeState::control_string;
            }
            break;
        }
    }
    return output;
}

std::string TerminalTextSanitizer::finish()
{
    std::string output;
    flush_invalid_utf8(output);
    // An unfinished escape/control string is discarded in full.
    escape_state_ = EscapeState::text;
    return output;
}

void TerminalTextSanitizer::consume_text_byte(unsigned char byte, std::string& output)
{
    if (!pending_utf8_.empty()) {
        if ((byte & 0xC0U) == 0x80U) {
            pending_utf8_ += static_cast<char>(byte);
            if (pending_utf8_.size() == pending_utf8_size_) {
                std::uint32_t codepoint = 0;
                const auto lead = static_cast<unsigned char>(pending_utf8_[0]);
                if (pending_utf8_size_ == 2) {
                    codepoint = lead & 0x1FU;
                } else if (pending_utf8_size_ == 3) {
                    codepoint = lead & 0x0FU;
                } else {
                    codepoint = lead & 0x07U;
                }
                for (std::size_t index = 1; index < pending_utf8_.size(); ++index) {
                    codepoint = (codepoint << 6U) | (static_cast<unsigned char>(pending_utf8_[index]) & 0x3FU);
                }

                const bool valid = (pending_utf8_size_ == 2 && codepoint >= 0x80U)
                    || (pending_utf8_size_ == 3 && codepoint >= 0x800U)
                    || (pending_utf8_size_ == 4 && codepoint >= 0x10000U && codepoint <= 0x10FFFFU);
                const bool surrogate = codepoint >= 0xD800U && codepoint <= 0xDFFFU;
                if (valid && !surrogate) {
                    consume_codepoint(pending_utf8_, codepoint, output);
                    pending_utf8_.clear();
                    pending_utf8_size_ = 0;
                } else {
                    flush_invalid_utf8(output);
                }
            }
            return;
        }
        flush_invalid_utf8(output);
    }

    if (byte == 0x1BU) {
        escape_state_ = EscapeState::escape;
    } else if (byte <= 0x1FU) {
        if (byte == '\n' && allow_newlines_) {
            output += '\n';
        } else if (byte == '\n') {
            output += "\\n";
        } else if (byte == '\t') {
            output += "\\t";
        } else {
            output += '^';
            output += static_cast<char>(byte + 0x40U);
        }
    } else if (byte == 0x7FU) {
        output += "^?";
    } else if (byte == 0x9BU) {
        escape_state_ = EscapeState::csi;
    } else if (byte == 0x9DU || byte == 0x90U || byte == 0x98U || byte == 0x9EU || byte == 0x9FU) {
        escape_state_ = EscapeState::control_string;
    } else if (byte >= 0xC2U && byte <= 0xDFU) {
        pending_utf8_ = static_cast<char>(byte);
        pending_utf8_size_ = 2;
    } else if (byte >= 0xE0U && byte <= 0xEFU) {
        pending_utf8_ = static_cast<char>(byte);
        pending_utf8_size_ = 3;
    } else if (byte >= 0xF0U && byte <= 0xF4U) {
        pending_utf8_ = static_cast<char>(byte);
        pending_utf8_size_ = 4;
    } else if (byte >= 0x80U) {
        append_hex_byte(byte, output);
    } else {
        output += static_cast<char>(byte);
    }
}

void TerminalTextSanitizer::consume_codepoint(const std::string& bytes,
                                              std::uint32_t codepoint,
                                              std::string& output)
{
    if ((codepoint >= 0x80U && codepoint <= 0x9FU) || is_bidi_control(codepoint)) {
        append_unicode_escape(codepoint, output);
    } else {
        output += bytes;
    }
}

void TerminalTextSanitizer::flush_invalid_utf8(std::string& output)
{
    for (const unsigned char byte : pending_utf8_) {
        append_hex_byte(byte, output);
    }
    pending_utf8_.clear();
    pending_utf8_size_ = 0;
}

std::string sanitize_terminal_line(std::string_view text)
{
    TerminalTextSanitizer sanitizer(false);
    auto result = sanitizer.feed(text);
    result += sanitizer.finish();
    return result;
}

} // namespace agent::ui
