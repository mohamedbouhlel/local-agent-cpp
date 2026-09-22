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

private:
    std::string buffer_;
};
