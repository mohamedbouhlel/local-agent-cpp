#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

struct HttpError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class HttpClient {
public:
    using Headers = std::vector<std::string>;

    std::string post(const std::string& url,
                     const Headers& headers,
                     const std::string& body,
                     long timeout_seconds) const;

    // Variante streaming : on_data est appelé pour chaque fragment reçu, et le
    // corps complet accumulé est retourné (diagnostic d'erreur, symétrie avec
    // post()). Mêmes garanties d'erreur que post().
    std::string post_streaming(const std::string& url,
                               const Headers& headers,
                               const std::string& body,
                               long timeout_seconds,
                               const std::function<void(const std::string&)>& on_data) const;
};
