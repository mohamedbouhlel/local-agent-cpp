#pragma once

#include "workspace_write_authorization.hpp"

// Explicit test-only decision source. Production code never uses this provider.
class TestApprovalProvider final : public ApprovalProvider {
public:
    ApprovalDecision request(std::shared_ptr<const WriteApprovalRequest>) override
    {
        return closed_ ? ApprovalDecision::cancelled : decision_;
    }
    void cancel_pending() noexcept override { closed_ = true; }
    ApprovalDecision decision_ = ApprovalDecision::approved;
private:
    bool closed_ = false;
};

inline std::shared_ptr<ApprovalProvider> test_approval_provider()
{
    return std::make_shared<TestApprovalProvider>();
}
