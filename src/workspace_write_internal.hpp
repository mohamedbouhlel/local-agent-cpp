#pragma once

#include <string>

namespace workspace_write_internal {
void verify_published_file(int directory_fd, const std::string& name, int verified_fd);
}
