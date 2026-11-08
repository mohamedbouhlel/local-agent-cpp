#include "tool_registry.hpp"
#include "filesystem_sandbox.hpp"
#include "ui/tui_application.hpp"
#include "ui/tui_approval_broker.hpp"
#include "workspace_tools.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <filesystem>
#include <string_view>

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3) return 2;
    AgentConfig config{argv[1], "test-model", "", 5};
    auto tools = std::make_shared<ToolRegistry>();
    const auto workspace = prepare_workspace_root(std::filesystem::absolute("workspace"));
    auto approvals = std::make_shared<agent::ui::TuiApprovalBroker>();
    register_workspace_tools(*tools, workspace, approvals);
    agent::ui::EventQueueLimits limits;
    if (argc == 2 || std::string_view(argv[2]) != "normal") {
        limits.max_events = 1; // force bounded-queue overflow to exercise UI recovery path
        limits.max_event_payload_bytes = 1; // force TextDelta loss independent of scheduling
    }
    try { return agent::ui::TuiApplication(std::move(config), std::move(tools), std::move(approvals), [workspace] { apply_filesystem_sandbox(workspace); }, limits).run(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 3; }
}
