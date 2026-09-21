#include "http_client.hpp"

#include <curl/curl.h>

#include <memory>
#include <stdexcept>
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

void ensure_curl_global()
{
    static const CurlGlobal global;
}

} // namespace

std::string HttpClient::post(const std::string& url,
                             const Headers& headers,
                             const std::string& body,
                             long timeout_seconds) const
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

    std::string response;
    char error_buffer[CURL_ERROR_SIZE] = {};

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header_list.get());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error_buffer);

    const auto code = curl_easy_perform(curl.get());
    if (code != CURLE_OK) {
        const std::string detail = error_buffer[0] != '\0' ? error_buffer : curl_easy_strerror(code);
        throw std::runtime_error("HTTP POST failed: " + detail);
    }

    long status_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status_code);
    if (status_code < 200 || status_code >= 300) {
        throw std::runtime_error("HTTP POST failed with status " + std::to_string(status_code) + ": " + response);
    }

    return response;
}
