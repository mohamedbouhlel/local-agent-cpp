#pragma once

#include <array>
#include <cstddef>

namespace agent::ui {

enum class TuiPage : std::size_t {
    conversation,
    tools,
    metrics,
    diagnostics,
    configuration,
    help,
    count,
};

enum class TuiFocus {
    composer,
    menu,
    content,
};

class TuiNavigation final {
public:
    TuiPage page() const noexcept { return page_; }
    TuiFocus focus() const noexcept { return focus_[static_cast<std::size_t>(page_)]; }
    bool select_function_key(int number) noexcept;
    void select(TuiPage page) noexcept;
    void escape_to_conversation() noexcept;
    void cycle_focus() noexcept;
    void set_focus(TuiFocus focus) noexcept;
    bool move_selection(int direction) noexcept;
    bool move_subsection(int direction) noexcept;
    bool select_subsection(int index) noexcept;
    bool activate_subsection() noexcept;
    bool close_subsection() noexcept;
    bool subsection_open() const noexcept;
    int subsection_index() const noexcept;
    static int subsection_count(TuiPage page) noexcept;
    bool can_submit() const noexcept { return page_ == TuiPage::conversation; }

    static const char* page_name(TuiPage page) noexcept;
    static const char* short_page_name(TuiPage page) noexcept;

private:
    TuiPage page_ = TuiPage::conversation;
    std::array<TuiFocus, static_cast<std::size_t>(TuiPage::count)> focus_ = {
        TuiFocus::composer, TuiFocus::menu, TuiFocus::menu,
        TuiFocus::menu, TuiFocus::menu, TuiFocus::menu,
    };
    std::array<int, static_cast<std::size_t>(TuiPage::count)> subsection_indices_{};
    std::array<bool, static_cast<std::size_t>(TuiPage::count)> subsection_open_{};
    std::array<int, static_cast<std::size_t>(TuiPage::count)> opened_subsection_indices_ = [] {
        std::array<int, static_cast<std::size_t>(TuiPage::count)> values{};
        values.fill(-1);
        return values;
    }();
};

} // namespace agent::ui
