#pragma once

#include "workspace_write_authorization.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>

namespace agent::ui {

class TuiApprovalBroker final : public ApprovalProvider {
public:
    ApprovalDecision request(std::shared_ptr<const WriteApprovalRequest> request) override;
    void cancel_pending() noexcept override;
    std::shared_ptr<const WriteApprovalRequest> pending_request() const;
    bool resolve(std::uint64_t approval_id, ApprovalDecision decision) noexcept;
    bool is_pending(std::uint64_t approval_id) const noexcept;

private:
    struct Pending {
        std::shared_ptr<const WriteApprovalRequest> request;
        std::optional<ApprovalDecision> decision;
    };
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::optional<Pending> pending_;
    bool closed_ = false;
};

} // namespace agent::ui
