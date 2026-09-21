#include "agent.hpp"

#include "http_client.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <utility>

namespace {

constexpr std::size_t max_history_messages = 20;

nlohmann::json messages_to_json(const std::vector<Message>& messages)
{
    auto json_messages = nlohmann::json::array();
    for (const auto& message : messages) {
        json_messages.push_back({
            {"role", message.role},
            {"content", message.content},
        });
    }
    return json_messages;
}

std::string extract_assistant_content(const std::string& response_body)
{
    const auto response = nlohmann::json::parse(response_body);
    return response.at("choices").at(0).at("message").at("content").get<std::string>();
}

} // namespace

Agent::Agent(ToolRegistry& tools, AgentConfig config)
    : tools_(tools)
    , config_(std::move(config))
{
}

std::string Agent::send(const std::string& input)
{
    append_message({"user", input});

    const nlohmann::json payload = {
        {"model", config_.model},
        {"messages", messages_to_json(history_)},
        {"stream", false},
    };

    HttpClient::Headers headers = {
        "Content-Type: application/json",
    };
    if (!config_.api_key.empty()) {
        headers.push_back("Authorization: Bearer " + config_.api_key);
    }

    const HttpClient client;
    const auto response_body = client.post(config_.base_url, headers, payload.dump(), config_.timeout_seconds);
    const auto content = extract_assistant_content(response_body);

    append_message({"assistant", content});
    return content;
}

void Agent::append_message(Message message)
{
    history_.push_back(std::move(message));
    if (history_.size() > max_history_messages) {
        history_.erase(history_.begin(), history_.begin() + static_cast<std::vector<Message>::difference_type>(history_.size() - max_history_messages));
    }
}
