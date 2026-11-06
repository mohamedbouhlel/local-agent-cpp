#include "ui/tui_application.hpp"

#include "tool_registry.hpp"
#include "ui/agent_worker.hpp"
#include "ui/terminal_session.hpp"
#include "ui/tui_event_loop.hpp"
#include "ui/tui_view.hpp"

#include <stdexcept>
#include <utility>

namespace agent::ui {

TuiApplication::TuiApplication(AgentConfig config,
                               std::shared_ptr<ToolRegistry> tools,
                               std::function<void()> enter_sandbox,
                               EventQueueLimits event_limits)
    : config_(std::move(config)), tools_(std::move(tools)), enter_sandbox_(std::move(enter_sandbox)), event_limits_(event_limits)
{
    if (!tools_ || !enter_sandbox_) throw std::invalid_argument("TUI requires tools and sandbox setup");
}

int TuiApplication::run()
{
    TerminalSession terminal;
    // ncurses loads terminfo during newterm(). Initialize that terminal state
    // before Landlock; then confine the process before creating/running Agent.
    enter_sandbox_();
    AgentWorker worker([config = config_, tools = tools_](std::shared_ptr<EventObserver> observer) {
        return std::make_unique<Agent>(*tools, config, std::move(observer));
    }, event_limits_);
    TuiView view;
    TuiEventLoop loop(worker, terminal, view);
    return loop.run();
}

} // namespace agent::ui
