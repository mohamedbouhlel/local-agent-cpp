#pragma once
#include <filesystem>
#include <string>
std::string search_workspace(const std::filesystem::path& root, const std::string& query);
std::string list_workspace(const std::filesystem::path& root);
