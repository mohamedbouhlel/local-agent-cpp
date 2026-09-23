#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Framing SSE uniquement (aucune connaissance du schéma OpenAI). Accumule des
// fragments de bytes arbitraires — un chunk réseau peut couper une ligne ou un
// événement en plein milieu, ou en contenir plusieurs — et extrait les
// payloads des événements complets, séparés par une ligne vide. Le marqueur
// "[DONE]" est retourné tel quel : c'est à l'appelant de le reconnaître.
class SseParser {
public:
    // Retourne les payloads "data: ..." des événements complets contenus dans
    // ce chunk (préfixe "data:" retiré), dans l'ordre du flux. Un événement
    // incomplet reste bufferisé pour l'appel suivant.
    std::vector<std::string> feed(const std::string& chunk);

    // À appeler en fin de flux : restitue (puis vide) le dernier événement
    // bufferisé même s'il n'est pas terminé par une ligne vide. Sans cela, un
    // flux coupé juste avant son séparateur final perdrait silencieusement sa
    // dernière trame (dette §B.2 de docs/roadmap/ROADMAP.md).
    std::vector<std::string> flush();

    // Octets encore bufferisés (diagnostic : un flux correctement terminé laisse
    // ce buffer vide).
    std::size_t buffered_bytes() const { return buffer_.size(); }

private:
    std::string buffer_;
};
