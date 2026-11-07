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
    bool move_selection(int direction) noexcept;
    bool can_submit() const noexcept { return page_ == TuiPage::conversation; }

    static const char* page_name(TuiPage page) noexcept;
    static const char* short_page_name(TuiPage page) noexcept;

private:
    TuiPage page_ = TuiPage::conversation;
    std::array<TuiFocus, static_cast<std::size_t>(TuiPage::count)> focus_ = {
        TuiFocus::composer, TuiFocus::menu, TuiFocus::menu,
        TuiFocus::menu, TuiFocus::menu, TuiFocus::menu,
    };
};

} // namespace agent::ui
