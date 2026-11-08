#pragma once

#include "tool.hpp"

#include <map>
#include <string>

#include <nlohmann/json.hpp>

class ToolRegistry {
public:
    void register_tool(Tool tool);
    std::string execute(const std::string& name, const std::string& args_json) const;
    std::unique_ptr<ToolInvocation> prepare_execution(const std::string& name,
                                                      const std::string& args_json,
                                                      const ToolInvocationContext& context) const;
    bool requires_approval(const std::string& name) const;
    nlohmann::json schemas_json() const;
    bool empty() const;

private:
    std::map<std::string, Tool> tools_;
};
