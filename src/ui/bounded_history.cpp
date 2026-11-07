#include "ui/bounded_history.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace agent::ui {

BoundedHistory::BoundedHistory(BoundedHistoryLimits limits) : limits_(limits)
{
    if (limits_.max_entries == 0) {
        throw std::invalid_argument("bounded history max_entries must be positive");
    }
}

BoundedHistoryAddResult BoundedHistory::add(std::string text)
{
    BoundedHistoryAddResult result;
    const std::size_t incoming_bytes = text.size();
    if (incoming_bytes > limits_.max_text_bytes) {
        if (rejected_too_large_count_ == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error("bounded history oversized-entry counter overflow");
        }
        ++rejected_too_large_count_;
        result.rejected_too_large = true;
        return result;
    }

    std::size_t evict_count = 0;
    std::size_t remaining_bytes = text_bytes_;
    auto oldest = entries_.begin();
    while (entries_.size() - evict_count >= limits_.max_entries ||
           incoming_bytes > limits_.max_text_bytes - remaining_bytes) {
        remaining_bytes -= oldest->size();
        ++oldest;
        ++evict_count;
    }
    if (evict_count > std::numeric_limits<std::uint64_t>::max() - evicted_count_) {
        throw std::overflow_error("bounded history eviction counter overflow");
    }

    // Stage the node first so allocation failure preserves the current history.
    // Transfer it only after eviction, keeping the history itself within both
    // limits throughout the mutation.
    std::list<std::string> staged;
    staged.push_back(std::move(text));
    for (std::size_t i = 0; i < evict_count; ++i) entries_.pop_front();
    entries_.splice(entries_.end(), staged);
    text_bytes_ = remaining_bytes + incoming_bytes;
    evicted_count_ += static_cast<std::uint64_t>(evict_count);
    result.retained = true;
    result.evicted_entries = evict_count;
    return result;
}

} // namespace agent::ui
