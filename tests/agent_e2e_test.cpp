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
#include "test_approval_provider.hpp"
#include "ui/agent_event.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
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

// Flux texte minimal : un fragment de contenu, puis le marqueur de fin.
std::string text_stream(const std::string& text)
{
    return delta_content(text) + sse_done();
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

class DenyWriteApproval final : public ApprovalProvider {
public:
    ApprovalDecision request(std::shared_ptr<const WriteApprovalRequest> request) override
    {
        ++calls;
        last_request = std::move(request);
        return ApprovalDecision::denied;
    }
    void cancel_pending() noexcept override { cancelled = true; }
    unsigned calls = 0;
    bool cancelled = false;
    std::shared_ptr<const WriteApprovalRequest> last_request;
};

class EventRecorder final : public agent::ui::EventObserver {
public:
    void on_event(const agent::ui::AgentEvent& event) override { events.push_back(event); }
    void on_observer_failure() override { ++failures; }
    std::vector<agent::ui::AgentEvent> events;
    unsigned failures = 0;
};

} // namespace

// ---------------------------------------------------------------------------
// Test A — streaming : texte fragmenté sur plusieurs événements SSE
// ---------------------------------------------------------------------------

TEST(agent, texte_fragmente_livraison_progressive)
{
    tests::TempDir workspace("e2e_text_stream");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path(), test_approval_provider());

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
    register_workspace_tools(registry, workspace.path(), test_approval_provider());

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

// ---------------------------------------------------------------------------
// Test C — état réel des fichiers après exécution (Phase R0)
// ---------------------------------------------------------------------------

TEST(agent, write_file_verifie_l_etat_reel_du_fichier)
{
    // Le verdict de l'itération ne doit pas reposer sur le texte du modèle, mais
    // sur l'état réel du workspace après exécution.
    tests::TempDir workspace("e2e_write_state");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path(), test_approval_provider());

    MockHttpServer server;
    server.enqueue(stream_response(
        delta_tool_call(0, "call_w", {{"name", "write_file"}, {"arguments", ""}})
        + delta_tool_call(0, "", {{"arguments", "{\"path\":\"note"}})
        + delta_tool_call(0, "", {{"arguments", ".txt\",\"content\":"}})
        + delta_tool_call(0, "", {{"arguments", "\"bonjour monde\"}"}})
        + sse_done()));
    server.enqueue(stream_response(text_stream("ecrit")));

    Agent agent(registry, config_for(server));
    bool tool_ok = false;
    AgentCallbacks callbacks;
    callbacks.on_tool_result = [&tool_ok](const ToolOutcome& outcome) {
        tool_ok = outcome.ok;
    };

    CHECK_EQ(agent.send("ecris note.txt", callbacks), std::string("ecrit"));
    CHECK(tool_ok);

    // Preuve d'état final : lecture du fichier réellement présent sur le disque,
    // et non du texte du modèle ni du message de l'outil.
    std::ifstream file(workspace.path() / "note.txt", std::ios::binary);
    CHECK(file.good());
    const std::string on_disk((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    CHECK_EQ(on_disk, std::string("bonjour monde"));
}

TEST(agent, refus_write_file_est_transmis_au_modele_sans_creer_de_fichier)
{
    tests::TempDir workspace("e2e_write_denied");
    auto approval = std::make_shared<DenyWriteApproval>();
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path(), approval);
    auto observer = std::make_shared<EventRecorder>();

    MockHttpServer server;
    server.enqueue(stream_response(
        delta_tool_call(0, "call_denied", {{"name", "write_file"}, {"arguments", ""}})
        + delta_tool_call(0, "", {{"arguments", R"({"path":"new/secret.txt","content":"private"})"}})
        + sse_done()));
    server.enqueue(stream_response(text_stream("je poursuis sans ecriture")));

    Agent agent(registry, config_for(server), observer);
    unsigned legacy_results = 0;
    AgentCallbacks callbacks;
    callbacks.on_tool_result = [&legacy_results](const ToolOutcome&) { ++legacy_results; };
    CHECK_EQ(agent.send("cree un fichier", callbacks), std::string("je poursuis sans ecriture"));
    CHECK_EQ(approval->calls, 1u);
    CHECK(approval->last_request != nullptr);
    CHECK_EQ(approval->last_request->invocation.tool_call_id, std::string("call_denied"));
    CHECK(!std::filesystem::exists(workspace.path() / "new"));
    CHECK_EQ(legacy_results, 0u);
    CHECK_EQ(observer->failures, 0u);
    unsigned auth_requested = 0;
    unsigned auth_denied = 0;
    unsigned tool_started = 0;
    for (const auto& event : observer->events) {
        if (event.type() == agent::ui::EventType::ToolAuthorizationRequested) {
            ++auth_requested;
            CHECK(event.metadata.tool_call_id.has_value());
            CHECK_EQ(event.metadata.tool_call_id->value, std::string("call_denied"));
            CHECK_EQ(std::get<agent::ui::ToolAuthorizationRequested>(event.payload).name,
                     std::string("write_file"));
        } else if (event.type() == agent::ui::EventType::ToolAuthorizationResolved) {
            ++auth_denied;
            CHECK(std::get<agent::ui::ToolAuthorizationResolved>(event.payload).status
                  == agent::ui::ToolAuthorizationStatus::denied);
        } else if (event.type() == agent::ui::EventType::ToolStarted) {
            ++tool_started;
        }
    }
    CHECK_EQ(auth_requested, 1u);
    CHECK_EQ(auth_denied, 1u);
    CHECK_EQ(tool_started, 0u);

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() == 2) {
        const auto second = nlohmann::json::parse(bodies[1]);
        const auto* tool = first_with_role(second.at("messages"), "tool");
        CHECK(tool != nullptr);
        if (tool != nullptr) {
            CHECK_EQ(tool->at("tool_call_id").get<std::string>(), std::string("call_denied"));
            CHECK_EQ(tool->at("content").get<std::string>(),
                     std::string("write authorization denied; no file was created"));
        }
    }
}
