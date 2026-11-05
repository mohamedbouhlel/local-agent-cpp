#include "agent.hpp"
#include "filesystem_sandbox.hpp"
#include "http_client.hpp"
#include "prompt_reader.hpp"
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

std::string first_line(const std::string& text)
{
    const auto newline = text.find('\n');
    return newline == std::string::npos ? text : text.substr(0, newline);
}

} // namespace

int main()
{
    auto config = load_config();
    ToolRegistry tools;
    std::filesystem::path workspace_root;
    try {
        workspace_root = prepare_workspace_root(std::filesystem::absolute("workspace"));
        register_workspace_tools(tools, workspace_root);
        apply_filesystem_sandbox(workspace_root);
    } catch (const std::exception& error) {
        std::cerr << "Filesystem sandbox setup failed: " << error.what() << '\n';
        return 1;
    }

    Agent agent(tools, config);

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

        bool streamed_text = false;
        int tools_ok = 0;
        int tools_failed = 0;

        AgentCallbacks callbacks;
        callbacks.on_text_delta = [&streamed_text](const std::string& fragment) {
            std::cout << fragment;
            std::cout.flush();
            streamed_text = true;
        };
        callbacks.on_tool_call = [](const std::string& name) {
            std::cout << "[appel de " << name << "...]\n";
        };
        // Verdict d'exécution produit par le programme : il reflète ce qui a
        // réellement été exécuté (succès/échec de chaque outil), et non ce que
        // le modèle affirme avoir fait.
        callbacks.on_tool_result = [&tools_ok, &tools_failed](const ToolOutcome& outcome) {
            if (outcome.ok) {
                ++tools_ok;
            } else {
                ++tools_failed;
            }
            std::cout << "[" << outcome.name << " -> " << (outcome.ok ? "succes" : "ECHEC")
                      << (outcome.ok ? std::string() : " (" + first_line(outcome.result) + ")")
                      << "]\n";
        };

        try {
            agent.send(input, callbacks);
            std::cout << "\n";
            if (tools_ok + tools_failed > 0) {
                std::cout << "[execution: " << tools_ok << " outil(s) reussi(s), " << tools_failed
                          << " en echec]\n";
            }
        } catch (const HttpError& error) {
            // Si du texte a déjà été imprimé en streaming, l'erreur doit
            // démarrer sur sa propre ligne pour rester lisible.
            if (streamed_text) {
                std::cout << "\n";
            }
            std::cerr << "Impossible de contacter LLM_BASE_URL (" << config.base_url << ")"
                      << ": " << error.what() << "\n";
        } catch (const AgentError& error) {
            // Flux incomplet ou tool call invalide : le programme le signale
            // comme un échec d'exécution, sans présenter la réponse comme
            // définitive.
            if (streamed_text) {
                std::cout << "\n";
            }
            std::cerr << "Reponse incomplete ou invalide du modele: " << error.what() << "\n";
        } catch (const std::exception& error) {
            if (streamed_text) {
                std::cout << "\n";
            }
            std::cerr << "Reponse du serveur inattendue: " << error.what() << "\n";
        }
    }

    return 0;
}
