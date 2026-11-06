#include "agent.hpp"
#include "filesystem_sandbox.hpp"
#include "http_client.hpp"
#include "prompt_reader.hpp"
#include "tool_registry.hpp"
#include "ui/console_renderer.hpp"
#include "ui/console_session.hpp"
#include "ui/presentation_controller.hpp"
#include "ui/terminal_capabilities.hpp"
#include "ui/tui_application.hpp"
#include "ui/ui_options.hpp"
#include "workspace_tools.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

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

int main(int argc, char** argv)
{
    agent::ui::InterfaceMode interface_mode;
    try {
        interface_mode = agent::ui::parse_interface_mode(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Invalid interface option: " << error.what() << '\n';
        return 2;
    }
    auto config = load_config();
    auto tools = std::make_shared<ToolRegistry>();
    std::filesystem::path workspace_root;
    try {
        workspace_root = prepare_workspace_root(std::filesystem::absolute("workspace"));
        register_workspace_tools(*tools, workspace_root);
    } catch (const std::exception& error) {
        std::cerr << "Filesystem sandbox setup failed: " << error.what() << '\n';
        return 1;
    }

    if (interface_mode == agent::ui::InterfaceMode::tui) {
        try {
            return agent::ui::TuiApplication(config, tools, [workspace_root] {
                apply_filesystem_sandbox(workspace_root);
            }).run();
        } catch (const std::exception& error) {
            std::cerr << "TUI failed: " << error.what() << '\n';
            return 1;
        }
    }

    try {
        apply_filesystem_sandbox(workspace_root);
    } catch (const std::exception& error) {
        std::cerr << "Filesystem sandbox setup failed: " << error.what() << '\n';
        return 1;
    }

    auto renderer = agent::ui::make_console_renderer(
        std::cout, std::cerr, agent::ui::detect_terminal_capabilities());
    auto presentation = std::make_shared<agent::ui::PresentationController>(std::move(renderer));
    Agent agent(*tools, config, presentation);

    std::cout << "agent-cpp ready. Type 'exit' to quit.\n";

    PromptReader reader(std::cin, std::cout);
    while (true) {
        const auto prompt = reader.read();

        // REPL-08 : la fin de flux termine le REPL. En multiline, le bloc en
        // cours est abandonne sans envoi ; il n'y a jamais de retour a l'invite
        // primaire apres un abandon.
        if (prompt.status == PromptStatus::Eof || prompt.status == PromptStatus::Aborted) {
            break;
        }

        // REPL-06 : un bloc de zero ligne n'est jamais transmis a l'agent.
        if (prompt.status == PromptStatus::EmptyBlock) {
            std::cout << "bloc vide ignore\n";
            continue;
        }

        // REPL-02 / REPL-07 : en mode simple uniquement, une ligne vide est
        // ignoree et "exit" (correspondance exacte, A-6) termine le REPL. En
        // multiline, ces lignes feraient partie du contenu du bloc.
        if (prompt.status == PromptStatus::Line) {
            if (prompt.content.empty()) {
                continue;
            }
            if (prompt.content == "exit") {
                break;
            }
        }

        const std::string& input = prompt.content;

        static_cast<void>(agent::ui::run_console_turn(agent, input, config.base_url, *presentation));
    }

    return 0;
}
