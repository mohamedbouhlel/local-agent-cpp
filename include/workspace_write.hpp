#pragma once

#include <filesystem>
#include <string>

std::string write_workspace(const std::filesystem::path& root,
                            const std::filesystem::path& relative,
                            const std::string& content, bool overwrite);
