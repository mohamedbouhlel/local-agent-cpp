#pragma once

#include <string>
#include <vector>

class HttpClient {
public:
    using Headers = std::vector<std::string>;

    std::string post(const std::string& url,
                     const Headers& headers,
                     const std::string& body,
                     long timeout_seconds) const;
};
