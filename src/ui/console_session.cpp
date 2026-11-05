#include "ui/console_session.hpp"

#include "http_client.hpp"

namespace agent::ui {

ConsoleTurnResult run_console_turn(Agent& agent,
                                  const std::string& input,
                                  std::string_view base_url,
                                  PresentationController& presentation)
{
    presentation.begin_turn();
    try {
        const auto response = agent.send(input);
        // Normally TextDelta is the sole content-rendering path. If the
        // observer is disabled before any delta could be rendered, use the
        // completed response once as a bounded fallback.
        if (presentation.observer_failed()) {
            if (presentation.turn_had_text()) {
                presentation.report_observer_failure_after_text();
            } else {
                presentation.render_fallback_response(response);
            }
        }
        return ConsoleTurnResult::completed;
    } catch (const HttpError& error) {
        presentation.report_failure("Impossible de contacter LLM_BASE_URL (" + std::string(base_url)
                                    + "): " + error.what());
    } catch (const AgentError& error) {
        presentation.report_failure("Reponse incomplete ou invalide du modele: " + std::string(error.what()));
    } catch (const std::exception& error) {
        presentation.report_failure("Reponse du serveur inattendue: " + std::string(error.what()));
    } catch (...) {
        presentation.report_failure("Erreur inattendue pendant le tour.");
    }
    return ConsoleTurnResult::failed;
}

} // namespace agent::ui
