#pragma once

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
};
