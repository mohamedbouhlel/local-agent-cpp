#include "mock_http_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

bool write_all(int fd, const std::string& data)
{
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto written = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (written <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(written);
    }
    return true;
}

std::string to_lower(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

std::string header_value(const std::string& headers, const std::string& name)
{
    const std::string lower_headers = to_lower(headers);
    const std::string lower_name = to_lower(name);
    const auto position = lower_headers.find(lower_name + ":");
    if (position == std::string::npos) {
        return {};
    }

    auto start = position + lower_name.size() + 1;
    while (start < headers.size() && (headers[start] == ' ' || headers[start] == '\t')) {
        ++start;
    }
    auto end = headers.find("\r\n", start);
    if (end == std::string::npos) {
        end = headers.size();
    }
    return headers.substr(start, end - start);
}

std::size_t parse_content_length(const std::string& value)
{
    try {
        return static_cast<std::size_t>(std::stoull(value));
    } catch (const std::exception&) {
        return 0;
    }
}

std::string status_text(int status)
{
    switch (status) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 408:
        return "Request Timeout";
    case 500:
        return "Internal Server Error";
    default:
        return "Status";
    }
}

// Etat controle pour une requete jamais completee : le client recoit une
// reponse explicite au lieu d'attendre indefiniment, et la connexion est fermee
// par l'appelant. La requete incomplete n'est PAS enregistree.
void respond_request_timeout(int connection)
{
    const std::string head =
        "HTTP/1.1 408 Request Timeout\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "Content-Length: 0\r\n\r\n";
    write_all(connection, head);
}

} // namespace

MockHttpServer::MockHttpServer(int read_timeout_ms)
    : read_timeout_ms_(read_timeout_ms > 0 ? read_timeout_ms : 2000)
{
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        throw std::runtime_error("mock: socket() failed");
    }

    int one = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        throw std::runtime_error("mock: bind() failed");
    }
    if (::listen(listen_fd_, 8) != 0) {
        throw std::runtime_error("mock: listen() failed");
    }

    socklen_t length = sizeof(address);
    if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        throw std::runtime_error("mock: getsockname() failed");
    }
    port_ = ntohs(address.sin_port);

    thread_ = std::thread([this] { run(); });
}

MockHttpServer::~MockHttpServer()
{
    stopped_ = true;
    // Reveille accept().
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
    }
    // Reveille aussi une lecture en cours sur la connexion active, sinon
    // l'arret attendrait le delai de lecture (voire jamais, avant correctif).
    const int active = active_connection_.load();
    if (active >= 0) {
        ::shutdown(active, SHUT_RDWR);
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
    }
}

std::string MockHttpServer::url(const std::string& path) const
{
    return "http://127.0.0.1:" + std::to_string(port_) + path;
}

void MockHttpServer::enqueue(Response response)
{
    std::lock_guard<std::mutex> lock(mutex_);
    responses_.push_back(std::move(response));
}

std::vector<std::string> MockHttpServer::received_bodies() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return bodies_;
}

std::size_t MockHttpServer::pending_responses() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return responses_.size();
}

void MockHttpServer::run()
{
    while (!stopped_) {
        sockaddr_in peer{};
        socklen_t length = sizeof(peer);
        const int connection = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&peer), &length);
        if (connection < 0) {
            break;
        }
        active_connection_ = connection;
        handle_connection(connection);
        active_connection_ = -1;
        ::close(connection);
    }
}

void MockHttpServer::handle_connection(int connection)
{
    int one = 1;
    ::setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    // Borne la lecture : un client silencieux ou une requete incomplete ne peut
    // plus immobiliser le serveur (mono-thread) indefiniment.
    const timeval read_timeout{read_timeout_ms_ / 1000, (read_timeout_ms_ % 1000) * 1000};
    ::setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));

    std::string buffer;
    char incoming[4096];
    std::size_t header_end = std::string::npos;
    std::size_t content_length = 0;
    bool continue_sent = false;

    while (true) {
        if (header_end == std::string::npos) {
            const auto found = buffer.find("\r\n\r\n");
            if (found != std::string::npos) {
                header_end = found;
                const std::string headers = buffer.substr(0, header_end);
                content_length = parse_content_length(header_value(headers, "Content-Length"));

                const std::string expect = header_value(headers, "Expect");
                if (!expect.empty() && expect.find("100-continue") != std::string::npos && !continue_sent) {
                    // curl attend "100 Continue" avant d'envoyer un corps volumineux.
                    write_all(connection, "HTTP/1.1 100 Continue\r\n\r\n");
                    continue_sent = true;
                }
            }
        }

        if (header_end != std::string::npos && buffer.size() >= header_end + 4 + content_length) {
            break;
        }

        const auto received = ::recv(connection, incoming, sizeof(incoming), 0);
        if (received == 0) {
            return; // client parti : rien à répondre
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue; // signal : on reprend la lecture (pas d'attente active)
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Delai de lecture depasse : requete jamais completee. Etat
                // controle : 408 explicite, aucun enregistrement.
                respond_request_timeout(connection);
                return;
            }
            return; // autre erreur reseau
        }
        buffer.append(incoming, static_cast<std::size_t>(received));
    }

    const std::string body = buffer.substr(header_end + 4, content_length);

    Response response;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bodies_.push_back(body);
        if (!responses_.empty()) {
            response = responses_.front();
            responses_.erase(responses_.begin());
        } else {
            response.status = 500;
        }
    }

    if (response.abort) {
        return; // fermeture sans réponse (test du chemin d'erreur curl)
    }

    const std::size_t declared = response.truncated ? response.body.size() + 100000 : response.body.size();

    std::string head = "HTTP/1.1 " + std::to_string(response.status) + " " + status_text(response.status) + "\r\n";
    head += "Content-Type: text/event-stream\r\n";
    head += "Connection: close\r\n";
    head += "Content-Length: " + std::to_string(declared) + "\r\n\r\n";

    if (!write_all(connection, head)) {
        return;
    }

    if (response.initial_delay_us > 0) ::usleep(static_cast<useconds_t>(response.initial_delay_us));

    if (response.piece_size == 0) {
        write_all(connection, response.body);
        return;
    }

    for (std::size_t offset = 0; offset < response.body.size(); offset += response.piece_size) {
        if (!write_all(connection, response.body.substr(offset, response.piece_size))) {
            return;
        }
        if (response.piece_delay_us > 0) {
            ::usleep(static_cast<useconds_t>(response.piece_delay_us));
        }
    }
}
