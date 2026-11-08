#pragma once

#include "agent.hpp"
#include "ui/agent_worker.hpp"
#include "ui/tui_approval_broker.hpp"

#include <memory>
#include <functional>

class ToolRegistry;

namespace agent::ui {

class TuiApplication final {
public:
    TuiApplication(AgentConfig config,
                   std::shared_ptr<ToolRegistry> tools,
                   std::shared_ptr<TuiApprovalBroker> approvals,
                   std::function<void()> enter_sandbox,
                   EventQueueLimits event_limits = {});
    int run();

private:
    AgentConfig config_;
    std::shared_ptr<ToolRegistry> tools_;
    std::shared_ptr<TuiApprovalBroker> approvals_;
    std::function<void()> enter_sandbox_;
    EventQueueLimits event_limits_;
};

} // namespace agent::ui
