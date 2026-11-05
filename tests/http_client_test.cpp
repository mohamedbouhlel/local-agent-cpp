// Tests du client HTTP (include/http_client.hpp, src/http_client.cpp).
//
// HttpClient n'est pas testable sans réseau : les tests utilisent un serveur
// HTTP local sur loopback (MockHttpServer), jamais un LLM ni un service
// distant.

#include "http_client.hpp"
#include "mock_http_server.hpp"
#include "test_support.hpp"

#include <string>
#include <stdexcept>
#include <vector>

namespace {

HttpClient::Headers default_headers()
{
    return {"Content-Type: application/json"};
}

} // namespace

namespace {

struct StreamingCallbackError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

} // namespace

TEST(http, post_succes_retourne_le_corps)
{
    MockHttpServer server;
    server.enqueue(MockHttpServer::Response{200, "{\"ok\":true}", 0, 0, false, false});

    const HttpClient client;
    const auto body = client.post(server.url("/v1/chat/completions"), default_headers(), "{\"q\":1}", 5);

    CHECK_EQ(body, std::string("{\"ok\":true}"));
    CHECK_EQ(server.received_bodies().size(), std::size_t(1));
}

TEST(http, post_statut_non_2xx_leve_httperror_avec_le_corps)
{
    MockHttpServer server;
    const std::string payload = "{\"error\":{\"message\":\"bad request\"}}";
    server.enqueue(MockHttpServer::Response{400, payload, 0, 0, false, false});

    const HttpClient client;
    // Une seule requete : le message d'erreur est capture puis inspecte
    // (evite de consommer deux reponses scriptees pour un seul scenario).
    const auto message = tests::capture_error([&] {
        client.post(server.url("/v1/chat/completions"), default_headers(), "{}", 5);
    });

    CHECK(!message.empty());
    CHECK(message.find("HTTP POST failed with status 400") != std::string::npos);
    CHECK(message.find("bad request") != std::string::npos);
    CHECK_EQ(server.received_bodies().size(), std::size_t(1));
}

TEST(http, post_streaming_fragmente_et_accumule)
{
    MockHttpServer server;
    const std::string body = "data: un\n\ndata: deux\n\ndata: [DONE]\n\n";
    MockHttpServer::Response response;
    response.status = 200;
    response.body = body;
    response.piece_size = 3;
    response.piece_delay_us = 5000;
    server.enqueue(response);

    const HttpClient client;
    std::vector<std::string> fragments;
    const auto returned = client.post_streaming(server.url("/v1/chat/completions"), default_headers(), "{}", 5,
                                                [&fragments](const std::string& fragment) {
                                                    fragments.push_back(fragment);
                                                });

    std::string concatenated;
    for (const auto& fragment : fragments) {
        concatenated += fragment;
    }

    CHECK_EQ(returned, body);
    CHECK_EQ(concatenated, body);
    // Le corps est ecrit par morceaux de 3 octets avec delai : le callback doit
    // etre appele plusieurs fois (livraison progressive).
    CHECK(fragments.size() > 1);
}

TEST(http, post_echec_curl_connexion_sans_reponse)
{
    MockHttpServer server;
    MockHttpServer::Response response;
    response.abort = true; // ferme la connexion sans repondre
    server.enqueue(response);

    const HttpClient client;
    CHECK_THROWS_MSG(client.post(server.url("/v1/chat/completions"), default_headers(), "{}", 5),
                     "HTTP POST failed");
}

TEST(http, post_reponse_tronquee_leve_httperror)
{
    MockHttpServer server;
    MockHttpServer::Response response;
    response.status = 200;
    response.body = "data: partiel\n\n";
    response.truncated = true; // Content-Length annonce plus grand que le corps envoye
    server.enqueue(response);

    const HttpClient client;
    CHECK_THROWS_MSG(client.post(server.url("/v1/chat/completions"), default_headers(), "{}", 5),
                     "HTTP POST failed");
}

TEST(http, post_streaming_statut_non_2xx)
{
    MockHttpServer server;
    MockHttpServer::Response response;
    response.status = 500;
    response.body = "erreur serveur";
    server.enqueue(response);

    const HttpClient client;
    int callback_calls = 0;
    CHECK_THROWS_MSG(
        client.post_streaming(server.url("/v1/chat/completions"), default_headers(), "{}", 5,
                              [&callback_calls](const std::string&) { ++callback_calls; }),
        "HTTP POST failed with status 500");
}

TEST(http, timeout_distinct_des_autres_echecs_curl)
{
    // Phase R0 : un timeout (CURLE_OPERATION_TIMEDOUT) doit produire un message
    // distinct d'une coupure réseau. Le mock écrit le corps octet par octet avec
    // un délai : le client (timeout 1 s) abandonne avant la fin.
    MockHttpServer server;
    MockHttpServer::Response response;
    response.status = 200;
    response.body = "data: un\n\ndata: deux\n\n";
    response.piece_size = 1;
    response.piece_delay_us = 200000; // 200 ms par octet : dépasse largement 1 s
    server.enqueue(response);

    const HttpClient client;
    CHECK_THROWS_MSG(
        client.post_streaming(server.url("/v1/chat/completions"), default_headers(), "{}", 1,
                              [](const std::string&) {}),
        "HTTP POST timed out after 1s");
}

TEST(http, streaming_callback_exception_rethrown_without_type_or_message_change)
{
    MockHttpServer server;
    MockHttpServer::Response response;
    response.status = 200;
    response.body = "fragment de test assez long pour forcer plusieurs octets";
    response.piece_size = 1;
    response.piece_delay_us = 1000;
    server.enqueue(response);

    const HttpClient client;
    bool caught_original_type = false;
    try {
        (void)client.post_streaming(server.url("/v1/chat/completions"),
                                   default_headers(),
                                   "{}",
                                   5,
                                   [](const std::string&) { throw StreamingCallbackError("callback sentinel"); });
    } catch (const StreamingCallbackError& error) {
        caught_original_type = true;
        CHECK_EQ(std::string(error.what()), std::string("callback sentinel"));
    } catch (...) {
        CHECK(false);
    }
    CHECK(caught_original_type);
    CHECK_EQ(server.received_bodies().size(), std::size_t(1));
}
