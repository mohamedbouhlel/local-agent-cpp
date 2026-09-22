#pragma once

#include "tool_registry.hpp"

#include <nlohmann/json.hpp>

#include <functional>
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

// Callbacks optionnels : vides par défaut, donc Agent::send reste utilisable
// sans eux. on_text_delta est appelé à chaque fragment de texte reçu (affichage
// progressif), on_tool_call au nom de l'outil juste avant son exécution.
struct AgentCallbacks {
    std::function<void(const std::string&)> on_text_delta;
    std::function<void(const std::string&)> on_tool_call;
};

class Agent {
public:
    Agent(ToolRegistry& tools, AgentConfig config);

    std::string send(const std::string& input, const AgentCallbacks& callbacks = {});

private:
    void append_message(Message message);
    void trim_history();

    ToolRegistry& tools_;
    AgentConfig config_;
    std::vector<Message> history_;
};
