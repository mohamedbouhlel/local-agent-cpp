// Tests du framing SSE (include/sse_parser.hpp, src/sse_parser.cpp).
//
// Le parseur ne connaît pas le schéma OpenAI : il extrait les payloads "data:"
// des événements complets et ignore le reste. Les attentes ci-dessous sont
// dérivées du code réel ; lorsqu'un comportement n'est pas défini par le code
// (ex. événement incomplet en fin de flux, absence de flush), le comportement
// observé est consigné et documenté comme tel.

#include "sse_parser.hpp"
#include "test_support.hpp"

#include <string>
#include <vector>

namespace {

std::vector<std::string> feed_all(SseParser& parser, const std::vector<std::string>& chunks)
{
    std::vector<std::string> payloads;
    for (const auto& chunk : chunks) {
        for (auto& payload : parser.feed(chunk)) {
            payloads.push_back(std::move(payload));
        }
    }
    return payloads;
}

// Payloads d'un seul appel feed().
std::vector<std::string> feed_one(const std::string& chunk)
{
    SseParser parser;
    return parser.feed(chunk);
}

} // namespace

TEST(sse, evenement_valide)
{
    const auto payloads = feed_one("data: {\"a\":1}\n\n");
    CHECK_EQ(payloads.size(), std::size_t(1));
    if (!payloads.empty()) {
        CHECK_EQ(payloads[0], std::string("{\"a\":1}"));
    }
}

TEST(sse, evenement_coupe_entre_plusieurs_fragments)
{
    SseParser parser;
    CHECK_EQ(parser.feed("data: {\"a\"").size(), std::size_t(0));
    CHECK_EQ(parser.feed(":1}").size(), std::size_t(0));
    const auto payloads = parser.feed("\n\n");
    CHECK_EQ(payloads.size(), std::size_t(1));
    if (!payloads.empty()) {
        CHECK_EQ(payloads[0], std::string("{\"a\":1}"));
    }
}

TEST(sse, separateur_coupe_entre_deux_fragments)
{
    SseParser parser;
    CHECK_EQ(parser.feed("data: x\n").size(), std::size_t(0)); // un '\n' isole n'est pas un separateur
    const auto payloads = parser.feed("\n");
    CHECK_EQ(payloads.size(), std::size_t(1));
    if (!payloads.empty()) {
        CHECK_EQ(payloads[0], std::string("x"));
    }
}

TEST(sse, plusieurs_evenements_dans_un_meme_chunk)
{
    const auto payloads = feed_one("data: un\n\ndata: deux\n\ndata: trois\n\n");
    CHECK_EQ(payloads.size(), std::size_t(3));
    if (payloads.size() == 3) {
        CHECK_EQ(payloads[0], std::string("un"));
        CHECK_EQ(payloads[1], std::string("deux"));
        CHECK_EQ(payloads[2], std::string("trois"));
    }
}

TEST(sse, evenement_incomplet_en_fin_de_flux)
{
    // Comportement observé : le parseur n'expose pas de flush(), donc un
    // événement non terminé par une ligne vide reste bufferisé et est perdu.
    // Consigné comme comportement réel, pas comme comportement souhaitable.
    const auto payloads = feed_one("data: jamais_termine");
    CHECK_EQ(payloads.size(), std::size_t(0));
}

TEST(sse, ligne_vide_et_evenement_vide)
{
    CHECK_EQ(feed_one("\n\n").size(), std::size_t(0));
    CHECK_EQ(feed_one("\n").size(), std::size_t(0));
    CHECK_EQ(feed_one("").size(), std::size_t(0));
}

TEST(sse, donnees_malformees_retournees_telles_quelles)
{
    // Le parseur SSE ne valide pas le JSON : la charge malformée est retournée
    // brute, c'est l'appelant (agent.cpp) qui l'ignore.
    const auto payloads = feed_one("data: {pas du json\n\n");
    CHECK_EQ(payloads.size(), std::size_t(1));
    if (!payloads.empty()) {
        CHECK_EQ(payloads[0], std::string("{pas du json"));
    }
}

TEST(sse, marqueur_done_retourne_tel_quel)
{
    const auto payloads = feed_one("data: [DONE]\n\n");
    CHECK_EQ(payloads.size(), std::size_t(1));
    if (!payloads.empty()) {
        CHECK_EQ(payloads[0], std::string("[DONE]"));
    }
}

TEST(sse, separateur_crlf)
{
    const auto payloads = feed_one("data: valeur\r\n\r\n");
    CHECK_EQ(payloads.size(), std::size_t(1));
    if (!payloads.empty()) {
        CHECK_EQ(payloads[0], std::string("valeur"));
    }
}

TEST(sse, plusieurs_lignes_data_concatenees)
{
    const auto payloads = feed_one("data: ligne1\ndata: ligne2\n\n");
    CHECK_EQ(payloads.size(), std::size_t(1));
    if (!payloads.empty()) {
        CHECK_EQ(payloads[0], std::string("ligne1\nligne2"));
    }
}

TEST(sse, commentaires_ignores)
{
    const auto payloads = feed_one(": keep-alive\n\ndata: utile\n\n: autre commentaire\n\n");
    CHECK_EQ(payloads.size(), std::size_t(1));
    if (!payloads.empty()) {
        CHECK_EQ(payloads[0], std::string("utile"));
    }
}

TEST(sse, commentaire_dans_le_meme_evenement)
{
    const auto payloads = feed_one(": ping\ndata: x\n\n");
    CHECK_EQ(payloads.size(), std::size_t(1));
    if (!payloads.empty()) {
        CHECK_EQ(payloads[0], std::string("x"));
    }
}

TEST(sse, evenement_sans_ligne_data_ignore)
{
    CHECK_EQ(feed_one("event: message\nid: 42\nretry: 1000\n\n").size(), std::size_t(0));
}

TEST(sse, data_sans_espace_apres_deux_points)
{
    const auto payloads = feed_one("data:{\"a\":1}\n\n");
    CHECK_EQ(payloads.size(), std::size_t(1));
    if (!payloads.empty()) {
        CHECK_EQ(payloads[0], std::string("{\"a\":1}"));
    }
}

TEST(sse, flux_fragmentaire_octet_par_octet)
{
    const std::string stream = "data: abc\n\ndata: def\n\n";
    SseParser parser;
    std::vector<std::string> payloads;
    for (const char character : stream) {
        for (auto& payload : parser.feed(std::string(1, character))) {
            payloads.push_back(std::move(payload));
        }
    }
    CHECK_EQ(payloads.size(), std::size_t(2));
    if (payloads.size() == 2) {
        CHECK_EQ(payloads[0], std::string("abc"));
        CHECK_EQ(payloads[1], std::string("def"));
    }
}

TEST(sse, feed_all_equivalence)
{
    SseParser parser;
    const auto payloads = feed_all(parser, {"data: 1\n\ndata: ", "2\n\n"});
    CHECK_EQ(payloads.size(), std::size_t(2));
    if (payloads.size() == 2) {
        CHECK_EQ(payloads[0], std::string("1"));
        CHECK_EQ(payloads[1], std::string("2"));
    }
}
