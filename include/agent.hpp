#pragma once

#include "tool_registry.hpp"
#include "ui/agent_event.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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
    Agent(ToolRegistry& tools,
          AgentConfig config,
          std::shared_ptr<agent::ui::EventObserver> observer);

    std::string send(const std::string& input, const AgentCallbacks& callbacks = {});

private:
    void append_message(Message message);
    void trim_history();
    template <typename PayloadFactory>
    void publish_event(PayloadFactory&& make_payload,
                       agent::ui::TurnId turn_id,
                       std::optional<agent::ui::RequestId> request_id = std::nullopt,
                       std::optional<std::string_view> tool_call_id = std::nullopt) noexcept
    {
        if (!observer_enabled_ || !observer_) {
            return;
        }

        try {
            const auto now = std::chrono::steady_clock::now();
            agent::ui::EventMetadata metadata;
            metadata.session_id = session_id_;
            metadata.turn_id = turn_id;
            metadata.request_id = request_id;
            if (tool_call_id) {
                metadata.tool_call_id = agent::ui::ToolCallId{std::string(*tool_call_id)};
            }
            metadata.sequence = next_event_sequence_;
            metadata.timestamp_utc = std::chrono::system_clock::now();
            metadata.elapsed = now - session_started_at_;
            agent::ui::AgentEvent event{std::move(metadata), make_payload()};
            ++next_event_sequence_;
            observer_->on_event(event);
        } catch (...) {
            observer_enabled_ = false;
            try {
                observer_->on_observer_failure();
            } catch (...) {
                // Observer diagnostics must not affect the agent operation.
            }
        }
    }

    ToolRegistry& tools_;
    AgentConfig config_;
    std::vector<Message> history_;
    std::shared_ptr<agent::ui::EventObserver> observer_;
    bool observer_enabled_ = false;
    agent::ui::SessionId session_id_;
    std::chrono::steady_clock::time_point session_started_at_;
    std::uint64_t next_turn_id_ = 1;
    std::uint64_t next_request_id_ = 1;
    std::uint64_t next_event_sequence_ = 1;
};
