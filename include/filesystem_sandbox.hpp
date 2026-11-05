#pragma once

#include <filesystem>

std::filesystem::path prepare_workspace_root(const std::filesystem::path& workspace_root);
void apply_filesystem_sandbox(const std::filesystem::path& workspace_root);
