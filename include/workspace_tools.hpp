#pragma once

#include "tool_registry.hpp"
#include "workspace_write_authorization.hpp"

#include <filesystem>

void register_workspace_tools(ToolRegistry& registry,
                             std::filesystem::path workspace_root,
                             std::shared_ptr<ApprovalProvider> approval_provider);
