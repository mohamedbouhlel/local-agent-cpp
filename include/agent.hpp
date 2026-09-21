#pragma once

#include "tool_registry.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

struct Message {
    nlohmann::json data;
};

struct AgentConfig {
    std::string base_url;
    std::string model;
    std::string api_key;
    long timeout_seconds = 60;
};

class Agent {
public:
    Agent(ToolRegistry& tools, AgentConfig config);

    std::string send(const std::string& input);

private:
    void append_message(Message message);
    void trim_history();

    ToolRegistry& tools_;
    AgentConfig config_;
    std::vector<Message> history_;
};
