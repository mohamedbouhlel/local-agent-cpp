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

bool TuiNavigation::move_selection(int direction) noexcept
{
    if (focus() != TuiFocus::menu || direction == 0) return false;
    const auto current = static_cast<int>(page_);
    const auto count = static_cast<int>(TuiPage::count);
    select(static_cast<TuiPage>((current + (direction > 0 ? 1 : count - 1)) % count));
    return true;
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
