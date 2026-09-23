#include "prompt_reader.hpp"

#include <utility>
#include <vector>

namespace {

// Sentinelles (byte-exactes en mode simple ; voir REPL-01 et REPL-04).
const std::string trigger_multiline = ":multi";
const std::string terminator = ":end";

// Retire UN SEUL '\r' final, s'il existe : c'est la fin de ligne d'un flux
// CRLF (A-2). Tout autre '\r' est conserve. Applique uniquement aux lignes lues
// en mode multiline (A-1 : le mode simple n'est pas normalise).
void strip_single_trailing_cr(std::string& line)
{
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

} // namespace

PromptReader::PromptReader(std::istream& input, std::ostream& output)
    : input_(input), output_(output)
{
}

PromptResult PromptReader::read()
{
    output_ << "> ";
    output_.flush();

    std::string line;
    if (!std::getline(input_, line)) {
        // Fin de flux en mode simple : terminer le REPL (REPL-08).
        return {PromptStatus::Eof, {}};
    }

    // Mode simple : le declenchement est byte-exact et AUCUNE normalisation
    // n'est appliquee a la ligne retournee (A-1, REPL-01, REPL-02).
    if (line != trigger_multiline) {
        return {PromptStatus::Line, line};
    }

    std::vector<std::string> lines;

    while (true) {
        output_ << "... ";
        output_.flush();

        if (!std::getline(input_, line)) {
            // Fin de flux en mode multiline : le bloc est abandonne, sans
            // envoi, et le REPL se termine — jamais de retour a l'invite
            // primaire (REPL-08).
            return {PromptStatus::Aborted, {}};
        }

        // Fin de ligne CRLF normalisee dans le bloc (A-1, A-2, REPL-03).
        strip_single_trailing_cr(line);

        // Terminateur byte-exact, exclu du contenu (REPL-04).
        if (line == terminator) {
            break;
        }

        lines.push_back(line);
    }

    if (lines.empty()) {
        // Zero ligne accumulee : bloc vide, aucun envoi (REPL-06).
        return {PromptStatus::EmptyBlock, {}};
    }

    // Contenu = lignes jointes par '\n', sans '\n' final artificiel (A-7).
    std::string content;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index != 0) {
            content += '\n';
        }
        content += lines[index];
    }

    return {PromptStatus::Block, std::move(content)};
}
