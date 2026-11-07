#include "ui/tui_view_data.hpp"

#include "agent.hpp"
#include "tool_registry.hpp"

#include <nlohmann/json.hpp>

namespace agent::ui {
namespace {

std::string safe_endpoint_origin(const std::string& endpoint)
{
    const auto scheme_end = endpoint.find("://");
    if (scheme_end == std::string::npos || scheme_end == 0) return "configured (details hidden)";
    const auto authority_begin = scheme_end + 3;
    const auto authority_end = endpoint.find_first_of("/?#", authority_begin);
    const auto raw_authority = endpoint.substr(
        authority_begin,
        authority_end == std::string::npos ? std::string::npos : authority_end - authority_begin);
    const auto at = raw_authority.rfind('@');
    const auto host = at == std::string::npos ? raw_authority : raw_authority.substr(at + 1);
    if (host.empty() || host.find_first_of(" \t\r\n") != std::string::npos) {
        return "configured (details hidden)";
    }
    return endpoint.substr(0, scheme_end + 3) + host;
}

} // namespace

TuiViewData make_tui_view_data(const AgentConfig& config, const ToolRegistry& tools)
{
    TuiViewData result;
    result.model = config.model;
    result.endpoint_origin = safe_endpoint_origin(config.base_url);
    result.timeout_seconds = config.timeout_seconds;
    result.api_key_configured = !config.api_key.empty();

    const auto schemas = tools.schemas_json();
    if (!schemas.is_array()) return result;
    for (const auto& entry : schemas) {
        const auto function = entry.value("function", nlohmann::json::object());
        if (!function.is_object()) continue;
        result.tools.push_back({function.value("name", std::string{}),
                                function.value("description", std::string{})});
    }
    return result;
}

} // namespace agent::ui
