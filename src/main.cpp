#include "agent.hpp"
#include "filesystem_sandbox.hpp"
#include "http_client.hpp"
#include "prompt_reader.hpp"
#include "runtime_config.hpp"
#include "tool_registry.hpp"
#include "ui/console_renderer.hpp"
#include "ui/console_session.hpp"
#include "ui/presentation_controller.hpp"
#include "ui/terminal_capabilities.hpp"
#include "ui/tui_application.hpp"
#include "ui/tui_approval_broker.hpp"
#include "ui/ui_options.hpp"
#include "workspace_tools.hpp"

#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <unistd.h>

namespace {

std::string getenv_or_default(const char* name, std::string default_value)
{
    const char* value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        return default_value;
    }
    return value;
}

long positive_seconds_from_env(const char* name, long fallback)
{
    const auto value = getenv_or_default(name, std::to_string(fallback));
    if (const auto timeout = agent::config::parse_positive_seconds(value)) return *timeout;
    std::cerr << "Invalid " << name << " value, using " << fallback << " seconds.\n";
    return fallback;
}

AgentConfig load_config()
{
    const auto legacy = positive_seconds_from_env("LLM_TIMEOUT_SECONDS", 60);
    const auto connect = positive_seconds_from_env("LLM_CONNECT_TIMEOUT_SECONDS", std::min<long>(legacy, 10));
    const auto idle = positive_seconds_from_env("LLM_IDLE_TIMEOUT_SECONDS", legacy);
    const auto total = positive_seconds_from_env("LLM_TOTAL_TIMEOUT_SECONDS", 300);
    return {
        getenv_or_default("LLM_BASE_URL", "http://localhost:8080/v1/chat/completions"),
        getenv_or_default("LLM_MODEL", "local-model"),
        getenv_or_default("LLM_API_KEY", ""),
        legacy,
        HttpTimeouts{connect, idle, total},
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
    std::shared_ptr<agent::ui::TuiApprovalBroker> tui_approvals;
    std::shared_ptr<ApprovalProvider> approval_provider;
    if (interface_mode == agent::ui::InterfaceMode::tui) {
        tui_approvals = std::make_shared<agent::ui::TuiApprovalBroker>();
        approval_provider = tui_approvals;
    } else {
        approval_provider = std::make_shared<ConsoleWriteApprovalProvider>();
    }
    auto tools = std::make_shared<ToolRegistry>();
    std::filesystem::path workspace_root;
    try {
        workspace_root = prepare_workspace_root(std::filesystem::absolute("workspace"));
        register_workspace_tools(*tools, workspace_root, approval_provider);
    } catch (const std::exception& error) {
        std::cerr << "Filesystem sandbox setup failed: " << error.what() << '\n';
        return 1;
    }

    if (interface_mode == agent::ui::InterfaceMode::tui) {
        try {
            return agent::ui::TuiApplication(config, tools, tui_approvals, [workspace_root] {
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

    FdInputStreamBuffer stdin_buffer(STDIN_FILENO);
    std::istream input(&stdin_buffer);
    PromptReader reader(input, std::cout);
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
