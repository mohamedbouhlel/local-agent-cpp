#pragma once

#include <stdexcept>
#include <string_view>

namespace agent::ui {

enum class InterfaceMode {
    classic,
    tui,
};

inline InterfaceMode parse_interface_mode(int argc, char* const argv[])
{
    InterfaceMode mode = InterfaceMode::classic;
    bool explicit_mode = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument != "--ui=tui" && argument != "--ui=classic") {
            continue; // Legacy arguments were ignored; preserve that behavior.
        }
        if (explicit_mode) {
            throw std::invalid_argument("specify only one --ui mode");
        }
        explicit_mode = true;
        mode = argument == "--ui=tui" ? InterfaceMode::tui : InterfaceMode::classic;
    }
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument.starts_with("--ui=") && argument != "--ui=tui" && argument != "--ui=classic") {
            throw std::invalid_argument("unsupported UI mode; use --ui=tui or --ui=classic");
        }
    }
    return mode;
}

} // namespace agent::ui
