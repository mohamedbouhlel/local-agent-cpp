#include "sse_parser.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace {

// Index du premier caractère du séparateur de fin d'événement (ligne vide)
// dans buffer, ou npos si aucun événement complet n'est disponible. Accepte
// "\n\n" et "\r\n\r\n". Un '\n' final isolé n'est pas un séparateur : on
// attend la suite du flux.
std::size_t find_event_end(const std::string& buffer)
{
    for (std::size_t index = 0; index + 1 < buffer.size(); ++index) {
        if (buffer[index] != '\n') {
            continue;
        }

        std::size_t second = index + 1;
        if (buffer[second] == '\r') {
            ++second;
        }
        if (second < buffer.size() && buffer[second] == '\n') {
            return index;
        }
    }
    return std::string::npos;
}

// Extrait les payloads "data:" d'un événement déjà délimité. Les lignes de
// commentaire (':'), les autres champs (event:, id:, retry:) et les lignes
// vides internes sont ignorés silencieusement.
std::vector<std::string> parse_event(const std::string& event)
{
    std::vector<std::string> data_lines;

    std::size_t start = 0;
    while (start <= event.size()) {
        const auto newline = event.find('\n', start);
        auto line = event.substr(start, newline == std::string::npos ? std::string::npos : newline - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.rfind("data:", 0) == 0) {
            auto value = line.substr(5);
            if (!value.empty() && value.front() == ' ') {
                value.erase(0, 1);
            }
            data_lines.push_back(std::move(value));
        }

        if (newline == std::string::npos) {
            break;
        }
        start = newline + 1;
    }

    if (data_lines.empty()) {
        return {};
    }

    // Plusieurs lignes "data:" dans un même événement sont concaténées par un
    // saut de ligne (spec SSE). OpenAI n'en envoie qu'une par événement.
    std::string payload;
    for (std::size_t index = 0; index < data_lines.size(); ++index) {
        if (index > 0) {
            payload += '\n';
        }
        payload += data_lines[index];
    }

    return {std::move(payload)};
}

} // namespace

std::vector<std::string> SseParser::feed(const std::string& chunk)
{
    buffer_ += chunk;

    std::vector<std::string> payloads;
    while (true) {
        const auto end = find_event_end(buffer_);
        if (end == std::string::npos) {
            break;
        }

        const auto event = buffer_.substr(0, end);

        std::size_t after = end + 1;
        if (after < buffer_.size() && buffer_[after] == '\r') {
            ++after;
        }
        ++after; // consomme le second '\n' du séparateur
        buffer_.erase(0, after);

        auto parsed = parse_event(event);
        for (auto& payload : parsed) {
            payloads.push_back(std::move(payload));
        }
    }

    return payloads;
}

std::vector<std::string> SseParser::flush()
{
    if (buffer_.empty()) {
        return {};
    }

    auto parsed = parse_event(buffer_);
    buffer_.clear();
    return parsed;
}
