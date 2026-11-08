#include "tool_registry.hpp"

#include <stdexcept>
#include <utility>

namespace {
class HandlerInvocation final : public ToolInvocation {
public:
    HandlerInvocation(std::function<std::string(nlohmann::json)> handler, nlohmann::json args)
        : handler_(std::move(handler)), args_(std::move(args)) {}
    std::string execute() override { return handler_(std::move(args_)); }
private:
    std::function<std::string(nlohmann::json)> handler_;
    nlohmann::json args_;
};
}

void ToolRegistry::register_tool(Tool tool)
{
    if (tool.name.empty()) {
        throw std::invalid_argument("tool name must not be empty");
    }
    if (!tool.handler && !tool.prepare) {
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
    auto invocation = prepare_execution(name, args_json, {});
    if (const auto request = invocation->approval_request()) {
        (void)request;
        const auto decision = invocation->authorize();
        if (decision != ApprovalDecision::approved) {
            switch (decision) {
            case ApprovalDecision::denied: return "write authorization denied; no file was created";
            case ApprovalDecision::cancelled: return "write authorization cancelled; no file was created";
            case ApprovalDecision::expired: return "write authorization expired; no file was created";
            default: return "write authorization unavailable; no file was created";
            }
        }
    }
    return invocation->execute();
}

std::unique_ptr<ToolInvocation> ToolRegistry::prepare_execution(const std::string& name,
                                                                 const std::string& args_json,
                                                                 const ToolInvocationContext& context) const
{
    const auto it = tools_.find(name);
    if (it == tools_.end()) {
        throw std::invalid_argument("unknown tool: " + name);
    }
    auto args = nlohmann::json::parse(args_json);
    if (it->second.prepare) {
        auto invocation = it->second.prepare(std::move(args), context);
        if (!invocation) throw std::logic_error("tool preparation returned no invocation");
        if (it->second.requires_approval && !invocation->approval_request())
            throw std::logic_error("tool requires approval but produced no approval request");
        return invocation;
    }
    if (it->second.requires_approval)
        throw std::logic_error("tool requires approval but has no preparation handler");
    return std::make_unique<HandlerInvocation>(it->second.handler, std::move(args));
}

bool ToolRegistry::requires_approval(const std::string& name) const
{
    const auto it = tools_.find(name);
    return it != tools_.end() && it->second.requires_approval;
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

bool ToolRegistry::empty() const
{
    return tools_.empty();
}
