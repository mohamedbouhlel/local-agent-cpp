#pragma once

#include <string>
#include <vector>

struct AgentConfig;
class ToolRegistry;

namespace agent::ui {

struct TuiToolSummary {
    std::string name;
    std::string description;
};

struct TuiViewData {
    std::string model;
    std::string endpoint_origin;
    long timeout_seconds = 0;
    long connect_timeout_seconds = 0;
    long idle_timeout_seconds = 0;
    long total_timeout_seconds = 0;
    bool api_key_configured = false;
    std::vector<TuiToolSummary> tools;
};

TuiViewData make_tui_view_data(const AgentConfig& config, const ToolRegistry& tools);

} // namespace agent::ui
