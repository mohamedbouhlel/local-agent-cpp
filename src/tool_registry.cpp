#include "tool_registry.hpp"

#include <stdexcept>
#include <utility>

void ToolRegistry::register_tool(Tool tool)
{
    if (tool.name.empty()) {
        throw std::invalid_argument("tool name must not be empty");
    }
    if (!tool.handler) {
        throw std::invalid_argument("tool handler must not be empty");
    }

    const auto name = tool.name;
    auto [_, inserted] = tools_.emplace(name, std::move(tool));
    if (!inserted) {
        throw std::invalid_argument("tool already registered: " + name);
    }
}

std::string ToolRegistry::execute(const std::string& name, const std::string& args_json) const
{
    const auto it = tools_.find(name);
    if (it == tools_.end()) {
        throw std::invalid_argument("unknown tool: " + name);
    }

    return it->second.handler(nlohmann::json::parse(args_json));
}

nlohmann::json ToolRegistry::schemas_json() const
{
    auto tools = nlohmann::json::array();
    for (const auto& [_, tool] : tools_) {
        tools.push_back({
            {"type", "function"},
            {"function",
             {
                 {"name", tool.name},
                 {"description", tool.description},
                 {"parameters", tool.schema},
             }},
        });
    }

    return tools;
}
