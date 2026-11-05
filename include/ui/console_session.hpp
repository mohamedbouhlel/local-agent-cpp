#pragma once

#include "agent.hpp"
#include "ui/presentation_controller.hpp"

#include <string>
#include <string_view>

namespace agent::ui {

enum class ConsoleTurnResult {
    completed,
    failed,
};

// Shared by the CLI and integration tests so exceptions are presented once,
// after Agent has emitted its terminal events.
ConsoleTurnResult run_console_turn(Agent& agent,
                                  const std::string& input,
                                  std::string_view base_url,
                                  PresentationController& presentation);

} // namespace agent::ui
