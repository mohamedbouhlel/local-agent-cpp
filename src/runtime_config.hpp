#pragma once

#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

namespace agent::config {

inline std::optional<long> parse_positive_seconds(std::string_view value) noexcept
{
    if (value.empty()) return std::nullopt;

    long seconds = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), seconds, 10);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || seconds <= 0) {
        return std::nullopt;
    }
    return seconds;
}

} // namespace agent::config
