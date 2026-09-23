// Tests de bout en bout d'Agent::send sur un mock HTTP/SSE local (MockHttpServer,
// tests/mock_http_server.*) : scénarios déterministes de l'itération P1.2.
//
// Aucun LLM réel ni service distant : tout se passe sur loopback. Les deux
// scénarios couvrent :
//   A. streaming — une réponse texte découpée en plusieurs événements SSE est
//      livrée fragment par fragment au callback, et le contenu final est
//      exactement la concaténation attendue ;
//   B. tool-calling — un tool call dont les champs sont fragmentés sur
//      plusieurs événements est reconstruit, l'outil est exécuté, et le
//      résultat est renvoyé au modèle dans la requête suivante avec le
//      tool_call_id correspondant.
//
// Les requêtes réellement émises sont inspectées (messages, résultat d'outil,
// paramètre stream). Complète tests/agent_streaming_test.cpp sans le modifier.

#include "agent.hpp"
#include "mock_http_server.hpp"
#include "test_support.hpp"
#include "tool_registry.hpp"
#include "workspace_tools.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string sse(const nlohmann::json& payload)
{
    return "data: " + payload.dump() + "\n\n";
}

std::string sse_done()
{
    return "data: [DONE]\n\n";
}

// Événement OpenAI portant un fragment de texte assistant.
std::string delta_content(const std::string& text)
{
    nlohmann::json event;
    event["choices"] = nlohmann::json::array(
        {nlohmann::json{{"index", 0}, {"delta", {{"content", text}}}}});
    return sse(event);
}

// Événement OpenAI portant un fragment de tool_call. index < 0 omet "index".
std::string delta_tool_call(int index, const std::string& id, const nlohmann::json& function)
{
    nlohmann::json call = nlohmann::json::object();
    if (index >= 0) {
        call["index"] = index;
    }
    if (!id.empty()) {
        call["id"] = id;
    }
    if (!function.is_null()) {
        call["function"] = function;
    }

    nlohmann::json delta = nlohmann::json::object();
    delta["tool_calls"] = nlohmann::json::array({call});

    nlohmann::json event;
    event["choices"] = nlohmann::json::array({nlohmann::json{{"index", 0}, {"delta", delta}}});
    return sse(event);
}

MockHttpServer::Response stream_response(const std::string& body)
{
    MockHttpServer::Response response;
    response.status = 200;
    response.body = body;
    return response;
}

AgentConfig config_for(const MockHttpServer& server)
{
    AgentConfig config;
    config.base_url = server.url("/v1/chat/completions");
    config.model = "local-model";
    config.api_key = "";
    config.timeout_seconds = 5;
    return config;
}

void prepare_workspace(const std::filesystem::path& root)
{
    std::ofstream(root / "a.txt", std::ios::binary) << "hello world\n";
}

std::vector<std::string> roles_of(const nlohmann::json& messages)
{
    std::vector<std::string> roles;
    for (const auto& message : messages) {
        roles.push_back(message.value("role", "<absent>"));
    }
    return roles;
}

const nlohmann::json* first_with_role(const nlohmann::json& messages, const std::string& role)
{
    for (const auto& message : messages) {
        if (message.value("role", "") == role) {
            return &message;
        }
    }
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// Test A — streaming : texte fragmenté sur plusieurs événements SSE
// ---------------------------------------------------------------------------

TEST(agent, texte_fragmente_livraison_progressive)
{
    tests::TempDir workspace("e2e_text_stream");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    const std::vector<std::string> fragments = {"Bon", "jour", ", ", "monde", " !"};

    MockHttpServer server;
    std::string body;
    for (const auto& fragment : fragments) {
        body += delta_content(fragment);
    }
    body += sse_done();
    server.enqueue(stream_response(body));

    Agent agent(registry, config_for(server));

    std::vector<std::string> received;
    std::string streamed;
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [&received, &streamed](const std::string& fragment) {
        received.push_back(fragment);
        streamed += fragment;
    };

    const auto reply = agent.send("dis bonjour", callbacks);

    // Le contenu final est exactement la concaténation des fragments...
    const std::string expected = "Bonjour, monde !";
    CHECK_EQ(reply, expected);
    CHECK_EQ(streamed, expected);
    // ...et chaque fragment a été livré individuellement, dans l'ordre exact.
    CHECK(received == fragments);

    // Une seule requête : réponse texte, aucun appel d'outil.
    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(1));
    if (bodies.size() != 1) {
        return;
    }

    const auto request = nlohmann::json::parse(bodies[0]);
    CHECK_EQ(request.at("model").get<std::string>(), std::string("local-model"));
    CHECK(request.at("stream").is_boolean());
    CHECK_EQ(request.at("stream").get<bool>(), true);
    CHECK(roles_of(request.at("messages")) == std::vector<std::string>{"user"});
    CHECK_EQ(request.at("messages").at(0).at("content").get<std::string>(), std::string("dis bonjour"));
}

// ---------------------------------------------------------------------------
// Test B — tool-calling : tool call fragmenté, exécution, requête de suivi
// ---------------------------------------------------------------------------

TEST(agent, tool_call_fragmente_bout_en_bout)
{
    tests::TempDir workspace("e2e_tool_call");
    prepare_workspace(workspace.path());
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    server.enqueue(stream_response(
        delta_tool_call(0, "call_e2e", {{"name", "read_file"}, {"arguments", ""}})
        + delta_tool_call(0, "", {{"arguments", "{\"path\":"}})
        + delta_tool_call(0, "", {{"arguments", "\"a.txt"}})
        + delta_tool_call(0, "", {{"arguments", "\"}"}})
        + sse_done()));
    server.enqueue(stream_response(delta_content("voici") + delta_content(" le contenu") + sse_done()));

    Agent agent(registry, config_for(server));

    std::vector<std::string> tool_names;
    AgentCallbacks callbacks;
    callbacks.on_tool_call = [&tool_names](const std::string& name) { tool_names.push_back(name); };

    const auto reply = agent.send("lis a.txt", callbacks);

    // Le modèle a demandé read_file, l'outil a été exécuté, puis le modèle a
    // produit la réponse finale.
    CHECK_EQ(reply, std::string("voici le contenu"));
    CHECK(tool_names == std::vector<std::string>{"read_file"});

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() != 2) {
        return;
    }

    // Requête 1 : tour utilisateur, streaming activé.
    const auto first = nlohmann::json::parse(bodies[0]);
    CHECK_EQ(first.at("stream").get<bool>(), true);
    CHECK(roles_of(first.at("messages")) == std::vector<std::string>{"user"});

    // Requête 2 : le tool call fragmenté a été reconstruit, l'outil exécuté, et
    // son résultat renvoyé au modèle avec le tool_call_id correspondant.
    const auto second = nlohmann::json::parse(bodies[1]);
    CHECK_EQ(second.at("stream").get<bool>(), true);
    CHECK(roles_of(second.at("messages")) == std::vector<std::string>({"user", "assistant", "tool"}));

    const nlohmann::json* assistant = first_with_role(second.at("messages"), "assistant");
    CHECK(assistant != nullptr);
    if (assistant != nullptr && assistant->contains("tool_calls")) {
        const auto& calls = assistant->at("tool_calls");
        CHECK_EQ(calls.size(), std::size_t(1));
        if (calls.size() == 1) {
            CHECK_EQ(calls.at(0).at("id").get<std::string>(), std::string("call_e2e"));
            CHECK_EQ(calls.at(0).at("type").get<std::string>(), std::string("function"));
            CHECK_EQ(calls.at(0).at("function").at("name").get<std::string>(), std::string("read_file"));
            CHECK_EQ(calls.at(0).at("function").at("arguments").get<std::string>(),
                     std::string("{\"path\":\"a.txt\"}"));
        }
    } else {
        CHECK(assistant != nullptr && assistant->contains("tool_calls"));
    }

    const nlohmann::json* tool = first_with_role(second.at("messages"), "tool");
    CHECK(tool != nullptr);
    if (tool != nullptr) {
        CHECK_EQ(tool->at("tool_call_id").get<std::string>(), std::string("call_e2e"));
        CHECK_EQ(tool->at("content").get<std::string>(), std::string("hello world\n"));
    }
}
