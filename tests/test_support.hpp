#pragma once

// Micro-harnais de tests sans dépendance externe (contrainte AGENTS.md:
// "Ne pas ajouter de framework externe sans le demander explicitement").
//
// Usage :
//   TEST(groupe, nom) { CHECK(condition); CHECK_EQ(a, b); }
//
// Le binaire accepte --filter=<groupe> pour n'exécuter qu'un groupe, et
// retourne 0 si aucun test n'a échoué.

#include <unistd.h>

#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace tests {

struct Case {
    std::string group;
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& cases()
{
    static std::vector<Case> registry;
    return registry;
}

inline long& failures()
{
    static long count = 0;
    return count;
}

inline long& checks()
{
    static long count = 0;
    return count;
}

class Registrar {
public:
    Registrar(std::string group, std::string name, std::function<void()> fn)
    {
        cases().push_back(Case{std::move(group), std::move(name), std::move(fn)});
    }
};

inline void fail(const std::string& expression, const char* file, int line, const std::string& detail)
{
    ++failures();
    std::cout << "    FAIL " << file << ":" << line << "  " << expression;
    if (!detail.empty()) {
        std::cout << "  [" << detail << "]";
    }
    std::cout << "\n";
}

// Retourne le message de l'exception levée, ou une chaîne vide si aucune
// exception n'est levée. (Limite connue : une exception dont what() est vide
// serait indistinguable de l'absence d'exception.)
template <typename Function>
std::string capture_error(Function&& function)
{
    try {
        function();
    } catch (const std::exception& error) {
        return std::string(error.what());
    }
    return {};
}

// Répertoire temporaire dédié à un test, supprimé automatiquement (RAII).
// Aucun test ne doit écrire dans le workspace réel du dépôt.
//
// Le nom inclut le PID du processus : deux exécutions simultanées de la même
// suite (deux builds, deux checkouts) ne partagent plus le même répertoire et
// ne peuvent donc plus se détruire mutuellement leurs fichiers (constat E2 du
// rapport docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md).
class TempDir {
public:
    explicit TempDir(const std::string& tag)
    {
        path_ = std::filesystem::temp_directory_path()
            / ("agent_cpp_test_" + std::to_string(::getpid()) + "_" + tag);
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_, error);
        path_ = std::filesystem::canonical(path_);
    }

    ~TempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace tests

#define TEST(group, name)                                                          \
    static void test_##group##_##name();                                           \
    [[maybe_unused]] static const tests::Registrar registrar_##group##_##name(     \
        #group, #name, test_##group##_##name);                                     \
    static void test_##group##_##name()

#define CHECK(condition)                                                           \
    do {                                                                           \
        ++tests::checks();                                                         \
        if (!(condition)) {                                                        \
            tests::fail(#condition, __FILE__, __LINE__, "");                       \
        }                                                                          \
    } while (false)

#define CHECK_EQ(actual, expected)                                                 \
    do {                                                                           \
        ++tests::checks();                                                         \
        const auto actual_value_ = (actual);                                       \
        const auto expected_value_ = (expected);                                   \
        if (!(actual_value_ == expected_value_)) {                                 \
            std::ostringstream detail_;                                            \
            detail_ << "obtenu=[" << actual_value_ << "] attendu=["               \
                    << expected_value_ << "]";                                     \
            tests::fail(#actual " == " #expected, __FILE__, __LINE__, detail_.str()); \
        }                                                                          \
    } while (false)

#define CHECK_THROWS_MSG(expression, substring)                                    \
    do {                                                                           \
        ++tests::checks();                                                         \
        std::string message_ = tests::capture_error([&] { (void)(expression); });  \
        if (message_.empty()) {                                                    \
            tests::fail(#expression " doit lever", __FILE__, __LINE__,             \
                        "aucune exception levee");                                 \
        } else if (message_.find(substring) == std::string::npos) {                \
            tests::fail(#expression " message", __FILE__, __LINE__,                \
                        "obtenu=[" + message_ + "] attendu contenant [" +          \
                            std::string(substring) + "]");                         \
        }                                                                          \
    } while (false)

#define CHECK_NO_THROW(expression)                                                 \
    do {                                                                           \
        ++tests::checks();                                                         \
        std::string message_ = tests::capture_error([&] { (void)(expression); });  \
        if (!message_.empty()) {                                                   \
            tests::fail(#expression " ne doit pas lever", __FILE__, __LINE__,      \
                        "exception inattendue: " + message_);                      \
        }                                                                          \
    } while (false)
