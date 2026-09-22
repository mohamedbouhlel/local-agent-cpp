#include "test_support.hpp"

#include <string>

int main(int argc, char** argv)
{
    std::string filter;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const std::string prefix = "--filter=";
        if (argument.rfind(prefix, 0) == 0) {
            filter = argument.substr(prefix.size());
        }
    }

    long executed = 0;
    for (const auto& test_case : tests::cases()) {
        if (!filter.empty() && test_case.group != filter) {
            continue;
        }

        ++executed;
        std::cout << "[ RUN   ] " << test_case.group << "." << test_case.name << "\n";
        const long failures_before = tests::failures();
        try {
            test_case.fn();
        } catch (const std::exception& error) {
            tests::fail(test_case.name, __FILE__, __LINE__,
                        std::string("exception non geree: ") + error.what());
        } catch (...) {
            tests::fail(test_case.name, __FILE__, __LINE__, "exception non geree (type inconnu)");
        }
        std::cout << (tests::failures() == failures_before ? "[    OK   ] " : "[  ECHEC  ] ")
                  << test_case.group << "." << test_case.name << "\n";
    }

    std::cout << "\n================ RESUME ================\n";
    std::cout << "cas executes        : " << executed << " / " << tests::cases().size() << "\n";
    std::cout << "assertions          : " << tests::checks() << "\n";
    std::cout << "assertions en echec : " << tests::failures() << "\n";

    if (executed == 0) {
        std::cout << "AUCUN CAS EXECUTE (filtre=[" << filter << "])\n";
        return 2;
    }

    std::cout << (tests::failures() == 0 ? "RESULTAT: OK" : "RESULTAT: ECHECS") << "\n";
    return tests::failures() == 0 ? 0 : 1;
}
