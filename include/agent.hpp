#pragma once

#include "tool_registry.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <stdexcept>
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

// Erreur de protocole côté agent : flux LLM incomplet (pas de "[DONE]"), réponse
// tronquée par la limite de tokens, ou tool call malformé (id/nom vide). Elle
// distingue une réponse inexploitable d'une erreur de transport (HttpError) et
// signifie toujours que le tour n'a PAS produit d'action valide : rien n'a été
// ajouté à l'historique pour ce tour.
struct AgentError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Résultat d'exécution d'un outil, exposé au programme appellant. Il permet
// d'établir un verdict d'exécution fondé sur ce qui a réellement été exécuté,
// indépendamment du texte produit par le modèle.
struct ToolOutcome {
    std::string name;
    bool ok = false;
    std::string result;
};

// Callbacks optionnels : vides par défaut, donc Agent::send reste utilisable
// sans eux. on_text_delta est appelé à chaque fragment de texte reçu (affichage
// progressif), on_tool_call au nom de l'outil juste avant son exécution, et
// on_tool_result après chaque exécution avec son succès/échec réel.
struct AgentCallbacks {
    std::function<void(const std::string&)> on_text_delta;
    std::function<void(const std::string&)> on_tool_call;
    std::function<void(const ToolOutcome&)> on_tool_result;
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
