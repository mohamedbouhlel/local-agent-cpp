#pragma once

#include "agent.hpp"
#include "ui/agent_worker.hpp"

#include <memory>
#include <functional>

class ToolRegistry;

namespace agent::ui {

class TuiApplication final {
public:
    TuiApplication(AgentConfig config,
                   std::shared_ptr<ToolRegistry> tools,
                   std::function<void()> enter_sandbox,
                   EventQueueLimits event_limits = {});
    int run();

private:
    AgentConfig config_;
    std::shared_ptr<ToolRegistry> tools_;
    std::function<void()> enter_sandbox_;
    EventQueueLimits event_limits_;
};

} // namespace agent::ui
