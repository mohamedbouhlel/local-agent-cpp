#include "agent.hpp"

#include "http_client.hpp"
#include "sse_parser.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr std::size_t max_history_messages = 20;
constexpr int max_tool_iterations = 5;

nlohmann::json messages_to_json(const std::vector<Message>& messages)
{
    auto json_messages = nlohmann::json::array();
    for (const auto& message : messages) {
        json_messages.push_back(message.data);
    }
    return json_messages;
}

nlohmann::json make_user_message(const std::string& content)
{
    return {
        {"role", "user"},
        {"content", content},
    };
}

nlohmann::json make_tool_message(const std::string& tool_call_id, const std::string& content)
{
    return {
        {"role", "tool"},
        {"tool_call_id", tool_call_id},
        {"content", content},
    };
}

nlohmann::json make_payload(const AgentConfig& config, const ToolRegistry& tools, const std::vector<Message>& history)
{
    auto payload = nlohmann::json{
        {"model", config.model},
        {"messages", messages_to_json(history)},
        {"stream", true},
    };

    if (!tools.empty()) {
        payload["tools"] = tools.schemas_json();
    }

    return payload;
}

struct ToolCallAccumulator {
    std::string id;
    std::string name;
    std::string arguments;
};

// Message assistant en cours de reconstruction depuis les deltas OpenAI. Les
// tool_calls sont indexés par leur champ "index" (std::map => ordre croissant
// déterministe), pour que des deltas entrelacés sur des index différents ne se
// mélangent pas.
struct StreamingMessageAccumulator {
    std::string content;
    std::map<std::size_t, ToolCallAccumulator> tool_calls;
};

bool has_tool_calls(const nlohmann::json& assistant_message)
{
    return assistant_message.contains("tool_calls")
        && assistant_message.at("tool_calls").is_array()
        && !assistant_message.at("tool_calls").empty();
}

void apply_tool_call_delta(StreamingMessageAccumulator& accumulator, const nlohmann::json& call_delta)
{
    if (!call_delta.is_object() || !call_delta.contains("index") || !call_delta.at("index").is_number_integer()) {
        // Sans index exploitable on ne peut pas rattacher le delta : on
        // l'ignore plutôt que de risquer de fusionner deux tool_calls.
        return;
    }

    const auto index_value = call_delta.at("index").get<long long>();
    if (index_value < 0) {
        return;
    }
    auto& call = accumulator.tool_calls[static_cast<std::size_t>(index_value)];

    // id arrive typiquement une fois : on l'assigne (pas de concaténation).
    if (call_delta.contains("id") && call_delta.at("id").is_string()) {
        call.id = call_delta.at("id").get<std::string>();
    }

    if (!call_delta.contains("function") || !call_delta.at("function").is_object()) {
        return;
    }

    const auto& function = call_delta.at("function");
    // name et arguments peuvent arriver fragmentés : on concatène dans l'ordre.
    if (function.contains("name") && function.at("name").is_string()) {
        call.name += function.at("name").get<std::string>();
    }
    if (function.contains("arguments") && function.at("arguments").is_string()) {
        call.arguments += function.at("arguments").get<std::string>();
    }
}

void apply_stream_delta(StreamingMessageAccumulator& accumulator,
                        const nlohmann::json& event,
                        const AgentCallbacks& callbacks)
{
    if (!event.contains("choices") || !event.at("choices").is_array() || event.at("choices").empty()) {
        return;
    }

    const auto& choice = event.at("choices").at(0);
    if (!choice.contains("delta") || !choice.at("delta").is_object()) {
        return;
    }
    const auto& delta = choice.at("delta");

    if (delta.contains("content") && delta.at("content").is_string()) {
        auto fragment = delta.at("content").get<std::string>();
        if (!fragment.empty()) {
            accumulator.content += fragment;
            if (callbacks.on_text_delta) {
                callbacks.on_text_delta(fragment);
            }
        }
    }

    if (delta.contains("tool_calls") && delta.at("tool_calls").is_array()) {
        for (const auto& call_delta : delta.at("tool_calls")) {
            apply_tool_call_delta(accumulator, call_delta);
        }
    }
}

// Assemble le message final au format attendu par le reste du code (identique à
// l'ancien extract_assistant_message() non streamé).
nlohmann::json build_assistant_message(const StreamingMessageAccumulator& accumulator)
{
    auto message = nlohmann::json{
        {"role", "assistant"},
        {"content", accumulator.content},
    };

    if (!accumulator.tool_calls.empty()) {
        auto tool_calls = nlohmann::json::array();
        for (const auto& [index, call] : accumulator.tool_calls) {
            static_cast<void>(index);
            tool_calls.push_back({
                {"id", call.id},
                {"type", "function"},
                {"function",
                 {
                     {"name", call.name},
                     // Un tool_call sans argument (deltas absents) doit rester
                     // un objet JSON valide pour l'exécution.
                     {"arguments", call.arguments.empty() ? "{}" : call.arguments},
                 }},
            });
        }
        message["tool_calls"] = std::move(tool_calls);
    }

    return message;
}

} // namespace

Agent::Agent(ToolRegistry& tools, AgentConfig config)
    : tools_(tools)
    , config_(std::move(config))
{
}

std::string Agent::send(const std::string& input, const AgentCallbacks& callbacks)
{
    append_message({make_user_message(input)});

    HttpClient::Headers headers = {
        "Content-Type: application/json",
    };
    if (!config_.api_key.empty()) {
        headers.push_back("Authorization: Bearer " + config_.api_key);
    }

    const HttpClient client;
    for (int iteration = 0; iteration < max_tool_iterations; ++iteration) {
        trim_history();

        const auto payload = make_payload(config_, tools_, history_);

        SseParser parser;
        StreamingMessageAccumulator accumulator;

        // Invariant : si post_streaming lève (coupure en plein flux, statut
        // non-2xx, ...), on sort de send() sans avoir rien ajouté à
        // l'historique pour ce tour — le message assistant partiel n'y figure
        // jamais. Le message user, lui, a déjà été ajouté avant la boucle.
        client.post_streaming(
            config_.base_url,
            headers,
            payload.dump(),
            config_.timeout_seconds,
            [&parser, &accumulator, &callbacks](const std::string& chunk) {
                for (const auto& data : parser.feed(chunk)) {
                    // "[DONE]" n'a pas de sens pour le parser SSE : on le
                    // reconnaît ici, côté OpenAI.
                    if (data == "[DONE]") {
                        continue;
                    }

                    nlohmann::json event;
                    try {
                        event = nlohmann::json::parse(data);
                    } catch (const nlohmann::json::exception&) {
                        // Fragment SSE isolé malformé : ignoré sans interrompre
                        // le reste du flux.
                        continue;
                    }

                    apply_stream_delta(accumulator, event, callbacks);
                }
            });

        const auto assistant_message = build_assistant_message(accumulator);

        if (!has_tool_calls(assistant_message)) {
            const auto content = assistant_message.at("content").get<std::string>();
            append_message({assistant_message});
            trim_history();
            return content;
        }

        append_message({assistant_message});
        for (const auto& tool_call : assistant_message.at("tool_calls")) {
            const auto tool_call_id = tool_call.at("id").get<std::string>();
            const auto name = tool_call.at("function").at("name").get<std::string>();
            const auto arguments = tool_call.at("function").value("arguments", "{}");

            if (callbacks.on_tool_call) {
                callbacks.on_tool_call(name);
            }

            std::string result;
            try {
                result = tools_.execute(name, arguments);
            } catch (const std::exception& error) {
                result = std::string("tool error: ") + error.what();
            }

            append_message({make_tool_message(tool_call_id, result)});
        }
    }

    throw std::runtime_error("tool call loop exceeded maximum iterations");
}

void Agent::append_message(Message message)
{
    history_.push_back(std::move(message));
}

void Agent::trim_history()
{
    if (history_.size() <= max_history_messages) {
        return;
    }

    std::vector<std::size_t> turn_starts;
    for (std::size_t index = 0; index < history_.size(); ++index) {
        if (history_[index].data.value("role", "") == "user") {
            turn_starts.push_back(index);
        }
    }

    std::size_t first_kept = 0;
    for (std::size_t turn = 0; turn + 1 < turn_starts.size(); ++turn) {
        if (history_.size() - turn_starts[turn] <= max_history_messages) {
            break;
        }
        first_kept = turn_starts[turn + 1];
    }

    if (first_kept > 0) {
        history_.erase(history_.begin(), history_.begin() + static_cast<std::vector<Message>::difference_type>(first_kept));
    }
}
