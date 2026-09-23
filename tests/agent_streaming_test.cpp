// Tests de bout en bout d'Agent::send : reconstruction des tool_calls en
// streaming et gestion de l'historique.
//
// Agent, trim_history et les helpers de reconstruction (agent.cpp) ne sont pas
// accessibles directement (membres privés / namespace anonyme) et une
// évolution de la production serait nécessaire pour des tests unitaires
// directs. Ces comportements sont donc observés À TRAVERS l'interface publique
// Agent::send, en inspectant les requêtes réellement émises vers un serveur
// HTTP local (MockHttpServer). Aucun LLM réel n'est contacté.

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

// --- construction de flux SSE -------------------------------------------------

std::string sse(const nlohmann::json& payload)
{
    return "data: " + payload.dump() + "\n\n";
}

std::string sse_done()
{
    return "data: [DONE]\n\n";
}

std::string delta_content(const std::string& text)
{
    nlohmann::json event;
    event["choices"] = nlohmann::json::array(
        {nlohmann::json{{"index", 0}, {"delta", {{"content", text}}}}});
    return sse(event);
}

std::string text_stream(const std::string& text)
{
    return delta_content(text) + sse_done();
}

// index < 0 => le champ "index" est OMIS du delta.
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

// --- inspection des requêtes reçues ------------------------------------------

std::vector<std::string> roles_of(const nlohmann::json& messages)
{
    std::vector<std::string> roles;
    for (const auto& message : messages) {
        roles.push_back(message.value("role", "<absent>"));
    }
    return roles;
}

std::vector<std::string> contents_of_role(const nlohmann::json& messages, const std::string& role)
{
    std::vector<std::string> contents;
    for (const auto& message : messages) {
        if (message.value("role", "") == role) {
            contents.push_back(message.value("content", "<absent>"));
        }
    }
    return contents;
}

const nlohmann::json* assistant_with_tool_calls(const nlohmann::json& messages)
{
    for (const auto& message : messages) {
        if (message.value("role", "") == "assistant" && message.contains("tool_calls")
            && message.at("tool_calls").is_array() && !message.at("tool_calls").empty()) {
            return &message;
        }
    }
    return nullptr;
}

bool contains(const std::vector<std::string>& values, const std::string& needle)
{
    for (const auto& value : values) {
        if (value == needle) {
            return true;
        }
    }
    return false;
}

void prepare_workspace(const std::filesystem::path& root)
{
    std::ofstream(root / "a.txt", std::ios::binary) << "hello world\n";
}

} // namespace

// ---------------------------------------------------------------------------
// A. Reconstruction des tool_calls
// ---------------------------------------------------------------------------

TEST(agent, tool_call_unique_arguments_fragmentes)
{
    tests::TempDir workspace("agent_tool_single");
    prepare_workspace(workspace.path());
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    server.enqueue(stream_response(
        delta_tool_call(0, "call_abc", {{"name", "read_file"}, {"arguments", ""}})
        + delta_tool_call(0, "", {{"arguments", "{\"pa"}})
        + delta_tool_call(0, "", {{"arguments", "th\":\"a."}})
        + delta_tool_call(0, "", {{"arguments", "txt\"}"}})
        + sse_done()));
    server.enqueue(stream_response(text_stream("fini")));

    Agent agent(registry, config_for(server));
    std::vector<std::string> tool_names;
    std::string streamed;
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [&streamed](const std::string& fragment) { streamed += fragment; };
    callbacks.on_tool_call = [&tool_names](const std::string& name) { tool_names.push_back(name); };

    const auto reply = agent.send("lis le fichier", callbacks);

    CHECK_EQ(reply, std::string("fini"));
    CHECK_EQ(streamed, std::string("fini"));
    CHECK(tool_names == std::vector<std::string>{"read_file"});

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() != 2) {
        return;
    }

    // Requête 1 : le tour utilisateur.
    const auto first = nlohmann::json::parse(bodies[0]);
    CHECK(roles_of(first.at("messages")) == std::vector<std::string>{"user"});

    // Requête 2 : historique user -> assistant(tool_calls) -> tool.
    const auto second = nlohmann::json::parse(bodies[1]);
    const auto roles = roles_of(second.at("messages"));
    CHECK(roles == std::vector<std::string>({"user", "assistant", "tool"}));

    const auto* assistant = assistant_with_tool_calls(second.at("messages"));
    CHECK(assistant != nullptr);
    if (assistant != nullptr) {
        const auto& calls = assistant->at("tool_calls");
        CHECK_EQ(calls.size(), std::size_t(1));
        CHECK_EQ(calls.at(0).at("id").get<std::string>(), std::string("call_abc"));
        CHECK_EQ(calls.at(0).at("type").get<std::string>(), std::string("function"));
        CHECK_EQ(calls.at(0).at("function").at("name").get<std::string>(), std::string("read_file"));
        CHECK_EQ(calls.at(0).at("function").at("arguments").get<std::string>(),
                 std::string("{\"path\":\"a.txt\"}"));
    }

    const auto tool_contents = contents_of_role(second.at("messages"), "tool");
    CHECK(tool_contents == std::vector<std::string>{"hello world\n"});
}

TEST(agent, tool_calls_multiples_index_entrelaces)
{
    tests::TempDir workspace("agent_tool_multi");
    prepare_workspace(workspace.path());
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    server.enqueue(stream_response(
        delta_tool_call(0, "call_s", {{"name", "search_files"}, {"arguments", ""}})
        + delta_tool_call(1, "call_r", {{"name", "read_file"}, {"arguments", ""}})
        + delta_tool_call(0, "", {{"arguments", "{\"que"}})
        + delta_tool_call(1, "", {{"arguments", "{\"pa"}})
        + delta_tool_call(0, "", {{"arguments", "ry\":\"hel"}})
        + delta_tool_call(1, "", {{"arguments", "th\":\"a."}})
        + delta_tool_call(0, "", {{"arguments", "lo\"}"}})
        + delta_tool_call(1, "", {{"arguments", "txt\"}"}})
        + sse_done()));
    server.enqueue(stream_response(text_stream("ok")));

    Agent agent(registry, config_for(server));
    std::vector<std::string> tool_names;
    AgentCallbacks callbacks;
    callbacks.on_tool_call = [&tool_names](const std::string& name) { tool_names.push_back(name); };

    const auto reply = agent.send("cherche et lis", callbacks);
    CHECK_EQ(reply, std::string("ok"));
    // Les index sont restitués dans l'ordre croissant, sans mélange.
    CHECK(tool_names == std::vector<std::string>({"search_files", "read_file"}));

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() != 2) {
        return;
    }

    const auto second = nlohmann::json::parse(bodies[1]);
    const auto* assistant = assistant_with_tool_calls(second.at("messages"));
    CHECK(assistant != nullptr);
    if (assistant != nullptr) {
        const auto& calls = assistant->at("tool_calls");
        CHECK_EQ(calls.size(), std::size_t(2));
        if (calls.size() == 2) {
            CHECK_EQ(calls.at(0).at("function").at("name").get<std::string>(), std::string("search_files"));
            CHECK_EQ(calls.at(0).at("function").at("arguments").get<std::string>(),
                     std::string("{\"query\":\"hello\"}"));
            CHECK_EQ(calls.at(1).at("function").at("name").get<std::string>(), std::string("read_file"));
            CHECK_EQ(calls.at(1).at("function").at("arguments").get<std::string>(),
                     std::string("{\"path\":\"a.txt\"}"));
        }
    }

    const auto tool_contents = contents_of_role(second.at("messages"), "tool");
    CHECK(tool_contents == std::vector<std::string>({"a.txt:1:hello world\n", "hello world\n"}));
}

TEST(agent, arguments_absents_normalises_en_objet_vide)
{
    tests::TempDir workspace("agent_tool_noargs");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    // Aucun champ "arguments" dans le delta : les arguments reconstruits sont
    // vides et doivent être normalisés en "{}" pour rester exécutables.
    server.enqueue(stream_response(delta_tool_call(0, "call_l", {{"name", "list_files"}}) + sse_done()));
    server.enqueue(stream_response(text_stream("ok")));

    Agent agent(registry, config_for(server));
    const auto reply = agent.send("liste", AgentCallbacks{});
    CHECK_EQ(reply, std::string("ok"));

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() != 2) {
        return;
    }

    const auto second = nlohmann::json::parse(bodies[1]);
    const auto* assistant = assistant_with_tool_calls(second.at("messages"));
    CHECK(assistant != nullptr);
    if (assistant != nullptr) {
        CHECK_EQ(assistant->at("tool_calls").at(0).at("function").at("arguments").get<std::string>(),
                 std::string("{}"));
    }
}

TEST(agent, delta_tool_call_sans_index_ignore)
{
    tests::TempDir workspace("agent_tool_noindex");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    // Delta sans "index" : non rattachable, donc ignoré. Aucun tool_call n'est
    // reconstruit, le tour se termine comme une réponse texte vide.
    server.enqueue(stream_response(
        delta_tool_call(-1, "call_x", {{"name", "read_file"}, {"arguments", "{}"}}) + sse_done()));

    Agent agent(registry, config_for(server));
    int tool_callbacks = 0;
    AgentCallbacks callbacks;
    callbacks.on_tool_call = [&tool_callbacks](const std::string&) { ++tool_callbacks; };

    const auto reply = agent.send("delta invalide", callbacks);

    CHECK_EQ(reply, std::string(""));
    CHECK_EQ(tool_callbacks, 0);
    CHECK_EQ(server.received_bodies().size(), std::size_t(1));
}

TEST(agent, outil_inconnu_retourne_tool_error)
{
    tests::TempDir workspace("agent_unknown_tool");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    server.enqueue(stream_response(
        delta_tool_call(0, "call_n", {{"name", "nope_tool"}, {"arguments", "{}"}}) + sse_done()));
    server.enqueue(stream_response(text_stream("ok")));

    Agent agent(registry, config_for(server));
    const auto reply = agent.send("appelle un outil inconnu", AgentCallbacks{});
    CHECK_EQ(reply, std::string("ok"));

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() != 2) {
        return;
    }
    const auto second = nlohmann::json::parse(bodies[1]);
    const auto tool_contents = contents_of_role(second.at("messages"), "tool");
    CHECK(tool_contents == std::vector<std::string>{"tool error: unknown tool: nope_tool"});
}

TEST(agent, arguments_invalides_retourne_tool_error)
{
    tests::TempDir workspace("agent_bad_args");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    server.enqueue(stream_response(
        delta_tool_call(0, "call_b", {{"name", "read_file"}, {"arguments", "{pas du json"}}) + sse_done()));
    server.enqueue(stream_response(text_stream("ok")));

    Agent agent(registry, config_for(server));
    const auto reply = agent.send("arguments invalides", AgentCallbacks{});
    CHECK_EQ(reply, std::string("ok"));

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() != 2) {
        return;
    }
    const auto second = nlohmann::json::parse(bodies[1]);
    const auto tool_contents = contents_of_role(second.at("messages"), "tool");
    CHECK_EQ(tool_contents.size(), std::size_t(1));
    if (!tool_contents.empty()) {
        CHECK(tool_contents[0].find("tool error:") == 0);
        CHECK(tool_contents[0].find("parse_error") != std::string::npos);
    }
}

TEST(agent, tool_call_sans_id_ou_nom_est_refuse)
{
    // Constat PHASE 1 (risque R1) : la boucle de dispatch utilisait .at("id") et
    // .at("function").at("name") SANS validation. Un delta ne fournissant que
    // l'index produisait un tool call aux champs vides, dispatché comme outil
    // inconnu puis inscrit dans l'historique (id vide => échange non conforme au
    // protocole OpenAI, le modèle « croyant » avoir appelé un outil).
    //
    // Comportement corrigé (R0) : un tool call sans id ni nom exploitables est
    // refusé ; le tour n'ajoute AUCUN message assistant ni « tool » à
    // l'historique et n'exécute aucun outil.
    tests::TempDir workspace("agent_empty_fields");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    server.enqueue(stream_response(delta_tool_call(0, "", nlohmann::json::object()) + sse_done()));
    server.enqueue(stream_response(text_stream("ok")));

    Agent agent(registry, config_for(server));
    std::string error_message;
    try {
        agent.send("outil sans nom", AgentCallbacks{});
    } catch (const AgentError& error) {
        error_message = error.what();
    }

    CHECK(error_message.find("malformed tool call") != std::string::npos);
    // Une seule requête émise : le tool call invalide n'a déclenché aucune
    // exécution.
    CHECK_EQ(server.received_bodies().size(), std::size_t(1));

    // Le tour suivant fonctionne, et l'historique ne contient aucun message
    // assistant/tool issu du tour refusé.
    CHECK_EQ(agent.send("second", AgentCallbacks{}), std::string("ok"));

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() != 2) {
        return;
    }
    const auto second = nlohmann::json::parse(bodies[1]);
    CHECK(roles_of(second.at("messages")) == std::vector<std::string>({"user", "user"}));
    CHECK(contents_of_role(second.at("messages"), "assistant").empty());
    CHECK(contents_of_role(second.at("messages"), "tool").empty());
}

// ---------------------------------------------------------------------------
// C. Complétude du flux et état d'exécution (Phase R0)
// ---------------------------------------------------------------------------

TEST(agent, flux_sans_done_refuse_et_historique_preserve)
{
    // Défaut initial : un flux coupé avant "[DONE]" était accepté comme réponse
    // finale, ce qui faisait passer une réponse partielle pour un message
    // assistant complet (« correction annoncée » sans preuve d'exécution).
    tests::TempDir workspace("agent_no_done");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    // Deux fragments de texte, puis AUCUN "[DONE]" : le flux s'arrête.
    server.enqueue(stream_response(delta_content("Bon") + delta_content("jour")));
    server.enqueue(stream_response(text_stream("reprise")));

    Agent agent(registry, config_for(server));
    std::string error_message;
    int fragments = 0;
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [&fragments](const std::string&) { ++fragments; };

    try {
        agent.send("bonjour", callbacks);
    } catch (const AgentError& error) {
        error_message = error.what();
    }

    CHECK(error_message.find("incomplete model stream") != std::string::npos);
    // Le texte a bien été streamé avant la détection : le test est pertinent.
    CHECK(fragments > 0);

    // Le tour suivant réussit ; l'historique ne contient pas de message assistant
    // partiel (seuls les messages user y figurent).
    CHECK_EQ(agent.send("second", AgentCallbacks{}), std::string("reprise"));

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() != 2) {
        return;
    }
    const auto second = nlohmann::json::parse(bodies[1]);
    CHECK(roles_of(second.at("messages")) == std::vector<std::string>({"user", "user"}));
    CHECK(contents_of_role(second.at("messages"), "assistant").empty());
}

TEST(agent, reponse_tronquee_par_limite_de_tokens_refusee)
{
    // finish_reason="length" signifie que le modèle a été coupé par sa limite de
    // tokens : le message est partiel et ne doit pas être accepté comme définitif.
    tests::TempDir workspace("agent_length");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    nlohmann::json event;
    event["choices"] = nlohmann::json::array(
        {nlohmann::json{{"index", 0}, {"delta", {{"content", "debut de phrase"}}}, {"finish_reason", "length"}}});

    MockHttpServer server;
    server.enqueue(stream_response(sse(event) + sse_done()));

    Agent agent(registry, config_for(server));
    std::string error_message;
    try {
        agent.send("raconte", AgentCallbacks{});
    } catch (const AgentError& error) {
        error_message = error.what();
    }

    CHECK(error_message.find("finish_reason=length") != std::string::npos);
    CHECK_EQ(server.received_bodies().size(), std::size_t(1));
}

TEST(agent, derniere_trame_sans_ligne_vide_est_traitee)
{
    // Défaut initial : le parser SSE sans flush() abandonnait silencieusement la
    // dernière trame non terminée par une ligne vide. Ici, le "[DONE]" final
    // n'est pas suivi de "\n\n" : il doit tout de même être pris en compte.
    tests::TempDir workspace("agent_unterminated");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    server.enqueue(stream_response(delta_content("fini") + "data: [DONE]"));

    Agent agent(registry, config_for(server));
    CHECK_EQ(agent.send("termine", AgentCallbacks{}), std::string("fini"));
}

TEST(agent, callback_tool_result_expose_le_verdict_d_execution)
{
    // Le programme doit pouvoir établir un verdict d'exécution fondé sur ce qui a
    // réellement été exécuté, indépendamment du texte du modèle.
    tests::TempDir workspace("agent_tool_outcome");
    prepare_workspace(workspace.path());
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    server.enqueue(stream_response(
        delta_tool_call(0, "call_ok", {{"name", "read_file"}, {"arguments", "{\"path\":\"a.txt\"}"}})
        + delta_tool_call(1, "call_ko", {{"name", "nope_tool"}, {"arguments", "{}"}}) + sse_done()));
    server.enqueue(stream_response(text_stream("ok")));

    Agent agent(registry, config_for(server));
    std::vector<ToolOutcome> outcomes;
    AgentCallbacks callbacks;
    callbacks.on_tool_result = [&outcomes](const ToolOutcome& outcome) { outcomes.push_back(outcome); };

    CHECK_EQ(agent.send("lis et appelle un outil inconnu", callbacks), std::string("ok"));

    CHECK_EQ(outcomes.size(), std::size_t(2));
    if (outcomes.size() != 2) {
        return;
    }
    CHECK_EQ(outcomes[0].name, std::string("read_file"));
    CHECK(outcomes[0].ok);
    CHECK_EQ(outcomes[0].result, std::string("hello world\n"));
    CHECK_EQ(outcomes[1].name, std::string("nope_tool"));
    CHECK(!outcomes[1].ok);
    CHECK(outcomes[1].result.find("tool error:") == 0);
}

TEST(agent, deltas_non_conformes_ignores)
{
    tests::TempDir workspace("agent_odd_deltas");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    // choices vide, absence de "choices", content null, charge JSON invalide :
    // tous doivent être ignorés sans interrompre le flux.
    server.enqueue(stream_response(
        sse(nlohmann::json{{"choices", nlohmann::json::array()}})
        + sse(nlohmann::json{{"id", "chunk-sans-choices"}})
        + sse(nlohmann::json{{"choices", nlohmann::json::array(
              {nlohmann::json{{"index", 0}, {"delta", {{"content", nullptr}}}}})}})
        + "data: {json invalide\n\n"
        + delta_content("ok")
        + sse_done()));

    Agent agent(registry, config_for(server));
    std::string streamed;
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [&streamed](const std::string& fragment) { streamed += fragment; };

    const auto reply = agent.send("deltas bizarres", callbacks);
    CHECK_EQ(reply, std::string("ok"));
    CHECK_EQ(streamed, std::string("ok"));
}

// ---------------------------------------------------------------------------
// B. Historique
// ---------------------------------------------------------------------------

TEST(agent, historique_ordre_et_tours_complets)
{
    tests::TempDir workspace("agent_history_order");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    server.enqueue(stream_response(text_stream("un")));
    server.enqueue(stream_response(text_stream("deux")));

    Agent agent(registry, config_for(server));
    CHECK_EQ(agent.send("question-1", AgentCallbacks{}), std::string("un"));
    CHECK_EQ(agent.send("question-2", AgentCallbacks{}), std::string("deux"));

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() != 2) {
        return;
    }

    const auto first = nlohmann::json::parse(bodies[0]);
    CHECK(roles_of(first.at("messages")) == std::vector<std::string>{"user"});

    const auto second = nlohmann::json::parse(bodies[1]);
    CHECK(roles_of(second.at("messages")) == std::vector<std::string>({"user", "assistant", "user"}));
    CHECK(contents_of_role(second.at("messages"), "user")
          == std::vector<std::string>({"question-1", "question-2"}));
}

TEST(agent, historique_tronque_par_tours_complets)
{
    tests::TempDir workspace("agent_history_trim");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    const int turns = 12;
    MockHttpServer server;
    for (int turn = 0; turn < turns; ++turn) {
        server.enqueue(stream_response(text_stream("reponse-" + std::to_string(turn + 1))));
    }

    Agent agent(registry, config_for(server));
    for (int turn = 1; turn <= turns; ++turn) {
        CHECK_EQ(agent.send("tour-" + std::to_string(turn), AgentCallbacks{}),
                 std::string("reponse-" + std::to_string(turn)));
    }

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(turns));
    if (bodies.size() != static_cast<std::size_t>(turns)) {
        return;
    }

    // Invariant : le cap de 20 messages est respecte avant chaque requête, et
    // la troncature ne laisse jamais un tour incomplet en tête.
    for (const auto& body : bodies) {
        const auto request = nlohmann::json::parse(body);
        const auto messages = request.at("messages");
        CHECK(messages.size() <= 20);
        CHECK(!messages.empty());
        if (!messages.empty()) {
            CHECK_EQ(messages.at(0).value("role", "<absent>"), std::string("user"));
        }
    }

    // Dernier tour : les tours les plus anciens ont été retirés.
    const auto last = nlohmann::json::parse(bodies.back());
    const auto users = contents_of_role(last.at("messages"), "user");
    CHECK(!users.empty());
    CHECK(!contains(users, "tour-1"));
    CHECK(contains(users, "tour-" + std::to_string(turns)));
    // Un tour en cours se termine toujours par le message utilisateur courant.
    CHECK_EQ(roles_of(last.at("messages")).back(), std::string("user"));
    // Aucun message "tool" parasite dans un historique sans outil.
    CHECK(contents_of_role(last.at("messages"), "tool").empty());
}

TEST(agent, echec_en_plein_flux_najoute_pas_de_message_assistant)
{
    // Invariant documente : si post_streaming leve, le message assistant
    // partiellement reconstruit n'est JAMAIS ajoute a l'historique.
    tests::TempDir workspace("agent_partial");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    MockHttpServer::Response broken;
    broken.status = 200;
    broken.body = delta_content("Texte partiel");
    broken.truncated = true; // reponse tronquee => erreur curl cote client
    server.enqueue(broken);
    server.enqueue(stream_response(text_stream("reprise ok")));

    Agent agent(registry, config_for(server));

    int streamed_fragments = 0;
    std::string error_message;
    AgentCallbacks callbacks;
    callbacks.on_text_delta = [&streamed_fragments](const std::string&) { ++streamed_fragments; };

    try {
        agent.send("premier", callbacks);
    } catch (const std::exception& error) {
        error_message = error.what();
    }

    CHECK(error_message.find("HTTP POST failed") != std::string::npos);
    // Le texte partiel a bien ete streame avant l'echec : le test est pertinent.
    CHECK(streamed_fragments > 0);

    // Le tour suivant doit fonctionner, et l'historique ne doit PAS contenir de
    // message assistant issu du tour echoue.
    CHECK_EQ(agent.send("second", AgentCallbacks{}), std::string("reprise ok"));

    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(2));
    if (bodies.size() != 2) {
        return;
    }

    const auto second = nlohmann::json::parse(bodies[1]);
    CHECK(roles_of(second.at("messages")) == std::vector<std::string>({"user", "user"}));
    CHECK(contents_of_role(second.at("messages"), "user")
          == std::vector<std::string>({"premier", "second"}));
    CHECK(contents_of_role(second.at("messages"), "assistant").empty());
}

TEST(agent, boucle_outils_limitee_a_cinq_iterations)
{
    tests::TempDir workspace("agent_loop_cap");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());

    MockHttpServer server;
    // Le serveur repond TOUJOURS un tool_call : la boucle doit s'arreter au cap.
    for (int request = 0; request < 5; ++request) {
        server.enqueue(stream_response(
            delta_tool_call(0, "call_l", {{"name", "list_files"}, {"arguments", "{}"}}) + sse_done()));
    }

    Agent agent(registry, config_for(server));
    CHECK_THROWS_MSG(agent.send("boucle infinie", AgentCallbacks{}),
                     "tool call loop exceeded maximum iterations");
    CHECK_EQ(server.received_bodies().size(), std::size_t(5));
}
