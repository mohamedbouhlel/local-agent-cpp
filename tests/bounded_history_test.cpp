#include "test_support.hpp"
#include "ui/bounded_history.hpp"
#include "ui/tui_view.hpp"

#include <limits>
#include <string>

using agent::ui::BoundedHistory;
using agent::ui::BoundedHistoryLimits;

TEST(tui4b1, empty_and_entry_limit_evict_fifo_exactly)
{
    BoundedHistory history;
    CHECK(history.empty());
    CHECK_EQ(history.text_bytes(), std::size_t(0));
    for (int i = 0; i < 32; ++i) {
        const auto result = history.add("row" + std::to_string(i));
        CHECK(result.retained);
        CHECK_EQ(result.evicted_entries, std::size_t(0));
    }
    const auto result = history.add("row32");
    CHECK(result.retained);
    CHECK_EQ(result.evicted_entries, std::size_t(1));
    CHECK_EQ(history.evicted_count(), std::uint64_t(1));
    CHECK_EQ(history.size(), std::size_t(32));
    CHECK(history.front() == "row1");
    CHECK(history.back() == "row32");
}

TEST(tui4b1, byte_limit_can_evict_before_entry_limit)
{
    BoundedHistory history;
    history.add(std::string(9'000, 'a'));
    history.add(std::string(7'000, 'b'));
    CHECK_EQ(history.size(), std::size_t(2));
    const auto result = history.add(std::string(2'000, 'c'));
    CHECK_EQ(result.evicted_entries, std::size_t(1));
    CHECK_EQ(history.size(), std::size_t(2));
    CHECK_EQ(history.text_bytes(), std::size_t(9'000));
    CHECK(history.front().front() == 'b');
    CHECK(history.back().front() == 'c');
}

TEST(tui4b1, both_limits_apply_and_large_entry_is_counted_but_not_retained)
{
    BoundedHistory history({3, 5});
    history.add("aa");
    history.add("bb");
    const auto add = history.add("cc");
    CHECK_EQ(add.evicted_entries, std::size_t(1));
    CHECK_EQ(history.size(), std::size_t(2));
    CHECK_EQ(history.text_bytes(), std::size_t(4));
    const auto rejected = history.add("123456");
    CHECK(rejected.rejected_too_large);
    CHECK(!rejected.retained);
    CHECK_EQ(rejected.evicted_entries, std::size_t(0));
    CHECK_EQ(history.rejected_too_large_count(), std::uint64_t(1));
    CHECK_EQ(history.front(), std::string("bb"));
    CHECK_EQ(history.back(), std::string("cc"));
}

TEST(tui4b1, empty_multibyte_and_post_sanitization_byte_accounting)
{
    BoundedHistory history({32, 8});
    history.add("");
    history.add("é");
    CHECK_EQ(history.text_bytes(), std::size_t(2));
    CHECK(history.back() == "é");

    agent::ui::TuiView view;
    view.add_diagnostic("safe\x1b]0;secret-title\x07tail");
    CHECK(view.diagnostics().back().find("secret-title") == std::string::npos);
    CHECK_EQ(view.diagnostics().text_bytes(), view.diagnostics().back().size());
    CHECK_EQ(view.diagnostics().evicted_count(), std::uint64_t(0));
}

TEST(tui4b1, independent_view_histories_warn_only_when_content_was_not_retained)
{
    agent::ui::TuiView view;
    auto rows = view.rendered_page_rows(agent::ui::TuiPage::diagnostics, 120);
    std::string joined;
    for (const auto& row : rows) joined += row + '\n';
    CHECK(joined.find("Historique partiel") == std::string::npos);

    for (int i = 0; i < 33; ++i) view.add_diagnostic("diagnostic-" + std::to_string(i));
    CHECK_EQ(view.diagnostics().evicted_count(), std::uint64_t(1));
    CHECK_EQ(view.tool_activity().evicted_count(), std::uint64_t(0));
    rows = view.rendered_page_rows(agent::ui::TuiPage::diagnostics, 120);
    joined.clear();
    for (const auto& row : rows) joined += row + '\n';
    CHECK(joined.find("Historique partiel : 1 entrée évincée") != std::string::npos);
    CHECK(joined.find("diagnostic-0") == std::string::npos);
    CHECK(joined.find("diagnostic-1") != std::string::npos);
    CHECK(view.rendered_page_rows(agent::ui::TuiPage::tools, 120).back()
          == "No tool execution observed in this session.");
}

TEST(tui4b1, diagnostic_scroll_anchor_survives_fifo_eviction)
{
    agent::ui::TuiView view;
    view.on_resize(80, 5);
    for (int i = 0; i < 32; ++i) view.add_diagnostic("diagnostic-" + std::to_string(i));
    view.scroll_page_up(agent::ui::TuiPage::diagnostics);
    const auto old_offset = view.page_scroll_offset(agent::ui::TuiPage::diagnostics);
    CHECK(old_offset > 0);
    view.add_diagnostic("diagnostic-32");
    CHECK_EQ(view.page_scroll_offset(agent::ui::TuiPage::diagnostics), old_offset + 2);
    CHECK(view.rendered_page_rows(agent::ui::TuiPage::diagnostics, 80).size() >= 32);
}

TEST(tui4b1, sustained_insertions_keep_exact_bounds_and_counters)
{
    BoundedHistory history;
    for (int i = 0; i < 2'000; ++i) {
        history.add(std::string(700, static_cast<char>('a' + (i % 26))));
        CHECK(history.size() <= 32);
        CHECK(history.text_bytes() <= 16'384);
        std::size_t measured = 0;
        for (const auto& item : history.entries()) measured += item.size();
        CHECK_EQ(history.text_bytes(), measured);
    }
    CHECK_EQ(history.evicted_count(), std::uint64_t(1'977));
    CHECK_EQ(history.rejected_too_large_count(), std::uint64_t(0));
    CHECK_EQ(history.size(), std::size_t(23));
}

TEST(tui4b1, oversized_sanitized_panel_entry_is_rejected_and_reported)
{
    agent::ui::TuiView view;
    view.add_tool_activity(std::string(16'385, 'x'));
    CHECK(view.tool_activity().empty());
    CHECK_EQ(view.tool_activity().rejected_too_large_count(), std::uint64_t(1));
    std::string joined;
    for (const auto& row : view.rendered_page_rows(agent::ui::TuiPage::tools, 120)) joined += row + '\n';
    CHECK(joined.find("1 entrée trop volumineuse non retenue") != std::string::npos);
    CHECK(joined.find(std::string(100, 'x')) == std::string::npos);
}
