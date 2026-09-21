#pragma once

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

struct Tool {
    std::string name;
    std::string description;
    nlohmann::json schema;
    std::function<std::string(nlohmann::json)> handler;
};
