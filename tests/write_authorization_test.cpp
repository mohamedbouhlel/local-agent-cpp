#include "test_support.hpp"
#include "test_approval_provider.hpp"
#include "prompt_reader.hpp"
#include "tool_registry.hpp"
#include "ui/tui_approval_broker.hpp"
#include "workspace_tools.hpp"
#include "workspace_write.hpp"
#include "workspace_write_authorization.hpp"

#include <fstream>
#include <future>
#include <iterator>
#include <condition_variable>
#include <mutex>
#include <pty.h>
#include <sstream>
#include <streambuf>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unistd.h>

namespace {

class RecordingApproval final : public ApprovalProvider {
public:
    ApprovalDecision request(std::shared_ptr<const WriteApprovalRequest> value) override
    {
        ++calls;
        request_value = std::move(value);
        return decision;
    }
    void cancel_pending() noexcept override { cancelled = true; }
    ApprovalDecision decision = ApprovalDecision::approved;
    std::shared_ptr<const WriteApprovalRequest> request_value;
    unsigned calls = 0;
    bool cancelled = false;
};

class SequentialApproval final : public ApprovalProvider {
public:
    ApprovalDecision request(std::shared_ptr<const WriteApprovalRequest> value) override
    {
        requests.push_back(std::move(value));
        return requests.size() == 1 ? ApprovalDecision::approved : ApprovalDecision::denied;
    }
    void cancel_pending() noexcept override {}
    std::vector<std::shared_ptr<const WriteApprovalRequest>> requests;
};

std::string read_text(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

class PromptSignalBuffer final : public std::streambuf {
public:
    void clear()
    {
        std::lock_guard lock(mutex_);
        contents_.clear();
    }

    std::string contents()
    {
        std::lock_guard lock(mutex_);
        return contents_;
    }

    bool wait_for_challenge(const std::string& marker, std::size_t challenge_size)
    {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2), [&] {
            const auto start = contents_.find(marker);
            return start != std::string::npos
                && contents_.size() >= start + marker.size() + challenge_size;
        });
    }

protected:
    int_type overflow(int_type ch) override
    {
        if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
        {
            std::lock_guard lock(mutex_);
            contents_.push_back(traits_type::to_char_type(ch));
        }
        changed_.notify_all();
        return ch;
    }

    std::streamsize xsputn(const char* data, std::streamsize size) override
    {
        {
            std::lock_guard lock(mutex_);
            contents_.append(data, static_cast<std::size_t>(size));
        }
        changed_.notify_all();
        return size;
    }

private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::string contents_;
};

std::string challenge_from(PromptSignalBuffer& output)
{
    constexpr std::string_view marker = "Confirmation: type exactly 'yes ";
    if (!output.wait_for_challenge(std::string(marker), 32)) return {};
    const auto contents = output.contents();
    const auto start = contents.find(marker);
    if (start == std::string::npos) return {};
    const auto token_start = start + marker.size();
    if (token_start + 32 > contents.size()) return {};
    return contents.substr(token_start, 32);
}

} // namespace

TEST(workspace, approval_prepares_exact_snapshot_and_writer_consumes_it_once)
{
    static_assert(!std::is_copy_constructible_v<PreparedWrite>);
    static_assert(std::is_move_constructible_v<PreparedWrite>);
    tests::TempDir workspace("approval_snapshot");
    auto provider = std::make_shared<RecordingApproval>();
    WorkspaceWriteAuthorizer authorizer(workspace.path(), provider);
    ToolInvocationContext context{7, 8, 9, "call-10"};
    auto request = authorizer.prepare_request("nested/file.txt", "exact\ncontent", context);
    CHECK(!std::filesystem::exists(workspace.path() / "nested"));
    CHECK_EQ(request->workspace_root, std::filesystem::canonical(workspace.path()));
    CHECK_EQ(request->relative_path, std::filesystem::path("nested/file.txt"));
    CHECK_EQ(request->content, std::string("exact\ncontent"));
    CHECK_EQ(request->content_bytes, std::size_t(13));
    CHECK_EQ(request->invocation.tool_call_id, std::string("call-10"));
    CHECK_EQ(request->missing_parent_directories.size(), std::size_t(1));
    auto result = authorizer.authorize(request);
    CHECK(std::holds_alternative<PreparedWrite>(result));
    auto& prepared = std::get<PreparedWrite>(result);
    CHECK_EQ(write_workspace(std::move(prepared)), std::string("wrote 13 bytes to nested/file.txt (created new file)"));
    CHECK_EQ(read_text(workspace.path() / "nested/file.txt"), std::string("exact\ncontent"));
    CHECK_THROWS_MSG(write_workspace(std::move(prepared)), "already consumed");
    CHECK_EQ(provider->calls, 1u);
}

TEST(workspace, refusals_cancellation_unavailable_and_expiry_create_nothing)
{
    for (const auto decision : {ApprovalDecision::denied, ApprovalDecision::cancelled,
                                ApprovalDecision::unavailable, ApprovalDecision::expired}) {
        tests::TempDir workspace("approval_refusal");
        auto provider = std::make_shared<RecordingApproval>();
        provider->decision = decision;
        ToolRegistry registry;
        register_workspace_tools(registry, workspace.path(), provider);
        const auto result = registry.execute("write_file", R"({"path":"parent/file.txt","content":"secret"})");
        CHECK(result.find("no file was created") != std::string::npos);
        CHECK(!std::filesystem::exists(workspace.path() / "parent"));
        CHECK_EQ(provider->calls, 1u);
    }
}

TEST(workspace, no_provider_and_invalid_preflight_fail_closed_before_parent_creation)
{
    tests::TempDir workspace("approval_unavailable");
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path(), {});
    const auto unavailable = registry.execute("write_file", R"({"path":"a/b.txt","content":"x"})");
    CHECK(unavailable.find("unavailable") != std::string::npos);
    CHECK(!std::filesystem::exists(workspace.path() / "a"));

    auto provider = std::make_shared<RecordingApproval>();
    ToolRegistry validating;
    register_workspace_tools(validating, workspace.path(), provider);
    CHECK_THROWS_MSG(validating.execute("write_file", R"({"path":"../outside","content":"x"})"), "'..'");
    CHECK_THROWS_MSG(validating.execute("write_file", "{\"path\":\"big\",\"content\":\"" + std::string(200'001, 'x') + "\"}"), "maximum write size");
    CHECK_EQ(provider->calls, 0u);
    CHECK(!std::filesystem::exists(workspace.path() / "a"));
}

TEST(workspace, target_created_after_approval_is_not_overwritten)
{
    tests::TempDir workspace("approval_race");
    auto provider = std::make_shared<RecordingApproval>();
    WorkspaceWriteAuthorizer authorizer(workspace.path(), provider);
    auto request = authorizer.prepare_request("parent/race.txt", "approved snapshot", {});
    auto result = authorizer.authorize(request);
    CHECK(std::holds_alternative<PreparedWrite>(result));
    std::filesystem::create_directories(workspace.path() / "parent");
    { std::ofstream out(workspace.path() / "parent/race.txt"); out << "competitor"; }
    CHECK_THROWS_MSG(write_workspace(std::move(std::get<PreparedWrite>(result))), "already exists");
    CHECK_EQ(read_text(workspace.path() / "parent/race.txt"), std::string("competitor"));
}

TEST(workspace, approval_is_per_call_and_denial_does_not_reuse_previous_approval)
{
    tests::TempDir workspace("approval_per_call");
    auto provider = std::make_shared<SequentialApproval>();
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path(), provider);
    const auto first = registry.execute("write_file", R"({"path":"one.txt","content":"first"})");
    const auto second = registry.execute("write_file", R"({"path":"two.txt","content":"second"})");
    CHECK(first.find("created new file") != std::string::npos);
    CHECK(second.find("authorization denied") != std::string::npos);
    CHECK_EQ(provider->requests.size(), std::size_t(2));
    CHECK(provider->requests[0]->approval_id != provider->requests[1]->approval_id);
    CHECK_EQ(provider->requests[0]->relative_path, std::filesystem::path("one.txt"));
    CHECK_EQ(provider->requests[0]->content, std::string("first"));
    CHECK_EQ(provider->requests[1]->relative_path, std::filesystem::path("two.txt"));
    CHECK_EQ(provider->requests[1]->content, std::string("second"));
    CHECK(std::filesystem::exists(workspace.path() / "one.txt"));
    CHECK(!std::filesystem::exists(workspace.path() / "two.txt"));
}

TEST(workspace, read_tools_do_not_consult_approval_provider)
{
    tests::TempDir workspace("approval_read_only");
    { std::ofstream out(workspace.path() / "read.txt"); out << "readable"; }
    auto provider = std::make_shared<RecordingApproval>();
    ToolRegistry registry;
    register_workspace_tools(registry, workspace.path(), provider);
    CHECK_EQ(registry.execute("read_file", R"({"path":"read.txt"})"), std::string("readable"));
    CHECK_EQ(provider->calls, 0u);
}

TEST(workspace, inspection_encoding_is_reversible_ascii_without_terminal_controls)
{
    const std::string hostile = "A\x1b]0;title\x07\n\t\xc3\xa9\xe2\x80\xae";
    const auto escaped = escape_write_content_for_inspection(hostile);
    CHECK(escaped.find('\x1b') == std::string::npos);
    CHECK(escaped.find('\x07') == std::string::npos);
    CHECK(escaped.find("\\x1B") != std::string::npos);
    CHECK(escaped.find("\\u{E9}") != std::string::npos);
    CHECK(escaped.find("\\u{202E}") != std::string::npos);
}

TEST(workspace, approval_challenges_are_fresh_and_require_the_exact_confirmation)
{
    const auto first = generate_approval_challenge();
    const auto second = generate_approval_challenge();
    CHECK_EQ(first.size(), std::size_t(32));
    CHECK_EQ(second.size(), std::size_t(32));
    CHECK(first != second);
    CHECK(matches_approval_confirmation(first, approval_confirmation(first)));
    CHECK(!matches_approval_confirmation(first, "yes"));
    CHECK(!matches_approval_confirmation(first, "y"));
    CHECK(!matches_approval_confirmation(first, approval_confirmation(second)));
    CHECK(!matches_approval_confirmation(first, approval_confirmation(first) + " "));
}

TEST(workspace, cli_provider_refuses_pretyped_yes_and_accepts_current_challenge)
{
    using namespace std::chrono_literals;
    int master = -1;
    int slave = -1;
    CHECK_EQ(::openpty(&master, &slave, nullptr, nullptr, nullptr), 0);
    if (master < 0 || slave < 0) return;
    auto request = std::make_shared<WriteApprovalRequest>();
    request->approval_id = 77;
    request->workspace_root = "/tmp/workspace";
    request->relative_path = "new.txt";
    request->content = "line\n";
    request->content_bytes = request->content.size();
    request->expires_at = std::chrono::steady_clock::now() + std::chrono::seconds(2);

    PromptSignalBuffer prompt_buffer;
    std::ostream output(&prompt_buffer);
    ConsoleWriteApprovalProvider provider(slave, slave, output);
    CHECK_EQ(::write(master, "yes\n", 4), 4);
    CHECK(provider.request(request) == ApprovalDecision::unavailable);

    FdInputStreamBuffer input_buffer(slave);
    std::istream input(&input_buffer);
    std::ostringstream repl_output;
    PromptReader reader(input, repl_output);
    const auto queued_repl_line = reader.read();
    CHECK(queued_repl_line.status == PromptStatus::Line);
    CHECK_EQ(queued_repl_line.content, std::string("yes"));

    request->expires_at = std::chrono::steady_clock::now() + 2s;
    auto pending = std::async(std::launch::async, [&] { return provider.request(request); });
    const auto challenge = challenge_from(prompt_buffer);
    CHECK_EQ(challenge.size(), std::size_t(32));
    CHECK(pending.wait_for(0ms) == std::future_status::timeout);
    const auto confirmation = approval_confirmation(challenge) + '\n';
    CHECK_EQ(::write(master, confirmation.data(), confirmation.size()), static_cast<ssize_t>(confirmation.size()));
    CHECK(pending.get() == ApprovalDecision::approved);

    request->approval_id = 82;
    request->expires_at = std::chrono::steady_clock::now() + 2s;
    PromptSignalBuffer second_prompt_buffer;
    std::ostream second_output(&second_prompt_buffer);
    ConsoleWriteApprovalProvider second_provider(slave, slave, second_output);
    auto second = std::async(std::launch::async, [&] { return second_provider.request(request); });
    const auto second_challenge = challenge_from(second_prompt_buffer);
    CHECK_EQ(second_challenge.size(), std::size_t(32));
    CHECK(second_challenge != challenge);
    const auto stale_confirmation = approval_confirmation(challenge) + '\n';
    CHECK_EQ(::write(master, stale_confirmation.data(), stale_confirmation.size()), static_cast<ssize_t>(stale_confirmation.size()));
    CHECK(second.get() == ApprovalDecision::denied);

    request->expires_at = std::chrono::steady_clock::now() + 2s;
    PromptSignalBuffer refused_buffer;
    std::ostream refused_output(&refused_buffer);
    ConsoleWriteApprovalProvider refused_provider(slave, slave, refused_output);
    auto refused = std::async(std::launch::async, [&] { return refused_provider.request(request); });
    CHECK_EQ(challenge_from(refused_buffer).size(), std::size_t(32));
    CHECK_EQ(::write(master, "YES\n", 4), 4);
    CHECK(refused.get() == ApprovalDecision::denied);

    ::close(master);
    ::close(slave);
}

TEST(workspace, cli_provider_treats_eof_and_expiration_as_non_approval)
{
    using namespace std::chrono_literals;
    int master = -1;
    int slave = -1;
    CHECK_EQ(::openpty(&master, &slave, nullptr, nullptr, nullptr), 0);
    if (master < 0 || slave < 0) return;
    auto request = std::make_shared<WriteApprovalRequest>();
    request->approval_id = 80;
    request->expires_at = std::chrono::steady_clock::now() + 2s;
    PromptSignalBuffer output_buffer;
    std::ostream output(&output_buffer);
    ConsoleWriteApprovalProvider eof_provider(slave, slave, output);
    auto eof = std::async(std::launch::async, [&] { return eof_provider.request(request); });
    CHECK_EQ(challenge_from(output_buffer).size(), std::size_t(32));
    CHECK_EQ(::write(master, "\x04", 1), 1);
    CHECK(eof.get() == ApprovalDecision::cancelled);

    request->approval_id = 81;
    request->expires_at = std::chrono::steady_clock::now() + 25ms;
    std::ostringstream expired_output;
    ConsoleWriteApprovalProvider expired_provider(slave, slave, expired_output);
    CHECK(expired_provider.request(request) == ApprovalDecision::expired);
    ::close(master);
    ::close(slave);
}

TEST(workspace, cli_authorizer_requires_fresh_challenge_and_partial_prefill_creates_nothing)
{
    using namespace std::chrono_literals;
    tests::TempDir workspace("approval_challenge_freshness");
    int master = -1;
    int slave = -1;
    CHECK_EQ(::openpty(&master, &slave, nullptr, nullptr, nullptr), 0);
    if (master < 0 || slave < 0) return;

    PromptSignalBuffer output_buffer;
    std::ostream output(&output_buffer);
    auto provider = std::make_shared<ConsoleWriteApprovalProvider>(slave, slave, output);
    WorkspaceWriteAuthorizer authorizer(workspace.path(), provider);

    CHECK_EQ(::write(master, "yes\n", 4), 4);
    auto complete_prefill = authorizer.prepare_request("early/complete.txt", "no", {});
    auto refused_early = authorizer.authorize(complete_prefill);
    CHECK(std::holds_alternative<ApprovalDecision>(refused_early));
    CHECK(std::get<ApprovalDecision>(refused_early) == ApprovalDecision::unavailable);
    CHECK(!std::filesystem::exists(workspace.path() / "early"));

    FdInputStreamBuffer input_buffer(slave);
    std::istream input(&input_buffer);
    std::ostringstream repl_output;
    PromptReader reader(input, repl_output);
    CHECK_EQ(reader.read().content, std::string("yes"));

    output_buffer.clear();
    CHECK_EQ(::write(master, "yes", 3), 3);
    auto partial_request = authorizer.prepare_request("early/partial.txt", "no", {});
    auto partial = std::async(std::launch::async, [&] { return authorizer.authorize(partial_request); });
    CHECK_EQ(challenge_from(output_buffer).size(), std::size_t(32));
    CHECK_EQ(::write(master, "\n", 1), 1);
    auto partial_result = partial.get();
    CHECK(std::holds_alternative<ApprovalDecision>(partial_result));
    CHECK(std::get<ApprovalDecision>(partial_result) == ApprovalDecision::denied);
    CHECK(!std::filesystem::exists(workspace.path() / "early"));

    output_buffer.clear();
    auto approved_request = authorizer.prepare_request("approved/file.txt", "approved exact body", {});
    auto approved = std::async(std::launch::async, [&] { return authorizer.authorize(approved_request); });
    const auto current_challenge = challenge_from(output_buffer);
    CHECK_EQ(current_challenge.size(), std::size_t(32));
    auto current_response = approval_confirmation(current_challenge) + '\n';
    CHECK_EQ(::write(master, current_response.data(), current_response.size()), static_cast<ssize_t>(current_response.size()));
    auto approved_result = approved.get();
    CHECK(std::holds_alternative<PreparedWrite>(approved_result));
    CHECK(write_workspace(std::move(std::get<PreparedWrite>(approved_result))).find("created new file") != std::string::npos);
    CHECK_EQ(read_text(workspace.path() / "approved/file.txt"), std::string("approved exact body"));

    output_buffer.clear();
    auto stale_request = authorizer.prepare_request("stale/file.txt", "must not exist", {});
    auto stale = std::async(std::launch::async, [&] { return authorizer.authorize(stale_request); });
    const auto next_challenge = challenge_from(output_buffer);
    CHECK_EQ(next_challenge.size(), std::size_t(32));
    CHECK(next_challenge != current_challenge);
    auto stale_response = approval_confirmation(current_challenge) + '\n';
    CHECK_EQ(::write(master, stale_response.data(), stale_response.size()), static_cast<ssize_t>(stale_response.size()));
    auto stale_result = stale.get();
    CHECK(std::holds_alternative<ApprovalDecision>(stale_result));
    CHECK(std::get<ApprovalDecision>(stale_result) == ApprovalDecision::denied);
    CHECK(!std::filesystem::exists(workspace.path() / "stale"));

    ::close(master);
    ::close(slave);
}

TEST(workspace, cli_late_confirmation_after_expiration_creates_nothing)
{
    using namespace std::chrono_literals;
    tests::TempDir workspace("approval_late_response");
    int master = -1;
    int slave = -1;
    CHECK_EQ(::openpty(&master, &slave, nullptr, nullptr, nullptr), 0);
    if (master < 0 || slave < 0) return;
    PromptSignalBuffer output_buffer;
    std::ostream output(&output_buffer);
    auto provider = std::make_shared<ConsoleWriteApprovalProvider>(slave, slave, output);
    WorkspaceWriteAuthorizer authorizer(workspace.path(), provider);
    auto request = std::make_shared<WriteApprovalRequest>(
        *authorizer.prepare_request("late/file.txt", "must not exist", {}));
    request->expires_at = std::chrono::steady_clock::now() + 5s;
    auto pending = std::async(std::launch::async, [&] { return authorizer.authorize(request); });
    const auto challenge = challenge_from(output_buffer);
    CHECK_EQ(challenge.size(), std::size_t(32));
    CHECK(pending.wait_for(6s) == std::future_status::ready);
    auto result = pending.get();
    CHECK(std::holds_alternative<ApprovalDecision>(result));
    CHECK(std::get<ApprovalDecision>(result) == ApprovalDecision::expired);
    const auto late = approval_confirmation(challenge) + '\n';
    CHECK_EQ(::write(master, late.data(), late.size()), static_cast<ssize_t>(late.size()));
    CHECK(!std::filesystem::exists(workspace.path() / "late"));
    ::close(master);
    ::close(slave);
}

TEST(workspace, cli_provider_returns_unavailable_without_interactive_tty)
{
    auto request = std::make_shared<WriteApprovalRequest>();
    request->approval_id = 78;
    request->expires_at = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    std::ostringstream output;
    ConsoleWriteApprovalProvider provider(-1, -1, output);
    CHECK(provider.request(request) == ApprovalDecision::unavailable);
}

TEST(workspace, cli_provider_refuses_and_preserves_repl_typeahead)
{
    int master = -1;
    int slave = -1;
    CHECK_EQ(::openpty(&master, &slave, nullptr, nullptr, nullptr), 0);
    if (master < 0 || slave < 0) return;
    FdInputStreamBuffer input_buffer(slave);
    std::istream input(&input_buffer);
    std::ostringstream prompt_output;
    PromptReader reader(input, prompt_output);
    CHECK_EQ(::write(master, "prompt\nyes\n", 11), 11);
    const auto prompt = reader.read();
    CHECK(prompt.status == PromptStatus::Line);
    CHECK_EQ(prompt.content, std::string("prompt"));

    auto request = std::make_shared<WriteApprovalRequest>();
    request->approval_id = 79;
    request->expires_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
    std::ostringstream output;
    ConsoleWriteApprovalProvider provider(slave, slave, output);
    CHECK(provider.request(request) == ApprovalDecision::unavailable);
    const auto queued_repl_line = reader.read();
    CHECK(queued_repl_line.status == PromptStatus::Line);
    CHECK_EQ(queued_repl_line.content, std::string("yes"));

    ::close(master);
    ::close(slave);
    std::cin.clear();
}

TEST(tui2, approval_broker_accepts_only_current_request_and_close_wakes_waiter)
{
    using namespace std::chrono_literals;
    agent::ui::TuiApprovalBroker broker;
    auto request = std::make_shared<WriteApprovalRequest>();
    request->approval_id = 42;
    request->expires_at = std::chrono::steady_clock::now() + 2s;
    auto waiting = std::async(std::launch::async, [&] { return broker.request(request); });
    const auto wait_until_pending = [](agent::ui::TuiApprovalBroker& target) {
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (std::chrono::steady_clock::now() < deadline) {
            if (auto pending = target.pending_request()) return pending;
            std::this_thread::sleep_for(1ms);
        }
        return std::shared_ptr<const WriteApprovalRequest>{};
    };
    auto pending = wait_until_pending(broker);
    CHECK(pending != nullptr);
    CHECK(!broker.resolve(41, ApprovalDecision::approved));
    CHECK(broker.resolve(42, ApprovalDecision::approved));
    CHECK(waiting.get() == ApprovalDecision::approved);
    CHECK(!broker.resolve(42, ApprovalDecision::approved));

    agent::ui::TuiApprovalBroker closing;
    auto second = std::make_shared<WriteApprovalRequest>();
    second->approval_id = 43;
    second->expires_at = std::chrono::steady_clock::now() + 2s;
    auto blocked = std::async(std::launch::async, [&] { return closing.request(second); });
    const auto closing_pending = wait_until_pending(closing);
    CHECK(closing_pending != nullptr);
    closing.cancel_pending();
    CHECK(blocked.wait_for(1s) == std::future_status::ready);
    CHECK(blocked.get() == ApprovalDecision::cancelled);
    CHECK(!closing.resolve(43, ApprovalDecision::approved));
}

TEST(tui2, approval_broker_expires_without_decision)
{
    agent::ui::TuiApprovalBroker broker;
    auto request = std::make_shared<WriteApprovalRequest>();
    request->approval_id = 51;
    request->expires_at = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    CHECK(broker.request(request) == ApprovalDecision::expired);
    CHECK(!broker.resolve(51, ApprovalDecision::approved));
}
