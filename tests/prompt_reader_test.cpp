// Tests du lecteur de prompt REPL (include/prompt_reader.hpp).
//
// Couvre REPL-01 -> REPL-09 et les arbitrages A-1 a A-9, en pilotant le lecteur
// par des flux injectes (std::istringstream / std::ostringstream) : aucun acces
// a std::cin, aucun reseau, aucun LLM. Les assertions portent sur le STATUT et
// le CONTENU retournes, jamais sur le libelle des invites (non normatif).

#include "prompt_reader.hpp"
#include "test_support.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace {

PromptResult read_one(const std::string& input)
{
    std::istringstream in(input);
    std::ostringstream out;
    PromptReader reader(in, out);
    return reader.read();
}

// Interactions NON terminales produites par un flux (Line/Block/EmptyBlock) :
// les statuts Eof et Aborted terminent la lecture et ne sont pas comptes.
std::vector<PromptResult> read_interactions(const std::string& input)
{
    std::istringstream in(input);
    std::ostringstream out;
    PromptReader reader(in, out);

    std::vector<PromptResult> results;
    while (true) {
        const auto result = reader.read();
        if (result.status == PromptStatus::Eof || result.status == PromptStatus::Aborted) {
            return results;
        }
        results.push_back(result);
    }
}

// Statut terminal atteint par un flux (Eof ou Aborted).
PromptStatus read_terminal_status(const std::string& input)
{
    std::istringstream in(input);
    std::ostringstream out;
    PromptReader reader(in, out);

    while (true) {
        const auto result = reader.read();
        if (result.status == PromptStatus::Eof || result.status == PromptStatus::Aborted) {
            return result.status;
        }
    }
}

std::size_t count_blocks(const std::vector<PromptResult>& results)
{
    std::size_t blocks = 0;
    for (const auto& result : results) {
        if (result.status == PromptStatus::Block) {
            ++blocks;
        }
    }
    return blocks;
}

} // namespace

// --- REPL-01 : declenchement du mode multiline -----------------------------

TEST(repl, multi_exact_declenche_le_mode_multiline)
{
    // ":multi" seul bascule en multiline ; la ligne n'est PAS retournee comme
    // contenu et ne declenche aucun envoi (ici : bloc vide, zero ligne).
    const auto result = read_one(":multi\n:end\n");
    CHECK(result.status == PromptStatus::EmptyBlock);
}

TEST(repl, multi_avec_espaces_n_est_pas_le_declencheur)
{
    CHECK(read_one(" :multi\n").content == std::string(" :multi"));
    CHECK(read_one(":multi \n").content == std::string(":multi "));
    CHECK(read_one(" :multi\n").status == PromptStatus::Line);
}

TEST(repl, multi_dans_un_bloc_est_litteral)
{
    const auto result = read_one(":multi\na\n:multi\nb\n:end\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string("a\n:multi\nb"));
}

TEST(repl, multi_crlf_n_est_pas_normalise_en_mode_simple)
{
    // A-1 : aucune normalisation CRLF en mode simple => la ligne conserve son
    // '\r' et n'est donc PAS le declencheur.
    const auto result = read_one(":multi\r\n");
    CHECK(result.status == PromptStatus::Line);
    CHECK_EQ(result.content, std::string(":multi\r"));
}

// --- REPL-02 : mode simple inchange ---------------------------------------

TEST(repl, ligne_simple_retournee_telle_quelle)
{
    const auto result = read_one("bonjour\n");
    CHECK(result.status == PromptStatus::Line);
    CHECK_EQ(result.content, std::string("bonjour"));
}

TEST(repl, ligne_vide_retournee_vide)
{
    const auto result = read_one("\n");
    CHECK(result.status == PromptStatus::Line);
    CHECK_EQ(result.content, std::string(""));
}

TEST(repl, ligne_d_espaces_non_vide)
{
    const auto result = read_one("   \n");
    CHECK(result.status == PromptStatus::Line);
    CHECK_EQ(result.content, std::string("   "));
}

TEST(repl, ligne_simple_conserve_le_cr_final)
{
    // A-1 : en mode simple, le '\r' eventuel est conserve tel quel.
    const auto result = read_one("a\r\n");
    CHECK(result.status == PromptStatus::Line);
    CHECK_EQ(result.content, std::string("a\r"));
}

// --- REPL-03 : accumulation et conservation exacte ------------------------

TEST(repl, conservation_exacte_des_lignes_vides_et_indentations)
{
    const auto result = read_one(":multi\nl1\n\n\tl2\n:end\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string("l1\n\n\tl2"));
}

TEST(repl, crlf_normalise_dans_le_bloc_multiline)
{
    const auto result = read_one(":multi\na\r\nb\n:end\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string("a\nb"));
}

TEST(repl, crlf_unique_retire_par_ligne_dans_le_bloc)
{
    // A-2 : un seul '\r' final est retire ; un second est conserve.
    const auto result = read_one(":multi\na\r\r\n:end\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string("a\r"));
}

TEST(repl, aucun_saut_de_ligne_final_artificiel)
{
    const auto result = read_one(":multi\na\n:end\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string("a"));
    CHECK_EQ(result.content.size(), std::size_t(1));
}

// --- REPL-04 : terminateur ":end" -----------------------------------------

TEST(repl, end_exact_termine_et_est_exclu_du_contenu)
{
    const auto result = read_one(":multi\nanalyse ceci\n:end\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string("analyse ceci"));
    CHECK(result.content.find(":end") == std::string::npos);
}

TEST(repl, end_indente_n_est_pas_un_terminateur)
{
    // " :end" (indente) est du contenu ; le bloc est termine par un ":end" exact.
    const auto result = read_one(":multi\n :end\n:end\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string(" :end"));
}

TEST(repl, end_en_mode_simple_est_du_contenu)
{
    const auto result = read_one(":end\n");
    CHECK(result.status == PromptStatus::Line);
    CHECK_EQ(result.content, std::string(":end"));
}

TEST(repl, terminateur_crlf_reconnu_apres_normalisation_du_bloc)
{
    // Le terminateur d'un flux CRLF est normalise comme le reste du bloc
    // (REPL-03/A-2) : le bloc ne reste pas suspendu.
    const auto result = read_one(":multi\na\r\n:end\r\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string("a"));
}

// --- REPL-05 : unicite de l'envoi -----------------------------------------

TEST(repl, un_seul_bloc_pour_un_bloc_valide)
{
    const auto results = read_interactions(":multi\na\nb\nc\n:end\n");
    CHECK_EQ(results.size(), std::size_t(1));
    CHECK_EQ(count_blocks(results), std::size_t(1));
    CHECK_EQ(results.front().content, std::string("a\nb\nc"));
}

TEST(repl, bloc_complet_retourne_en_une_seule_lecture)
{
    // Un bloc complet n'est restitue qu'a la terminaison, et en UNE seule
    // lecture : la collecte ne produit aucun resultat partiel intermediaire,
    // puis la lecture suivante est EOF.
    // (Le lecteur n'effectue AUCUN envoi : l'unicite de l'appel a Agent::send
    // est la responsabilite du REPL, qui envoie une fois par resultat.)
    std::istringstream in(":multi\na\nb\n:end\n");
    std::ostringstream out;
    PromptReader reader(in, out);

    const auto first = reader.read();
    CHECK(first.status == PromptStatus::Block);
    CHECK_EQ(first.content, std::string("a\nb"));
    // Une seule interaction disponible : la lecture suivante est EOF.
    CHECK(reader.read().status == PromptStatus::Eof);
}

// --- REPL-06 : blocs vides -------------------------------------------------

TEST(repl, bloc_zero_ligne_n_est_pas_envoye)
{
    const auto results = read_interactions(":multi\n:end\n");
    CHECK_EQ(results.size(), std::size_t(1));
    CHECK(results.front().status == PromptStatus::EmptyBlock);
    CHECK_EQ(count_blocks(results), std::size_t(0));
}

TEST(repl, bloc_de_lignes_vides_est_envoye)
{
    // Deux lignes vides => contenu "\n" : structurellement non vide (A-7).
    const auto result = read_one(":multi\n\n\n:end\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string("\n"));
    CHECK_EQ(result.content.size(), std::size_t(1));
}

TEST(repl, bloc_d_espaces_est_envoye)
{
    const auto result = read_one(":multi\n   \n:end\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string("   "));
}

// --- REPL-07 : exit selon le mode -----------------------------------------

TEST(repl, exit_dans_un_bloc_est_litteral)
{
    const auto result = read_one(":multi\nexit\n:end\n");
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content, std::string("exit"));
}

TEST(repl, exit_simple_est_retourne_comme_ligne_exacte)
{
    const auto result = read_one("exit\n");
    CHECK(result.status == PromptStatus::Line);
    CHECK_EQ(result.content, std::string("exit"));
}

TEST(repl, exit_avec_espaces_est_du_contenu)
{
    // A-6 : correspondance exacte ; "exit " n'est pas la commande.
    const auto result = read_one("exit \n");
    CHECK(result.status == PromptStatus::Line);
    CHECK_EQ(result.content, std::string("exit "));
}

// --- REPL-08 : EOF / Ctrl+D ------------------------------------------------

TEST(repl, eof_en_mode_simple_termine_le_repl)
{
    const auto result = read_one("");
    CHECK(result.status == PromptStatus::Eof);
}

TEST(repl, eof_en_multiline_abandonne_le_bloc)
{
    CHECK(read_one(":multi\na\nb\n").status == PromptStatus::Aborted);
    CHECK(read_terminal_status(":multi\na\nb\n") == PromptStatus::Aborted);
    CHECK_EQ(read_interactions(":multi\na\nb\n").size(), std::size_t(0));
}

TEST(repl, eof_immediat_apres_multi_abandonne)
{
    CHECK(read_one(":multi\n").status == PromptStatus::Aborted);
    CHECK_EQ(read_interactions(":multi\n").size(), std::size_t(0));
}

// --- REPL-09 : retour a l'invite primaire ---------------------------------

TEST(repl, retour_au_mode_simple_apres_terminaison_par_end)
{
    const auto results = read_interactions(":multi\na\n:end\nbonjour\n");
    CHECK_EQ(results.size(), std::size_t(2));
    CHECK(results[0].status == PromptStatus::Block);
    CHECK_EQ(results[0].content, std::string("a"));
    CHECK(results[1].status == PromptStatus::Line);
    CHECK_EQ(results[1].content, std::string("bonjour"));
}

TEST(repl, retour_au_mode_simple_apres_bloc_vide)
{
    const auto results = read_interactions(":multi\n:end\nbonjour\n");
    CHECK_EQ(results.size(), std::size_t(2));
    CHECK(results[0].status == PromptStatus::EmptyBlock);
    CHECK(results[1].status == PromptStatus::Line);
    CHECK_EQ(results[1].content, std::string("bonjour"));
}

TEST(repl, pas_de_retour_au_prompt_apres_abandon_par_eof)
{
    // L'abandon termine la lecture : aucune interaction supplementaire n'est
    // produite apres l'Aborted.
    CHECK(read_one(":multi\na\n").status == PromptStatus::Aborted);
    CHECK(read_terminal_status(":multi\na\n") == PromptStatus::Aborted);
    CHECK_EQ(read_interactions(":multi\na\n").size(), std::size_t(0));
}

// --- A-8 : aucune limite de taille ----------------------------------------

TEST(repl, bloc_volumineux_accepte_sans_limite)
{
    std::string input = ":multi\n";
    const std::string big(100000, 'x');
    input += big + "\n:end\n";

    const auto result = read_one(input);
    CHECK(result.status == PromptStatus::Block);
    CHECK_EQ(result.content.size(), std::size_t(100000));
}
