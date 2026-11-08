#pragma once

#include <cstddef>
#include <istream>
#include <ostream>
#include <streambuf>
#include <string>

// Reads one byte at a time from an existing descriptor. This avoids reading
// ahead into the next interactive line when the same terminal is also used by
// a later synchronous prompt (for example write approval).
class FdInputStreamBuffer final : public std::streambuf {
public:
    explicit FdInputStreamBuffer(int fd) noexcept : fd_(fd) {}

protected:
    int_type underflow() override;

private:
    int fd_;
    char current_{};
};

// Lecture d'UNE interaction REPL, en mode simple (une ligne) ou multiline
// (bloc terminé par ":end"). Composant volontairement isolé : il ne connaît ni
// Agent, ni le réseau, ni le LLM ; ses flux d'entrée/sortie sont injectés, ce
// qui le rend testable avec std::istringstream / std::ostringstream.
//
// Le composant ne fait AUCUN envoi : il retourne un résultat ; c'est l'appelant
// (le REPL) qui décide d'appeler Agent::send.
//
// Exigences couvertes : REPL-01, REPL-03, REPL-04, REPL-06, REPL-08, REPL-09
// (voir docs/specifications/repl-multiline-requirements.md).
enum class PromptStatus {
    Line,        // mode simple : la ligne lue, telle quelle (peut être vide)
    Block,       // bloc multiline non vide terminé par ":end"
    EmptyBlock,  // ":end" immédiat : zéro ligne accumulée (aucun envoi)
    Eof,         // fin de flux en mode simple (terminer le REPL)
    Aborted,     // fin de flux en mode multiline : bloc abandonné, terminer le REPL
};

struct PromptResult {
    PromptStatus status = PromptStatus::Line;
    std::string content;  // Line : la ligne ; Block : contenu assemblé par '\n'
};

class PromptReader {
public:
    PromptReader(std::istream& input, std::ostream& output);

    // Lit une interaction. Affiche l'invite primaire ("> ") ou de continuation
    // ("... ") — libellés non normatifs (voir la spécification).
    PromptResult read();

private:
    std::istream& input_;
    std::ostream& output_;
};
