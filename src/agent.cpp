#include "agent.hpp"

#include "http_client.hpp"
#include "sse_parser.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <atomic>
#include <chrono>
#include <map>
#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

constexpr std::size_t max_history_messages = 20;
constexpr int max_tool_iterations = 5;

std::atomic<std::uint64_t> next_session_id{1};

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

// Mémorise le dernier finish_reason non vide rencontré ("stop", "length",
// "tool_calls", ...). Un flux sans finish_reason explicite reste accepté : la
// complétude est prouvée par le marqueur "[DONE]", pas par ce champ.
void collect_finish_reason(const nlohmann::json& event, std::string& finish_reason)
{
    if (!event.contains("choices") || !event.at("choices").is_array() || event.at("choices").empty()) {
        return;
    }

    const auto& choice = event.at("choices").at(0);
    if (choice.contains("finish_reason") && choice.at("finish_reason").is_string()) {
        finish_reason = choice.at("finish_reason").get<std::string>();
    }
}

void apply_stream_delta(StreamingMessageAccumulator& accumulator,
                        const nlohmann::json& event,
                        const std::function<void(const std::string&)>& on_text_delta)
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
            on_text_delta(fragment);
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

agent::ui::EventType agent::ui::AgentEvent::type() const noexcept
{
    return std::visit(
        [](const auto& value) noexcept {
            using Payload = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Payload, agent::ui::RequestStarted>) {
                return agent::ui::EventType::RequestStarted;
            } else if constexpr (std::is_same_v<Payload, agent::ui::HttpRequestStarted>) {
                return agent::ui::EventType::HttpRequestStarted;
            } else if constexpr (std::is_same_v<Payload, agent::ui::TextDelta>) {
                return agent::ui::EventType::TextDelta;
            } else if constexpr (std::is_same_v<Payload, agent::ui::ToolAuthorizationRequested>) {
                return agent::ui::EventType::ToolAuthorizationRequested;
            } else if constexpr (std::is_same_v<Payload, agent::ui::ToolAuthorizationResolved>) {
                return agent::ui::EventType::ToolAuthorizationResolved;
            } else if constexpr (std::is_same_v<Payload, agent::ui::ToolStarted>) {
                return agent::ui::EventType::ToolStarted;
            } else if constexpr (std::is_same_v<Payload, agent::ui::ToolSucceeded>) {
                return agent::ui::EventType::ToolSucceeded;
            } else if constexpr (std::is_same_v<Payload, agent::ui::ToolFailed>) {
                return agent::ui::EventType::ToolFailed;
            } else if constexpr (std::is_same_v<Payload, agent::ui::HttpRequestFinished>) {
                return agent::ui::EventType::HttpRequestFinished;
            } else if constexpr (std::is_same_v<Payload, agent::ui::RequestFailed>) {
                return agent::ui::EventType::RequestFailed;
            } else {
                return agent::ui::EventType::TurnFinished;
            }
        },
        payload);
}

Agent::Agent(ToolRegistry& tools, AgentConfig config)
    : Agent(tools, std::move(config), nullptr)
{
}

Agent::Agent(ToolRegistry& tools,
             AgentConfig config,
             std::shared_ptr<agent::ui::EventObserver> observer)
    : tools_(tools)
    , config_(std::move(config))
    , observer_(std::move(observer))
    , observer_enabled_(static_cast<bool>(observer_))
    , session_id_{next_session_id.fetch_add(1, std::memory_order_relaxed)}
    , session_started_at_(std::chrono::steady_clock::now())
{
}

std::string Agent::send(const std::string& input, const AgentCallbacks& callbacks)
{
    using namespace agent::ui;
    const TurnId turn_id{next_turn_id_++};
    std::uint32_t request_count = 0;
    std::uint32_t tools_succeeded = 0;
    std::uint32_t tools_failed = 0;
    bool legacy_callback_failed = false;
    bool turn_finished = false;
    bool iteration_limit_reached = false;
    std::optional<RequestId> current_request_id;

    const auto finish_turn = [&](TurnOutcome outcome) noexcept {
        if (turn_finished) {
            return;
        }
        turn_finished = true;
        publish_event(
            [&] { return EventPayload{TurnFinished{outcome, request_count, tools_succeeded, tools_failed}}; },
            turn_id);
    };

    try {
        publish_event([] { return EventPayload{RequestStarted{}}; }, turn_id);
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
            bool saw_done = false;
            std::string finish_reason;
            const auto request_body = payload.dump();
            const RequestId request_id{next_request_id_++};
            std::uint64_t response_bytes = 0;

            std::function<void(const std::string&)> on_text_delta = [&](const std::string& fragment) {
                publish_event([&] { return EventPayload{TextDelta{fragment}}; }, turn_id, request_id);
                if (callbacks.on_text_delta) {
                    try {
                        callbacks.on_text_delta(fragment);
                    } catch (...) {
                        legacy_callback_failed = true;
                        throw;
                    }
                }
            };

            // Consomme un payload SSE déjà délimité.
            auto consume = [&accumulator, &on_text_delta, &finish_reason, &saw_done](const std::string& data) {
                // "[DONE]" n'a pas de sens pour le parser SSE : on le reconnaît ici,
                // côté OpenAI, et il constitue la preuve de complétude du flux.
                if (data == "[DONE]") {
                    saw_done = true;
                    return;
                }

                nlohmann::json event;
                try {
                    event = nlohmann::json::parse(data);
                } catch (const nlohmann::json::exception&) {
                    // Fragment SSE isolé malformé : ignoré sans interrompre le
                    // reste du flux.
                    return;
                }

                collect_finish_reason(event, finish_reason);
                apply_stream_delta(accumulator, event, on_text_delta);
            };

            std::function<void(const std::string&)> on_chunk = [&parser, &consume, &response_bytes](
                                                                    const std::string& chunk) {
                response_bytes += static_cast<std::uint64_t>(chunk.size());
                for (const auto& data : parser.feed(chunk)) {
                    consume(data);
                }
            };

            current_request_id = request_id;
            ++request_count;
            publish_event([&] { return EventPayload{HttpRequestStarted{static_cast<std::uint32_t>(iteration + 1)}}; },
                          turn_id,
                          request_id);
            const auto request_started_at = std::chrono::steady_clock::now();
            try {
                // Invariant : le message assistant partiel reste hors historique
                // si le transport ou un callback legacy échoue.
                client.post_streaming(
                    config_.base_url,
                    headers,
                    request_body,
                    config_.timeout_seconds,
                    on_chunk);
            } catch (...) {
                publish_event(
                    [&] {
                        return EventPayload{HttpRequestFinished{
                            std::chrono::steady_clock::now() - request_started_at,
                            response_bytes,
                            legacy_callback_failed ? HttpRequestOutcome::legacy_callback_aborted
                                                   : HttpRequestOutcome::failed}};
                    },
                    turn_id,
                    request_id);
                throw;
            }
            publish_event(
                [&] {
                    return EventPayload{HttpRequestFinished{std::chrono::steady_clock::now() - request_started_at,
                                                            response_bytes,
                                                            HttpRequestOutcome::completed}};
                },
                turn_id,
                request_id);

            // Une trame finale non terminée par une ligne vide ne doit pas être
            // perdue silencieusement.
            for (const auto& data : parser.flush()) {
                consume(data);
            }

            if (!saw_done) {
                throw AgentError("incomplete model stream: missing [DONE] marker");
            }
            if (finish_reason == "length") {
                throw AgentError("model response truncated: finish_reason=length");
            }

            const auto assistant_message = build_assistant_message(accumulator);

            if (!has_tool_calls(assistant_message)) {
                const auto content = assistant_message.at("content").get<std::string>();
                append_message({assistant_message});
                trim_history();
                finish_turn(TurnOutcome::completed);
                return content;
            }

            // Valider tous les IDs avant de publier un démarrage d'outil ou de
            // modifier l'historique assistant/tool.
            for (const auto& tool_call : assistant_message.at("tool_calls")) {
                const auto id = tool_call.at("id").get<std::string>();
                const auto name = tool_call.at("function").at("name").get<std::string>();
                if (id.empty() || name.empty()) {
                    throw AgentError("malformed tool call from model: empty id or name");
                }
            }

            append_message({assistant_message});
            for (const auto& tool_call : assistant_message.at("tool_calls")) {
                const auto tool_call_id = tool_call.at("id").get<std::string>();
                const auto name = tool_call.at("function").at("name").get<std::string>();
                const auto arguments = tool_call.at("function").value("arguments", "{}");

                const auto tool_started_at = std::chrono::steady_clock::now();
                std::string result;
                bool ok = true;
                bool started = false;
                auto start_tool = [&] {
                    if (callbacks.on_tool_call) {
                        try { callbacks.on_tool_call(name); }
                        catch (...) { legacy_callback_failed = true; throw; }
                    }
                    publish_event([&] { return EventPayload{ToolStarted{name}}; }, turn_id, request_id, tool_call_id);
                    started = true;
                };
                try {
                    ToolInvocationContext invocation_context{session_id_.value, turn_id.value,
                                                             request_id.value, tool_call_id};
                    auto invocation = tools_.prepare_execution(name, arguments, invocation_context);
                    if (const auto approval = invocation->approval_request()) {
                        publish_event([&] { return EventPayload{ToolAuthorizationRequested{name}}; },
                                      turn_id, request_id, tool_call_id);
                        const auto decision = invocation->authorize();
                        const auto status = decision == ApprovalDecision::approved ? ToolAuthorizationStatus::approved
                            : decision == ApprovalDecision::denied ? ToolAuthorizationStatus::denied
                            : decision == ApprovalDecision::cancelled ? ToolAuthorizationStatus::cancelled
                            : decision == ApprovalDecision::expired ? ToolAuthorizationStatus::expired
                            : ToolAuthorizationStatus::unavailable;
                        publish_event([&] { return EventPayload{ToolAuthorizationResolved{status}}; },
                                      turn_id, request_id, tool_call_id);
                        if (decision != ApprovalDecision::approved) {
                            result = decision == ApprovalDecision::denied ? "write authorization denied; no file was created"
                                : decision == ApprovalDecision::cancelled ? "write authorization cancelled; no file was created"
                                : decision == ApprovalDecision::expired ? "write authorization expired; no file was created"
                                : "write authorization unavailable; no file was created";
                            append_message({make_tool_message(tool_call_id, result)});
                            continue;
                        }
                        (void)approval;
                    }
                    start_tool();
                    result = invocation->execute();
                } catch (const std::exception& error) {
                    if (legacy_callback_failed) throw;
                    if (!started) start_tool();
                    ok = false;
                    result = std::string("tool error: ") + error.what();
                } catch (...) {
                    if (legacy_callback_failed) throw;
                    // Preserve the historical propagation of non-standard
                    // handler exceptions while reporting this real execution
                    // failure to observers.
                    const auto tool_duration = std::chrono::steady_clock::now() - tool_started_at;
                    if (!started) start_tool();
                    ++tools_failed;
                    publish_event([&] { return EventPayload{ToolFailed{name, tool_duration}}; },
                                  turn_id,
                                  request_id,
                                  tool_call_id);
                    throw;
                }
                const auto tool_duration = std::chrono::steady_clock::now() - tool_started_at;
                if (ok) {
                    ++tools_succeeded;
                    publish_event([&] { return EventPayload{ToolSucceeded{name, tool_duration}}; },
                                  turn_id,
                                  request_id,
                                  tool_call_id);
                } else {
                    ++tools_failed;
                    publish_event([&] { return EventPayload{ToolFailed{name, tool_duration}}; },
                                  turn_id,
                                  request_id,
                                  tool_call_id);
                }

                // Le résultat historique reste complet pour les consommateurs
                // applicatifs; il n'est pas copié dans AgentEvent.
                if (callbacks.on_tool_result) {
                    try {
                        callbacks.on_tool_result(ToolOutcome{name, ok, result});
                    } catch (...) {
                        legacy_callback_failed = true;
                        throw;
                    }
                }

                append_message({make_tool_message(tool_call_id, result)});
            }
        }

        iteration_limit_reached = true;
        throw AgentError("tool call loop exceeded maximum iterations");
    } catch (const HttpError&) {
        const auto category = legacy_callback_failed ? RequestFailureCategory::legacy_callback
                                                      : RequestFailureCategory::http;
        publish_event([&] { return EventPayload{RequestFailed{category}}; }, turn_id, current_request_id);
        finish_turn(TurnOutcome::failed);
        throw;
    } catch (const AgentError&) {
        const auto category = legacy_callback_failed
                                  ? RequestFailureCategory::legacy_callback
                                  : (iteration_limit_reached ? RequestFailureCategory::iteration_limit
                                                             : RequestFailureCategory::protocol);
        publish_event([&] { return EventPayload{RequestFailed{category}}; }, turn_id, current_request_id);
        finish_turn(TurnOutcome::failed);
        throw;
    } catch (...) {
        const auto category = legacy_callback_failed ? RequestFailureCategory::legacy_callback
                                                     : RequestFailureCategory::unexpected;
        publish_event([&] { return EventPayload{RequestFailed{category}}; }, turn_id, current_request_id);
        finish_turn(TurnOutcome::failed);
        throw;
    }
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
