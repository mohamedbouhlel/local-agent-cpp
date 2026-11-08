#pragma once

#include <filesystem>
#include <string>

#include "workspace_write_authorization.hpp"

std::string write_workspace(PreparedWrite&& prepared);
