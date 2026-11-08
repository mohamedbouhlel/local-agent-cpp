#include "tool_registry.hpp"
#include "filesystem_sandbox.hpp"
#include "ui/tui_application.hpp"
#include "ui/tui_approval_broker.hpp"
#include "workspace_tools.hpp"

#include <cstdlib>
#include <chrono>
#include <iostream>
#include <memory>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace {
class ExpiringWriteInvocation final : public ToolInvocation {
public:
    ExpiringWriteInvocation(std::shared_ptr<agent::ui::TuiApprovalBroker> approvals,
                            std::shared_ptr<WriteApprovalRequest> request)
        : approvals_(std::move(approvals)), request_(std::move(request)) {}

    std::shared_ptr<const WriteApprovalRequest> approval_request() const override { return request_; }
    ApprovalDecision authorize() override { return approvals_->request(request_); }
    std::string execute() override
    {
        const auto target = request_->workspace_root / request_->relative_path;
        std::filesystem::create_directories(target.parent_path());
        std::ofstream out(target, std::ios::binary);
        out << request_->content;
        if (!out) throw std::runtime_error("test-only expiry writer failed");
        return "test-only write executed";
    }

private:
    std::shared_ptr<agent::ui::TuiApprovalBroker> approvals_;
    std::shared_ptr<WriteApprovalRequest> request_;
};

void register_short_expiry_test_tool(ToolRegistry& tools,
                                     const std::filesystem::path& workspace,
                                     const std::shared_ptr<agent::ui::TuiApprovalBroker>& approvals)
{
    const auto prepare = [workspace, approvals](nlohmann::json args, const ToolInvocationContext& context) {
        auto request = std::make_shared<WriteApprovalRequest>();
        request->approval_id = 1;
        request->invocation = context;
        request->workspace_root = workspace;
        request->relative_path = args.value("path", "nested/expired.txt");
        request->content = args.value("content", "expiry test content");
        request->content_bytes = request->content.size();
        request->missing_parent_directories = {request->relative_path.parent_path()};
        request->expires_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(450);
        return std::make_unique<ExpiringWriteInvocation>(approvals, std::move(request));
    };
    tools.register_tool(Tool{"write_file", "test-only short-expiry approval probe",
        {{"type", "object"}, {"properties", {{"path", {{"type", "string"}}},
                                                 {"content", {{"type", "string"}}}}},
         {"required", {"path", "content"}}},
        [](nlohmann::json) { return std::string("unexpected direct handler"); }, prepare, true});
}
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3) return 2;
    AgentConfig config{argv[1], "test-model", "", 5};
    auto tools = std::make_shared<ToolRegistry>();
    const auto workspace = prepare_workspace_root(std::filesystem::absolute("workspace"));
    auto approvals = std::make_shared<agent::ui::TuiApprovalBroker>();
    const std::string_view mode = argc >= 3 ? argv[2] : "overflow";
    if (mode == "short-expiry") register_short_expiry_test_tool(*tools, workspace, approvals);
    else register_workspace_tools(*tools, workspace, approvals);
    agent::ui::EventQueueLimits limits;
    if (mode != "normal" && mode != "short-expiry") {
        limits.max_events = 1; // force bounded-queue overflow to exercise UI recovery path
        limits.max_event_payload_bytes = 1; // force TextDelta loss independent of scheduling
    }
    try { return agent::ui::TuiApplication(std::move(config), std::move(tools), std::move(approvals), [workspace] { apply_filesystem_sandbox(workspace); }, limits).run(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 3; }
}
