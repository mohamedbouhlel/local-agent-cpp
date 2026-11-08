#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "workspace_write_authorization.hpp"

class ToolInvocation {
public:
    virtual ~ToolInvocation() = default;
    virtual std::shared_ptr<const WriteApprovalRequest> approval_request() const { return {}; }
    virtual ApprovalDecision authorize() { return ApprovalDecision::approved; }
    virtual std::string execute() = 0;
};

struct Tool {
    Tool() = default;
    Tool(std::string tool_name,
         std::string tool_description,
         nlohmann::json tool_schema,
         std::function<std::string(nlohmann::json)> tool_handler,
         std::function<std::unique_ptr<ToolInvocation>(nlohmann::json, const ToolInvocationContext&)> tool_prepare = {},
         bool approval_required = false)
        : name(std::move(tool_name)), description(std::move(tool_description)), schema(std::move(tool_schema)),
          handler(std::move(tool_handler)), prepare(std::move(tool_prepare)), requires_approval(approval_required) {}

    std::string name;
    std::string description;
    nlohmann::json schema;
    std::function<std::string(nlohmann::json)> handler;
    std::function<std::unique_ptr<ToolInvocation>(nlohmann::json, const ToolInvocationContext&)> prepare;
    bool requires_approval = false;
};
