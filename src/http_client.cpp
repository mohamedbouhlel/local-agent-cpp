#include "http_client.hpp"

#include <curl/curl.h>

#include <functional>
#include <memory>
#include <string>

namespace {

class CurlGlobal {
public:
    CurlGlobal()
    {
        const auto code = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (code != CURLE_OK) {
            throw std::runtime_error("curl_global_init failed");
        }
    }

    ~CurlGlobal()
    {
        curl_global_cleanup();
    }

    CurlGlobal(const CurlGlobal&) = delete;
    CurlGlobal& operator=(const CurlGlobal&) = delete;
};

struct CurlHandleDeleter {
    void operator()(CURL* handle) const
    {
        if (handle != nullptr) {
            curl_easy_cleanup(handle);
        }
    }
};

struct CurlHeadersDeleter {
    void operator()(curl_slist* headers) const
    {
        if (headers != nullptr) {
            curl_slist_free_all(headers);
        }
    }
};

using CurlHandle = std::unique_ptr<CURL, CurlHandleDeleter>;
using CurlHeaders = std::unique_ptr<curl_slist, CurlHeadersDeleter>;

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* response = static_cast<std::string*>(userdata);
    response->append(ptr, size * nmemb);
    return size * nmemb;
}

struct StreamSink {
    std::string* body;
    const std::function<void(const std::string&)>* on_data;
};

size_t streaming_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    const auto total = size * nmemb;
    auto* sink = static_cast<StreamSink*>(userdata);
    sink->body->append(ptr, total);
    if (sink->on_data != nullptr && *sink->on_data) {
        (*sink->on_data)(std::string(ptr, total));
    }
    return total;
}

void ensure_curl_global()
{
    static const CurlGlobal global;
}

// Exécute le POST et renvoie le corps accumulé par write_function dans
// write_data. response_for_diagnostics référence ce même corps, pour
// construire le message d'erreur des statuts non-2xx (un serveur qui refuse
// la requête ne renvoie généralement pas de SSE, d'où l'accumulation
// systématique côté streaming).
std::string perform_request(const std::string& url,
                            const HttpClient::Headers& headers,
                            const std::string& body,
                            long timeout_seconds,
                            curl_write_callback write_function,
                            void* write_data,
                            const std::string& response_for_diagnostics)
{
    ensure_curl_global();

    CurlHandle curl(curl_easy_init());
    if (!curl) {
        throw std::runtime_error("failed to initialize curl handle");
    }

    CurlHeaders header_list(nullptr);
    for (const auto& header : headers) {
        auto* next = curl_slist_append(header_list.get(), header.c_str());
        if (next == nullptr) {
            throw std::runtime_error("failed to append HTTP header");
        }
        header_list.release();
        header_list.reset(next);
    }

    char error_buffer[CURL_ERROR_SIZE] = {};

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header_list.get());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_function);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, write_data);
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error_buffer);

    const auto code = curl_easy_perform(curl.get());
    if (code != CURLE_OK) {
        const std::string detail = error_buffer[0] != '\0' ? error_buffer : curl_easy_strerror(code);
        throw HttpError("HTTP POST failed: " + detail);
    }

    long status_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status_code);
    if (status_code < 200 || status_code >= 300) {
        throw HttpError("HTTP POST failed with status " + std::to_string(status_code) + ": " + response_for_diagnostics);
    }

    return response_for_diagnostics;
}

} // namespace

std::string HttpClient::post(const std::string& url,
                             const Headers& headers,
                             const std::string& body,
                             long timeout_seconds) const
{
    std::string response;
    perform_request(url, headers, body, timeout_seconds, write_callback, &response, response);
    return response;
}

std::string HttpClient::post_streaming(const std::string& url,
                                       const Headers& headers,
                                       const std::string& body,
                                       long timeout_seconds,
                                       const std::function<void(const std::string&)>& on_data) const
{
    std::string response;
    StreamSink sink{&response, &on_data};
    perform_request(url, headers, body, timeout_seconds, streaming_write_callback, &sink, response);
    return response;
}
