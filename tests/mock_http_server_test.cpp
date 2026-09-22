// Tests du mock HTTP lui-meme, cibles sur le constat R-01 du rapport
// docs/reports/PHASE_1_1_REVIEW_REPORT.md : le serveur etait mono-thread et
// pouvait rester bloque indefiniment sur une connexion silencieuse ou une
// requete incomplete.
//
// Aucun LLM ni service distant : tout se passe sur loopback.
// Les sockets clients bruts portent un SO_RCVTIMEO explicite afin qu'une
// regression fasse ECHOUER le test au lieu de le faire pendre.

#include "http_client.hpp"
#include "mock_http_server.hpp"
#include "test_support.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

// Client TCP brut : permet d'envoyer une requete volontairement incomplete, ou
// rien du tout, ce qu'HttpClient ne sait pas faire.
class RawClient {
public:
    RawClient(const MockHttpServer& server, int recv_timeout_ms)
        : fd_(::socket(AF_INET, SOCK_STREAM, 0))
    {
        if (fd_ < 0) {
            throw std::runtime_error("raw socket() failed");
        }

        const timeval timeout{recv_timeout_ms / 1000, (recv_timeout_ms % 1000) * 1000};
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(static_cast<unsigned short>(server.port()));
        if (::connect(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            ::close(fd_);
            throw std::runtime_error("raw connect() failed");
        }
    }

    ~RawClient()
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    RawClient(const RawClient&) = delete;
    RawClient& operator=(const RawClient&) = delete;

    void send(const std::string& data) const
    {
        ::send(fd_, data.data(), data.size(), 0);
    }

    // Lit jusqu'a fermeture par le serveur, borne par SO_RCVTIMEO.
    std::string read() const
    {
        std::string response;
        char buffer[512];
        while (true) {
            const auto received = ::recv(fd_, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                break;
            }
            response.append(buffer, static_cast<std::size_t>(received));
        }
        return response;
    }

private:
    int fd_ = -1;
};

long milliseconds_since(const std::chrono::steady_clock::time_point& start)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

HttpClient::Headers json_headers()
{
    return {"Content-Type: application/json"};
}

} // namespace

TEST(mock, requete_valide_toujours_servie)
{
    MockHttpServer server;
    server.enqueue(MockHttpServer::Response{200, "{\"ok\":true}", 0, 0, false, false});

    const HttpClient client;
    const auto body = client.post(server.url("/v1/chat/completions"), json_headers(), "{}", 5);

    CHECK_EQ(body, std::string("{\"ok\":true}"));
    CHECK_EQ(server.received_bodies().size(), std::size_t(1));
    CHECK_EQ(server.pending_responses(), std::size_t(0));
}

TEST(mock, connexion_silencieuse_recoit_408_sans_bloquer)
{
    MockHttpServer server(300); // delai de lecture court et explicite
    const RawClient silent(server, 3000);
    // Aucune donnee envoyee : la reponse doit arriver malgre tout (bornee a 3 s).
    const auto response = silent.read();

    CHECK(response.find("408") != std::string::npos);
    CHECK(response.find("Request Timeout") != std::string::npos);

    // Le serveur doit rester exploitable pour la requete suivante.
    server.enqueue(MockHttpServer::Response{200, "sain", 0, 0, false, false});
    const HttpClient client;
    CHECK_EQ(client.post(server.url("/v1/chat/completions"), json_headers(), "{}", 5), std::string("sain"));

    // La requete jamais completee n'a pas ete enregistree : seule la valide l'est.
    CHECK_EQ(server.received_bodies().size(), std::size_t(1));
}

TEST(mock, requete_incomplete_scenario_r01_ne_bloque_pas)
{
    // Reproduction exacte de la sonde R-01 : en-tete complet, Content-Length
    // annonce a 100 octets, un seul octet de corps envoye, jamais la suite.
    MockHttpServer server(300);
    const RawClient partial(server, 3000);
    partial.send("POST /v1/chat/completions HTTP/1.1\r\nContent-Length: 100\r\n\r\n{");

    const auto response = partial.read();
    CHECK(response.find("408") != std::string::npos);

    // La connexion suivante est servie normalement (avant correctif : le
    // serveur restait bloque et ce client expirait).
    server.enqueue(MockHttpServer::Response{200, "apres-coup", 0, 0, false, false});
    const HttpClient client;
    CHECK_EQ(client.post(server.url("/v1/chat/completions"), json_headers(), "{}", 5),
             std::string("apres-coup"));
    CHECK_EQ(server.received_bodies().size(), std::size_t(1));
    CHECK_EQ(server.pending_responses(), std::size_t(0));
}

TEST(mock, arret_prompt_apres_scenario_de_timeout)
{
    // Le serveur a deja clos la connexion silencieuse (408 recu) : l'arret doit
    // etre immediat.
    auto server = std::make_unique<MockHttpServer>(300);
    {
        const RawClient silent(*server, 3000);
        CHECK(silent.read().find("408") != std::string::npos);
    }

    const auto start = std::chrono::steady_clock::now();
    server.reset();
    const auto elapsed = milliseconds_since(start);
    CHECK(elapsed < 1500);
    std::cout << "    (arret apres timeout : " << elapsed << " ms)\n";
}

TEST(mock, arret_prompt_pendant_une_lecture_bloquee)
{
    // Delai de lecture volontairement long (5 s) : sans reveil de la connexion
    // active au destructeur, l'arret attendrait ce delai voire jamais.
    auto server = std::make_unique<MockHttpServer>(5000);
    const RawClient silent(*server, 1000);
    // Laisse le serveur accepter la connexion (sinon le test perdrait sa portee).
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto start = std::chrono::steady_clock::now();
    server.reset();
    const auto elapsed = milliseconds_since(start);

    CHECK(elapsed < 1500);
    std::cout << "    (arret pendant lecture bloquee : " << elapsed << " ms)\n";
}
