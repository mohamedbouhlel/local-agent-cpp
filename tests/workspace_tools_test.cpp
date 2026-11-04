// Tests des outils workspace et du registre d'outils.
//
// Toutes les opérations fichiers se font dans un répertoire temporaire dédié
// (tests::TempDir) : le workspace réel du dépôt n'est jamais utilisé.
//
// Les cas marqués « DEFECT DOCUMENTE » encodent le comportement RÉELLEMENT
// observé du code actuel, y compris lorsqu'il est jugé discutable. Ils ne
// doivent pas être lus comme une validation du comportement souhaité.

#include "test_support.hpp"
#include "tool_registry.hpp"
#include "workspace_tools.hpp"

#include <unistd.h>

#include <cstddef>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

ToolRegistry make_registry(const std::filesystem::path& root)
{
    ToolRegistry registry;
    register_workspace_tools(registry, root);
    return registry;
}

std::string call(ToolRegistry& registry, const std::string& tool, const std::string& arguments)
{
    return registry.execute(tool, arguments);
}

void write_text(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::string read_text(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::size_t count_lines(const std::string& text)
{
    std::size_t lines = 0;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            ++lines;
        }
    }
    return lines;
}

std::string find_temp_residue(const std::filesystem::path& root)
{
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.path().filename().string().find(".tmp.") != std::string::npos) {
            return entry.path().string();
        }
    }
    return {};
}

bool running_as_root()
{
    return ::geteuid() == 0;
}

// Restaure des permissions autorisant la suppression du répertoire temporaire.
class PermissionRestorer {
public:
    explicit PermissionRestorer(std::filesystem::path path)
        : path_(std::move(path))
    {
    }

    ~PermissionRestorer()
    {
        std::error_code error;
        std::filesystem::permissions(path_,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace,
                                     error);
    }

private:
    std::filesystem::path path_;
};

} // namespace

// ---------------------------------------------------------------------------
// Registre d'outils
// ---------------------------------------------------------------------------

TEST(workspace, registre_outil_inconnu)
{
    tests::TempDir workspace("registry_unknown");
    auto registry = make_registry(workspace.path());
    CHECK_THROWS_MSG(call(registry, "nope", "{}"), "unknown tool: nope");
}

TEST(workspace, registre_quatre_outils_et_schemas)
{
    tests::TempDir workspace("registry_schemas");
    auto registry = make_registry(workspace.path());

    CHECK(!registry.empty());
    const auto schemas = registry.schemas_json();
    CHECK_EQ(schemas.size(), std::size_t(4));

    std::vector<std::string> names;
    for (const auto& schema : schemas) {
        names.push_back(schema.at("function").at("name").get<std::string>());
        CHECK_EQ(schema.at("type").get<std::string>(), std::string("function"));
    }
    // std::map => ordre alphabetique deterministe.
    const std::vector<std::string> expected = {"list_files", "read_file", "search_files", "write_file"};
    CHECK(names == expected);
}

TEST(workspace, registre_double_enregistrement_rejete)
{
    tests::TempDir workspace("registry_duplicate");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path());
    CHECK_THROWS_MSG(register_workspace_tools(registry, workspace.path()), "tool already registered");
}

// ---------------------------------------------------------------------------
// Lectures / écritures nominales
// ---------------------------------------------------------------------------

TEST(workspace, ecriture_puis_lecture)
{
    tests::TempDir workspace("write_read");
    auto registry = make_registry(workspace.path());

    CHECK_EQ(call(registry, "write_file", R"({"path":"a.txt","content":"bonjour"})"),
             std::string("wrote 7 bytes to a.txt (created new file)"));
    CHECK_EQ(call(registry, "read_file", R"({"path":"a.txt"})"), std::string("bonjour"));
}

TEST(workspace, ecriture_verifiee_sur_disque)
{
    // Correctif R0 : write_file relit le fichier écrit avant de publier le
    // succès. Le test contrôle l'état final sur le disque, indépendamment du
    // message retourné.
    tests::TempDir workspace("write_verified");
    auto registry = make_registry(workspace.path());

    const std::string content = "ligne1\nligne2\nfin";
    call(registry, "write_file", R"({"path":"verif.txt","content":"ligne1\nligne2\nfin"})");

    std::ifstream file(workspace.path() / "verif.txt", std::ios::binary);
    CHECK(file.good());
    const std::string on_disk((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    CHECK_EQ(on_disk, content);
}

TEST(workspace, creation_repertoires_parents)
{
    tests::TempDir workspace("parents");
    auto registry = make_registry(workspace.path());

    CHECK_EQ(call(registry, "write_file", R"({"path":"a/b/c.txt","content":"x"})"),
             std::string("wrote 1 bytes to a/b/c.txt (created new file)"));
    CHECK(std::filesystem::exists(workspace.path() / "a" / "b" / "c.txt"));
    CHECK_EQ(call(registry, "read_file", R"({"path":"a/b/c.txt"})"), std::string("x"));
}

TEST(workspace, garde_fou_overwrite)
{
    tests::TempDir workspace("overwrite");
    auto registry = make_registry(workspace.path());

    call(registry, "write_file", R"({"path":"a.txt","content":"v1"})");

    CHECK_THROWS_MSG(call(registry, "write_file", R"({"path":"a.txt","content":"v2"})"),
                     "file already exists: a.txt (pass overwrite: true to replace it)");
    CHECK_EQ(call(registry, "read_file", R"({"path":"a.txt"})"), std::string("v1"));

    CHECK_EQ(call(registry, "write_file", R"({"path":"a.txt","content":"v2","overwrite":true})"),
             std::string("wrote 2 bytes to a.txt (overwrote existing file)"));
    CHECK_EQ(call(registry, "read_file", R"({"path":"a.txt"})"), std::string("v2"));
}

TEST(workspace, ecriture_atomique_sans_residu)
{
    tests::TempDir workspace("atomic");
    auto registry = make_registry(workspace.path());

    call(registry, "write_file", R"({"path":"a.txt","content":"v1"})");
    CHECK_EQ(find_temp_residue(workspace.path()), std::string(""));

    call(registry, "write_file", R"({"path":"a.txt","content":"v2","overwrite":true})");
    CHECK_EQ(find_temp_residue(workspace.path()), std::string(""));
}

TEST(workspace, permissions_preservees_a_lecrasement)
{
    tests::TempDir workspace("perms");
    auto registry = make_registry(workspace.path());

    call(registry, "write_file", R"({"path":"a.txt","content":"v1"})");
    const auto target = workspace.path() / "a.txt";
    std::filesystem::permissions(target,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace);
    const auto before = std::filesystem::status(target).permissions();

    call(registry, "write_file", R"({"path":"a.txt","content":"v2","overwrite":true})");

    CHECK(std::filesystem::status(target).permissions() == before);
    CHECK_EQ(read_text(target), std::string("v2"));
}

// ---------------------------------------------------------------------------
// Chemins et sandbox
// ---------------------------------------------------------------------------

TEST(workspace, chemin_inexistant)
{
    tests::TempDir workspace("missing");
    auto registry = make_registry(workspace.path());
    CHECK_THROWS_MSG(call(registry, "read_file", R"({"path":"absent.txt"})"), "file does not exist");
}

TEST(workspace, chemin_vide)
{
    tests::TempDir workspace("empty_path");
    auto registry = make_registry(workspace.path());
    CHECK_THROWS_MSG(call(registry, "read_file", R"({"path":""})"), "workspace path must not be empty");
    CHECK_THROWS_MSG(call(registry, "write_file", R"({"path":"","content":"x"})"),
                     "workspace path must not be empty");
}

TEST(workspace, chemin_absolu)
{
    tests::TempDir workspace("absolute");
    auto registry = make_registry(workspace.path());

    // Cibles absolues prises DANS l'espace temporaire propre au test (constat E3
    // du rapport V) : le rejet doit porter sur le caractere ABSOLU du chemin, et
    // non sur un chemin partage entre executions (ex. /tmp/...).
    const auto read_target = (workspace.path() / "lecture_absolue.txt").string();
    const auto write_target = (workspace.path() / "ecriture_absolue.txt").string();

    CHECK_THROWS_MSG(call(registry, "read_file", std::string(R"({"path":")") + read_target + R"("})"),
                     "absolute workspace paths are not allowed");
    CHECK_THROWS_MSG(call(registry, "write_file",
                          std::string(R"({"path":")") + write_target + R"(","content":"x"})"),
                     "absolute workspace paths are not allowed");
    CHECK(!std::filesystem::exists(write_target));
}

TEST(workspace, chemin_avec_double_point)
{
    tests::TempDir workspace("dotdot");
    auto registry = make_registry(workspace.path());
    CHECK_THROWS_MSG(call(registry, "read_file", R"({"path":"../x.txt"})"),
                     "workspace path must not contain '..'");
    CHECK_THROWS_MSG(call(registry, "write_file", R"({"path":"sub/../../x.txt","content":"x"})"),
                     "workspace path must not contain '..'");
}

TEST(workspace, chemin_point_seul_sort_du_sandbox)
{
    tests::TempDir workspace("dot");
    auto registry = make_registry(workspace.path());
    CHECK_THROWS_MSG(call(registry, "write_file", R"({"path":".","content":"x"})"),
                     "workspace path escapes sandbox");
}

TEST(workspace, sortie_par_lien_symbolique_rejetee)
{
    tests::TempDir workspace("symlink_in");
    tests::TempDir outside("symlink_out");
    write_text(outside.path() / "secret.txt", "donnee hors sandbox");

    std::error_code error;
    std::filesystem::create_symlink(outside.path(), workspace.path() / "lien", error);
    if (error) {
        std::cout << "    (SKIP: creation de lien symbolique impossible: " << error.message() << ")\n";
        return;
    }

    auto registry = make_registry(workspace.path());
    CHECK_THROWS_MSG(call(registry, "read_file", R"({"path":"lien/secret.txt"})"),
                     "workspace path escapes sandbox");
    CHECK_THROWS_MSG(call(registry, "write_file", R"({"path":"lien/ecrit.txt","content":"x"})"),
                     "workspace path escapes sandbox");
    CHECK(!std::filesystem::exists(outside.path() / "ecrit.txt"));
}

TEST(workspace, recherche_et_liste_excluent_lien_vers_fichier_exterieur)
{
    tests::TempDir workspace("symlink_search_in");
    tests::TempDir outside("symlink_search_out");
    write_text(outside.path() / "secret.txt", "marqueur_exterieur_unique\n");
    write_text(workspace.path() / "normal.txt", "marqueur_interieur_unique\n");

    std::error_code error;
    std::filesystem::create_symlink(outside.path() / "secret.txt", workspace.path() / "lien.txt", error);
    if (error) {
        std::cout << "    (SKIP: creation de lien symbolique impossible: " << error.message() << ")\n";
        return;
    }

    auto registry = make_registry(workspace.path());
    CHECK(call(registry, "search_files", R"({"query":"marqueur_exterieur_unique"})").empty());
    CHECK(call(registry, "search_files", R"({"query":"marqueur_interieur_unique"})").find("normal.txt") != std::string::npos);
    CHECK(call(registry, "list_files", "{}").find("lien.txt") == std::string::npos);
    CHECK_THROWS_MSG(call(registry, "read_file", R"({"path":"../../etc/passwd"})"), "must not contain");
}

TEST(workspace, recherche_ignore_lien_remplace_en_concurrence)
{
    tests::TempDir workspace("race_search_in");
    tests::TempDir outside("race_search_out");
    write_text(workspace.path() / "normal.txt", "interieur\n");
    write_text(outside.path() / "secret.txt", "secret_concurrent_unique\n");
    std::error_code error;
    std::filesystem::create_symlink(workspace.path() / "normal.txt", workspace.path() / "lien", error);
    if (error) throw std::runtime_error("test requires symbolic links: " + error.message());
    auto registry = make_registry(workspace.path());
    std::atomic<bool> stop{false};
    std::thread writer([&] {
        unsigned iteration = 0;
        while (!stop.load()) {
            std::error_code ec;
            const auto temporary = workspace.path() / ".swap";
            const auto target = (++iteration % 2) ? outside.path() / "secret.txt" : workspace.path() / "normal.txt";
            std::filesystem::create_symlink(target, temporary, ec);
            if (!ec) std::filesystem::rename(temporary, workspace.path() / "lien", ec);
        }
    });
    bool leaked = false;
    std::string failure;
    try {
        for (int i = 0; i < 2000 && !leaked; ++i) {
            leaked = call(registry, "search_files", R"({"query":"secret_concurrent_unique"})")
                         .find("secret_concurrent_unique") != std::string::npos;
        }
    } catch (const std::exception& exception) { failure = exception.what(); }
    stop.store(true);
    writer.join();
    CHECK(failure.empty());
    CHECK(!leaked);
}

TEST(workspace, arguments_manquants_ou_mal_typoes)
{
    tests::TempDir workspace("args");
    auto registry = make_registry(workspace.path());

    CHECK_THROWS_MSG(call(registry, "read_file", "{}"), "missing required string argument: path");
    CHECK_THROWS_MSG(call(registry, "write_file", R"({"path":"x.txt"})"),
                     "missing required string argument: content");
    CHECK_THROWS_MSG(call(registry, "write_file", R"({"path":"x.txt","content":"y","overwrite":"yes"})"),
                     "argument must be a boolean: overwrite");
    CHECK_THROWS_MSG(call(registry, "search_files", R"({"query":""})"), "query must not be empty");
}

TEST(workspace, cible_est_un_repertoire)
{
    tests::TempDir workspace("dir_target");
    auto registry = make_registry(workspace.path());
    std::filesystem::create_directories(workspace.path() / "d");
    CHECK_THROWS_MSG(call(registry, "write_file", R"({"path":"d","content":"x"})"),
                     "cannot write file: path is an existing directory: d");
}

// ---------------------------------------------------------------------------
// Limites de taille
// ---------------------------------------------------------------------------

TEST(workspace, limite_ecriture_200000)
{
    tests::TempDir workspace("write_limit");
    auto registry = make_registry(workspace.path());

    const std::string at_limit(200000, 'y');
    const auto accepted = call(registry, "write_file",
                               std::string(R"({"path":"exact.txt","content":")") + at_limit + R"("})");
    CHECK_EQ(accepted, std::string("wrote 200000 bytes to exact.txt (created new file)"));
    CHECK(std::filesystem::exists(workspace.path() / "exact.txt"));

    const std::string over_limit(200001, 'x');
    CHECK_THROWS_MSG(call(registry, "write_file",
                          std::string(R"({"path":"trop.txt","content":")") + over_limit + R"("})"),
                     "exceeding the maximum write size of 200000");
    CHECK(!std::filesystem::exists(workspace.path() / "trop.txt"));
}

TEST(workspace, limite_lecture_50000_et_marqueur)
{
    tests::TempDir workspace("read_limit");
    auto registry = make_registry(workspace.path());

    const std::string big(60000, 'z');
    write_text(workspace.path() / "big.txt", big);

    const auto content = call(registry, "read_file", R"({"path":"big.txt"})");
    CHECK(content.find("[truncated: file exceeds 50000 characters]") != std::string::npos);
    CHECK_EQ(content.substr(0, 50000), std::string(50000, 'z'));
    CHECK(content.size() < big.size());

    // Fichier sous la limite : aucun marqueur.
    write_text(workspace.path() / "small.txt", std::string(10, 'q'));
    const auto small = call(registry, "read_file", R"({"path":"small.txt"})");
    CHECK_EQ(small, std::string(10, 'q'));
    CHECK(small.find("[truncated") == std::string::npos);
}

TEST(workspace, fichier_binaire_rejete)
{
    tests::TempDir workspace("binary");
    auto registry = make_registry(workspace.path());

    std::string binary = "texte";
    binary.push_back('\0');
    binary += "suite";
    write_text(workspace.path() / "b.bin", binary);

    CHECK_THROWS_MSG(call(registry, "read_file", R"({"path":"b.bin"})"),
                     "cannot read binary file: b.bin");
}

// ---------------------------------------------------------------------------
// list_files / search_files
// ---------------------------------------------------------------------------

TEST(workspace, list_files_exclut_les_chemins_caches)
{
    tests::TempDir workspace("list");
    auto registry = make_registry(workspace.path());

    write_text(workspace.path() / "a.txt", "a");
    std::filesystem::create_directories(workspace.path() / "sub");
    write_text(workspace.path() / "sub" / "b.txt", "b");
    std::filesystem::create_directories(workspace.path() / ".cache");
    write_text(workspace.path() / ".cache" / "c.txt", "c");
    write_text(workspace.path() / ".hidden", "h");

    const auto listing = call(registry, "list_files", "{}");
    CHECK(listing.find("a.txt") != std::string::npos);
    CHECK(listing.find("sub/b.txt") != std::string::npos);
    CHECK(listing.find(".cache") == std::string::npos);
    CHECK(listing.find(".hidden") == std::string::npos);
}

TEST(workspace, search_files_litteral_et_sensible_a_la_casse)
{
    tests::TempDir workspace("search_case");
    auto registry = make_registry(workspace.path());

    write_text(workspace.path() / "a.txt", "Hello world\nhello world\n");

    const auto upper = call(registry, "search_files", R"({"query":"Hello"})");
    CHECK_EQ(upper, std::string("a.txt:1:Hello world\n"));

    const auto lower = call(registry, "search_files", R"({"query":"hello"})");
    CHECK_EQ(lower, std::string("a.txt:2:hello world\n"));
}

TEST(workspace, search_files_plafond_50_resultats)
{
    tests::TempDir workspace("search_cap");
    auto registry = make_registry(workspace.path());

    std::string content;
    for (int line = 0; line < 60; ++line) {
        content += "needle " + std::to_string(line) + "\n";
    }
    write_text(workspace.path() / "a.txt", content);

    const auto results = call(registry, "search_files", R"({"query":"needle"})");
    CHECK(results.find("[truncated: reached 50 search results]") != std::string::npos);
    // 50 lignes de resultat + la ligne de marqueur.
    CHECK_EQ(count_lines(results), std::size_t(51));
}

TEST(workspace, search_files_ignore_les_binaires_et_les_caches)
{
    tests::TempDir workspace("search_skip");
    auto registry = make_registry(workspace.path());

    std::string binary = "needle";
    binary.push_back('\0');
    write_text(workspace.path() / "b.bin", binary);
    std::filesystem::create_directories(workspace.path() / ".cache");
    write_text(workspace.path() / ".cache" / "c.txt", "needle\n");

    CHECK_EQ(call(registry, "search_files", R"({"query":"needle"})"), std::string(""));
}

// ---------------------------------------------------------------------------
// Erreurs d'accès (reproductibles en tant qu'utilisateur non privilégié)
// ---------------------------------------------------------------------------

TEST(workspace, ecriture_dans_repertoire_non_inscriptible)
{
    if (running_as_root()) {
        std::cout << "    (SKIP: execution en root, permissions non applicables)\n";
        return;
    }

    tests::TempDir workspace("readonly_dir");
    auto registry = make_registry(workspace.path());

    const auto locked = workspace.path() / "verrou";
    std::filesystem::create_directories(locked);
    const PermissionRestorer restorer(locked);
    std::filesystem::permissions(locked,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::replace);

    CHECK_THROWS_MSG(call(registry, "write_file", R"({"path":"verrou/x.txt","content":"x"})"),
                     "failed to open file for writing");
    CHECK_EQ(find_temp_residue(locked), std::string(""));
}

TEST(workspace, fichier_texte_illisible_signale_comme_binaire_DEFECT_DOCUMENTE)
{
    // DEFECT DOCUMENTE (constat R10 de l'audit PHASE 1) : appears_binary()
    // retourne true quand le fichier ne peut pas etre ouvert, donc un fichier
    // TEXTE simplement illisible est signale "cannot read binary file".
    // Ce test encode le comportement observe, pas le comportement souhaite.
    if (running_as_root()) {
        std::cout << "    (SKIP: execution en root, permissions non applicables)\n";
        return;
    }

    tests::TempDir workspace("unreadable");
    auto registry = make_registry(workspace.path());

    const auto target = workspace.path() / "texte.txt";
    write_text(target, "contenu lisible");
    const PermissionRestorer restorer(target);
    std::filesystem::permissions(target, std::filesystem::perms::none,
                                 std::filesystem::perm_options::replace);

    CHECK_THROWS_MSG(call(registry, "read_file", R"({"path":"texte.txt"})"),
                     "cannot read binary file: texte.txt");
}
