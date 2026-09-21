#include "agent.hpp"
#include "http_client.hpp"
#include "tool_registry.hpp"
#include "workspace_tools.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::string getenv_or_default(const char* name, std::string default_value)
{
    const char* value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        return default_value;
    }
    return value;
}

long timeout_from_env()
{
    const auto value = getenv_or_default("LLM_TIMEOUT_SECONDS", "60");
    try {
        const auto timeout = std::stol(value);
        if (timeout <= 0) {
            throw std::invalid_argument("timeout must be positive");
        }
        return timeout;
    } catch (const std::exception&) {
        std::cerr << "Invalid LLM_TIMEOUT_SECONDS value, using 60 seconds.\n";
        return 60;
    }
}

AgentConfig load_config()
{
    return {
        getenv_or_default("LLM_BASE_URL", "http://localhost:8080/v1/chat/completions"),
        getenv_or_default("LLM_MODEL", "local-model"),
        getenv_or_default("LLM_API_KEY", ""),
        timeout_from_env(),
    };
}

} // namespace

int main()
{
    auto config = load_config();
    ToolRegistry tools;
    const auto workspace_root = std::filesystem::absolute("workspace");
    std::filesystem::create_directories(workspace_root);
    register_workspace_tools(tools, workspace_root);

    Agent agent(tools, config);

    std::cout << "agent-cpp ready. Type 'exit' to quit.\n";

    std::string input;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, input) || input == "exit") {
            break;
        }

        if (input.empty()) {
            continue;
        }

        try {
            std::cout << agent.send(input) << "\n";
        } catch (const HttpError& error) {
            std::cerr << "Impossible de contacter LLM_BASE_URL (" << config.base_url << ")"
                      << ": " << error.what() << "\n";
        } catch (const std::exception& error) {
            std::cerr << "Reponse du serveur inattendue: " << error.what() << "\n";
        }
    }

    return 0;
}
