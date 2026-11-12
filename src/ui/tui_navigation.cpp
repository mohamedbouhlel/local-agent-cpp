#include "ui/tui_navigation.hpp"

namespace agent::ui {

bool TuiNavigation::select_function_key(int number) noexcept
{
    if (number < 1 || number > static_cast<int>(TuiPage::count)) return false;
    select(static_cast<TuiPage>(number - 1));
    return true;
}

void TuiNavigation::select(TuiPage page) noexcept
{
    if (page < TuiPage::conversation || page >= TuiPage::count) return;
    page_ = page;
}

void TuiNavigation::escape_to_conversation() noexcept
{
    page_ = TuiPage::conversation;
}

void TuiNavigation::cycle_focus() noexcept
{
    auto& current = focus_[static_cast<std::size_t>(page_)];
    if (page_ == TuiPage::conversation) {
        current = current == TuiFocus::composer ? TuiFocus::menu
                 : current == TuiFocus::menu ? TuiFocus::content
                                             : TuiFocus::composer;
    } else {
        current = current == TuiFocus::menu ? TuiFocus::content : TuiFocus::menu;
    }
}

void TuiNavigation::set_focus(TuiFocus focus) noexcept
{
    if (page_ != TuiPage::conversation && focus == TuiFocus::composer) return;
    focus_[static_cast<std::size_t>(page_)] = focus;
}

bool TuiNavigation::move_selection(int direction) noexcept
{
    if (focus() != TuiFocus::menu || direction == 0) return false;
    if (subsection_count(page_) != 0) return move_subsection(direction);
    const auto current = static_cast<int>(page_);
    const auto count = static_cast<int>(TuiPage::count);
    select(static_cast<TuiPage>((current + (direction > 0 ? 1 : count - 1)) % count));
    return true;
}

bool TuiNavigation::move_subsection(int direction) noexcept
{
    const int count = subsection_count(page_);
    if (count == 0 || direction == 0) return false;
    auto& index = subsection_indices_[static_cast<std::size_t>(page_)];
    index = (index + (direction > 0 ? 1 : count - 1)) % count;
    return true;
}

bool TuiNavigation::select_subsection(int index) noexcept
{
    if (index < 0 || index >= subsection_count(page_)) return false;
    subsection_indices_[static_cast<std::size_t>(page_)] = index;
    return true;
}

bool TuiNavigation::activate_subsection() noexcept
{
    if (subsection_count(page_) == 0) return false;
    const auto page_index = static_cast<std::size_t>(page_);
    auto& open = subsection_open_[page_index];
    auto& opened_index = opened_subsection_indices_[page_index];
    const int selected = subsection_indices_[page_index];
    if (open && opened_index == selected) {
        open = false;
        opened_index = -1;
    } else {
        open = true;
        opened_index = selected;
    }
    return true;
}

bool TuiNavigation::close_subsection() noexcept
{
    const auto page_index = static_cast<std::size_t>(page_);
    auto& open = subsection_open_[page_index];
    if (!open) return false;
    open = false;
    opened_subsection_indices_[page_index] = -1;
    return true;
}

bool TuiNavigation::subsection_open() const noexcept
{
    const auto page_index = static_cast<std::size_t>(page_);
    return subsection_open_[page_index]
        && opened_subsection_indices_[page_index] == subsection_indices_[page_index];
}

int TuiNavigation::subsection_index() const noexcept
{
    return subsection_indices_[static_cast<std::size_t>(page_)];
}

int TuiNavigation::subsection_count(TuiPage page) noexcept
{
    if (page == TuiPage::configuration) return 3;
    if (page == TuiPage::help) return 3;
    return 0;
}

const char* TuiNavigation::page_name(TuiPage page) noexcept
{
    switch (page) {
    case TuiPage::conversation: return "Conversation";
    case TuiPage::tools: return "Tools";
    case TuiPage::metrics: return "Metrics";
    case TuiPage::diagnostics: return "Diagnostics";
    case TuiPage::configuration: return "Configuration";
    case TuiPage::help: return "Help";
    case TuiPage::count: break;
    }
    return "Unknown";
}

const char* TuiNavigation::short_page_name(TuiPage page) noexcept
{
    switch (page) {
    case TuiPage::conversation: return "Chat";
    case TuiPage::tools: return "Tools";
    case TuiPage::metrics: return "Metrics";
    case TuiPage::diagnostics: return "Diag";
    case TuiPage::configuration: return "Config";
    case TuiPage::help: return "Help";
    case TuiPage::count: break;
    }
    return "?";
}

} // namespace agent::ui
