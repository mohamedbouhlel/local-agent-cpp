#include "tool_registry.hpp"
#include "filesystem_sandbox.hpp"
#include "ui/tui_application.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <filesystem>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    AgentConfig config{argv[1], "test-model", "", 5};
    auto tools = std::make_shared<ToolRegistry>();
    const auto workspace = prepare_workspace_root(std::filesystem::absolute("workspace"));
    agent::ui::EventQueueLimits limits;
    limits.max_events = 1; // force bounded-queue overflow to exercise UI recovery path
    limits.max_event_payload_bytes = 1; // force TextDelta loss independent of scheduling
    try { return agent::ui::TuiApplication(std::move(config), std::move(tools), [workspace] { apply_filesystem_sandbox(workspace); }, limits).run(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 3; }
}
