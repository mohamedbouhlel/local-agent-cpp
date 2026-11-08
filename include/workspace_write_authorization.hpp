#pragma once

#include <chrono>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct ToolInvocationContext {
    std::uint64_t session_id = 0;
    std::uint64_t turn_id = 0;
    std::uint64_t request_id = 0;
    std::string tool_call_id;
};

enum class ApprovalDecision { approved, denied, cancelled, unavailable, expired };

struct WriteApprovalRequest {
    std::uint64_t approval_id = 0;
    ToolInvocationContext invocation;
    std::filesystem::path workspace_root;
    std::filesystem::path relative_path;
    std::vector<std::filesystem::path> missing_parent_directories;
    std::string content;
    std::size_t content_bytes = 0;
    std::chrono::steady_clock::time_point expires_at;
};

class ApprovalProvider {
public:
    virtual ~ApprovalProvider() = default;
    virtual ApprovalDecision request(std::shared_ptr<const WriteApprovalRequest> request) = 0;
    virtual void cancel_pending() noexcept = 0;
};

class WorkspaceWriteAuthorizer;

class PreparedWrite final {
public:
    PreparedWrite(const PreparedWrite&) = delete;
    PreparedWrite& operator=(const PreparedWrite&) = delete;
    PreparedWrite(PreparedWrite&& other) noexcept;
    PreparedWrite& operator=(PreparedWrite&&) = delete;
    ~PreparedWrite() = default;

private:
    PreparedWrite(std::filesystem::path root,
                  std::filesystem::path relative,
                  std::string content,
                  std::uint64_t approval_id);
    std::filesystem::path root_;
    std::filesystem::path relative_;
    std::string content_;
    std::uint64_t approval_id_ = 0;
    bool consumed_ = false;
    friend class WorkspaceWriteAuthorizer;
    friend std::string write_workspace(PreparedWrite&&);
};

using WritePreparation = std::variant<PreparedWrite, ApprovalDecision>;

class WorkspaceWriteAuthorizer final {
public:
    WorkspaceWriteAuthorizer(std::filesystem::path workspace_root,
                             std::shared_ptr<ApprovalProvider> provider);

    std::shared_ptr<const WriteApprovalRequest> prepare_request(
        const std::string& requested_path,
        const std::string& content,
        const ToolInvocationContext& invocation) const;
    WritePreparation authorize(std::shared_ptr<const WriteApprovalRequest> request) const;

private:
    std::filesystem::path root_;
    std::shared_ptr<ApprovalProvider> provider_;
};

class ConsoleWriteApprovalProvider final : public ApprovalProvider {
public:
    ConsoleWriteApprovalProvider();
    ConsoleWriteApprovalProvider(int input_fd, int diagnostic_fd, std::ostream& output);
    ApprovalDecision request(std::shared_ptr<const WriteApprovalRequest> request) override;
    void cancel_pending() noexcept override;
private:
    int input_fd_;
    int diagnostic_fd_;
    std::ostream& output_;
    std::atomic<bool> cancelled_{false};
};

// Reversible, ASCII-only display representation. It never emits terminal
// controls, and line breaks are shown as visible escapes for complete review.
std::string escape_write_content_for_inspection(const std::string& content);

// Generates a fresh 128-bit OS-random challenge. Failure must be handled as
// unavailable; callers must never substitute a predictable value.
std::string generate_approval_challenge();
std::string approval_confirmation(std::string_view challenge);
bool matches_approval_confirmation(std::string_view challenge, std::string_view response) noexcept;
