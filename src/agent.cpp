#include "agent.hpp"

#include "http_client.hpp"

#include <nlohmann/json.hpp>

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
        {"stream", false},
    };

    if (!tools.empty()) {
        payload["tools"] = tools.schemas_json();
    }

    return payload;
}

nlohmann::json extract_assistant_message(const std::string& response_body)
{
    const auto response = nlohmann::json::parse(response_body);
    return response.at("choices").at(0).at("message");
}

bool has_tool_calls(const nlohmann::json& assistant_message)
{
    return assistant_message.contains("tool_calls")
        && assistant_message.at("tool_calls").is_array()
        && !assistant_message.at("tool_calls").empty();
}

} // namespace

Agent::Agent(ToolRegistry& tools, AgentConfig config)
    : tools_(tools)
    , config_(std::move(config))
{
}

std::string Agent::send(const std::string& input)
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
        const auto response_body = client.post(config_.base_url, headers, payload.dump(), config_.timeout_seconds);
        const auto assistant_message = extract_assistant_message(response_body);

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
