#include "ui/tui_approval_broker.hpp"

namespace agent::ui {

ApprovalDecision TuiApprovalBroker::request(std::shared_ptr<const WriteApprovalRequest> request)
{
    if (!request) return ApprovalDecision::unavailable;
    std::unique_lock lock(mutex_);
    if (closed_ || pending_) return ApprovalDecision::unavailable;
    pending_.emplace(Pending{std::move(request), std::nullopt});
    const auto id = pending_->request->approval_id;
    const auto deadline = pending_->request->expires_at;
    changed_.notify_all();
    changed_.wait_until(lock, deadline, [this, id] {
        return closed_ || !pending_ || pending_->request->approval_id != id || pending_->decision.has_value();
    });
    if (closed_) return ApprovalDecision::cancelled;
    if (!pending_ || pending_->request->approval_id != id) return ApprovalDecision::cancelled;
    auto result = pending_->decision.value_or(ApprovalDecision::expired);
    pending_.reset();
    changed_.notify_all();
    return result;
}

void TuiApprovalBroker::cancel_pending() noexcept
{
    std::lock_guard lock(mutex_);
    closed_ = true;
    if (pending_ && !pending_->decision) pending_->decision = ApprovalDecision::cancelled;
    changed_.notify_all();
}

std::shared_ptr<const WriteApprovalRequest> TuiApprovalBroker::pending_request() const
{
    std::lock_guard lock(mutex_);
    if (closed_ || !pending_ || pending_->decision) return {};
    return pending_->request;
}

bool TuiApprovalBroker::resolve(std::uint64_t approval_id, ApprovalDecision decision) noexcept
{
    if (decision == ApprovalDecision::expired) return false;
    std::lock_guard lock(mutex_);
    if (closed_ || !pending_ || pending_->decision || pending_->request->approval_id != approval_id
        || std::chrono::steady_clock::now() >= pending_->request->expires_at) return false;
    pending_->decision = decision;
    changed_.notify_all();
    return true;
}

bool TuiApprovalBroker::is_pending(std::uint64_t approval_id) const noexcept
{
    std::lock_guard lock(mutex_);
    return !closed_ && pending_ && !pending_->decision && pending_->request->approval_id == approval_id;
}

} // namespace agent::ui
