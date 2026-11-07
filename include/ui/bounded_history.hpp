#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <string>

namespace agent::ui {

struct BoundedHistoryLimits {
    std::size_t max_entries = 32;
    std::size_t max_text_bytes = 16'384;
};

struct BoundedHistoryAddResult {
    bool retained = false;
    bool rejected_too_large = false;
    std::size_t evicted_entries = 0;
};

class BoundedHistory final {
public:
    explicit BoundedHistory(BoundedHistoryLimits limits = {});

    BoundedHistoryAddResult add(std::string text);

    const std::list<std::string>& entries() const noexcept { return entries_; }
    bool empty() const noexcept { return entries_.empty(); }
    std::size_t size() const noexcept { return entries_.size(); }
    const std::string& front() const { return entries_.front(); }
    const std::string& back() const { return entries_.back(); }
    std::size_t text_bytes() const noexcept { return text_bytes_; }
    std::uint64_t evicted_count() const noexcept { return evicted_count_; }
    std::uint64_t rejected_too_large_count() const noexcept { return rejected_too_large_count_; }
    const BoundedHistoryLimits& limits() const noexcept { return limits_; }

private:
    BoundedHistoryLimits limits_;
    std::list<std::string> entries_;
    std::size_t text_bytes_ = 0;
    std::uint64_t evicted_count_ = 0;
    std::uint64_t rejected_too_large_count_ = 0;
};

} // namespace agent::ui
