#include "workspace_write_authorization.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <system_error>
#include <sys/random.h>
#include <unistd.h>
#include <utility>

namespace {
std::atomic<std::uint64_t> next_approval_id{1};
constexpr auto approval_lifetime = std::chrono::minutes(5);

bool input_is_pending(int fd)
{
    pollfd input{fd, POLLIN, 0};
    int ready = 0;
    do { ready = ::poll(&input, 1, 0); } while (ready < 0 && errno == EINTR);
    return ready < 0 || (ready > 0 && input.revents != 0);
}

bool inside(const std::filesystem::path& root, const std::filesystem::path& path)
{
    auto r = root.begin();
    auto p = path.begin();
    for (; r != root.end() && p != path.end() && *r == *p; ++r, ++p) {}
    return r == root.end() && p != path.end();
}

unsigned hex_value(unsigned char c) { return c < 10 ? c + '0' : c - 10 + 'A'; }
void append_hex_byte(std::string& out, unsigned char c)
{
    out += "\\x";
    out.push_back(static_cast<char>(hex_value(c >> 4)));
    out.push_back(static_cast<char>(hex_value(c & 0x0f)));
}

bool decode_utf8(const std::string& s, std::size_t at, std::uint32_t& cp, std::size_t& count)
{
    const auto b0 = static_cast<unsigned char>(s[at]);
    if (b0 < 0x80) { cp = b0; count = 1; return true; }
    if (b0 >= 0xc2 && b0 <= 0xdf) { cp = b0 & 0x1f; count = 2; }
    else if (b0 >= 0xe0 && b0 <= 0xef) { cp = b0 & 0x0f; count = 3; }
    else if (b0 >= 0xf0 && b0 <= 0xf4) { cp = b0 & 0x07; count = 4; }
    else return false;
    if (at + count > s.size()) return false;
    for (std::size_t i = 1; i < count; ++i) {
        const auto b = static_cast<unsigned char>(s[at + i]);
        if ((b & 0xc0) != 0x80) return false;
        cp = (cp << 6) | (b & 0x3f);
    }
    if ((count == 3 && cp < 0x800) || (count == 4 && cp < 0x10000)
        || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    return true;
}

void append_codepoint_escape(std::string& out, std::uint32_t cp)
{
    constexpr char digits[] = "0123456789ABCDEF";
    char buf[8];
    std::size_t n = 0;
    do { buf[n++] = digits[cp & 0xf]; cp >>= 4; } while (cp != 0);
    out += "\\u{";
    while (n != 0) out.push_back(buf[--n]);
    out.push_back('}');
}
} // namespace

PreparedWrite::PreparedWrite(std::filesystem::path root,
                             std::filesystem::path relative,
                             std::string content,
                             std::uint64_t approval_id)
    : root_(std::move(root)), relative_(std::move(relative)), content_(std::move(content)), approval_id_(approval_id)
{
}

PreparedWrite::PreparedWrite(PreparedWrite&& other) noexcept
    : root_(std::move(other.root_)), relative_(std::move(other.relative_)), content_(std::move(other.content_)),
      approval_id_(std::exchange(other.approval_id_, 0)), consumed_(std::exchange(other.consumed_, true))
{
}

WorkspaceWriteAuthorizer::WorkspaceWriteAuthorizer(std::filesystem::path workspace_root,
                                                   std::shared_ptr<ApprovalProvider> provider)
    : root_(std::filesystem::canonical(std::move(workspace_root))), provider_(std::move(provider))
{
    if (!std::filesystem::is_directory(root_)) throw std::invalid_argument("workspace root is not a directory");
}

std::shared_ptr<const WriteApprovalRequest> WorkspaceWriteAuthorizer::prepare_request(
    const std::string& requested_path,
    const std::string& content,
    const ToolInvocationContext& invocation) const
{
    if (requested_path.empty() || requested_path.find('\0') != std::string::npos)
        throw std::invalid_argument("workspace path must not be empty or contain NUL");
    const std::filesystem::path relative(requested_path);
    if (relative.is_absolute()) throw std::invalid_argument("absolute workspace paths are not allowed");
    for (const auto& part : relative) if (part == "..") throw std::invalid_argument("workspace path must not contain '..'");
    if (content.size() > 200'000) {
        throw std::invalid_argument("content is " + std::to_string(content.size())
                                    + " bytes, exceeding the maximum write size of 200000 bytes");
    }

    std::error_code ec;
    const auto candidate = std::filesystem::weakly_canonical(root_ / relative, ec);
    if (ec || !inside(root_, candidate)) throw std::invalid_argument("workspace path escapes sandbox");
    if (candidate == root_ || candidate.filename().empty()) throw std::invalid_argument("workspace path must name a file");
    const auto effective_relative = candidate.lexically_relative(root_);

    ec.clear();
    const auto target_status = std::filesystem::symlink_status(candidate, ec);
    if (!ec && std::filesystem::exists(target_status)) {
        if (std::filesystem::is_directory(target_status)) {
            throw std::invalid_argument("cannot write file: path is an existing directory: "
                                        + effective_relative.generic_string());
        }
        throw std::invalid_argument("file already exists: " + effective_relative.generic_string()
                                    + " (overwriting is disabled until workspace access uses an isolated system identity)");
    }
    if (ec && ec != std::errc::no_such_file_or_directory) throw std::runtime_error("failed to inspect target file");

    std::vector<std::filesystem::path> missing;
    auto parent = root_;
    const auto relative_parent = effective_relative.parent_path();
    for (const auto& component : relative_parent) {
        if (component == "." || component.empty()) continue;
        parent /= component;
        ec.clear();
        const auto status = std::filesystem::symlink_status(parent, ec);
        if (ec == std::errc::no_such_file_or_directory || (!ec && !std::filesystem::exists(status))) {
            missing.push_back(parent.lexically_relative(root_));
            ec.clear();
            continue;
        }
        if (ec) throw std::runtime_error("failed to inspect workspace parent directory");
        if (!std::filesystem::is_directory(status)) throw std::invalid_argument("workspace parent is not a directory");
    }

    auto request = std::make_shared<WriteApprovalRequest>();
    request->approval_id = next_approval_id.fetch_add(1, std::memory_order_relaxed);
    if (request->approval_id == 0) throw std::overflow_error("approval request id exhausted");
    request->invocation = invocation;
    request->workspace_root = root_;
    request->relative_path = effective_relative;
    request->missing_parent_directories = std::move(missing);
    request->content = content;
    request->content_bytes = content.size();
    request->expires_at = std::chrono::steady_clock::now() + approval_lifetime;
    return request;
}

WritePreparation WorkspaceWriteAuthorizer::authorize(std::shared_ptr<const WriteApprovalRequest> request) const
{
    if (!request || !provider_) return ApprovalDecision::unavailable;
    if (request->approval_id == 0 || std::chrono::steady_clock::now() >= request->expires_at)
        return ApprovalDecision::expired;
    ApprovalDecision decision = ApprovalDecision::unavailable;
    try { decision = provider_->request(request); } catch (...) { decision = ApprovalDecision::unavailable; }
    if (std::chrono::steady_clock::now() >= request->expires_at)
        return ApprovalDecision::expired;
    if (decision != ApprovalDecision::approved) return decision;
    return PreparedWrite(request->workspace_root, request->relative_path, request->content, request->approval_id);
}

std::string escape_write_content_for_inspection(const std::string& content)
{
    std::string out;
    out.reserve(content.size());
    for (std::size_t i = 0; i < content.size();) {
        const auto byte = static_cast<unsigned char>(content[i]);
        if (byte < 0x80) {
            ++i;
            switch (byte) {
            case '\n': out += "\\n\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\\': out += "\\\\"; break;
            default:
                if (byte < 0x20 || byte == 0x7f) append_hex_byte(out, byte);
                else out.push_back(static_cast<char>(byte));
            }
            continue;
        }
        std::uint32_t cp = 0;
        std::size_t count = 0;
        if (!decode_utf8(content, i, cp, count)) { append_hex_byte(out, byte); ++i; continue; }
        append_codepoint_escape(out, cp);
        i += count;
    }
    return out;
}

std::string generate_approval_challenge()
{
    std::array<unsigned char, 16> random_bytes{};
    std::size_t offset = 0;
    while (offset < random_bytes.size()) {
        const auto count = ::getrandom(random_bytes.data() + offset, random_bytes.size() - offset, GRND_NONBLOCK);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw std::system_error(count < 0 ? errno : EIO, std::generic_category(), "getrandom");
        offset += static_cast<std::size_t>(count);
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string challenge;
    challenge.reserve(random_bytes.size() * 2);
    for (const auto byte : random_bytes) {
        challenge.push_back(digits[byte >> 4]);
        challenge.push_back(digits[byte & 0x0f]);
    }
    return challenge;
}

std::string approval_confirmation(std::string_view challenge)
{
    return "yes " + std::string(challenge);
}

bool matches_approval_confirmation(std::string_view challenge, std::string_view response) noexcept
{
    if (challenge.size() != 32 || response.size() != challenge.size() + 4
        || response.substr(0, 4) != "yes ") return false;
    unsigned char difference = 0;
    for (std::size_t i = 0; i < challenge.size(); ++i)
        difference |= static_cast<unsigned char>(challenge[i] ^ response[i + 4]);
    return difference == 0;
}

ConsoleWriteApprovalProvider::ConsoleWriteApprovalProvider()
    : ConsoleWriteApprovalProvider(STDIN_FILENO, STDERR_FILENO, std::cerr)
{
}

ConsoleWriteApprovalProvider::ConsoleWriteApprovalProvider(int input_fd, int diagnostic_fd, std::ostream& output)
    : input_fd_(input_fd), diagnostic_fd_(diagnostic_fd), output_(output)
{
}

ApprovalDecision ConsoleWriteApprovalProvider::request(std::shared_ptr<const WriteApprovalRequest> request)
{
    if (!request || cancelled_.load(std::memory_order_acquire)
        || input_fd_ < 0 || diagnostic_fd_ < 0 || !::isatty(input_fd_) || !::isatty(diagnostic_fd_))
        return ApprovalDecision::unavailable;

    // Input already queued belongs to the REPL or is typeahead. Fail closed
    // without consuming it so that it remains available to the next REPL read.
    if (input_is_pending(input_fd_)) return ApprovalDecision::unavailable;

    output_ << "\nWRITE APPROVAL REQUIRED\n"
              << "Workspace: " << escape_write_content_for_inspection(request->workspace_root.string()) << "\n"
              << "Create: " << escape_write_content_for_inspection(request->relative_path.generic_string()) << "\n"
              << "Size: " << request->content_bytes << " bytes\n"
              << "Missing parent directories:";
    if (request->missing_parent_directories.empty()) output_ << " none";
    for (const auto& parent : request->missing_parent_directories)
        output_ << " " << escape_write_content_for_inspection(parent.generic_string());
    output_ << "\nContent (ASCII escaped, reversible):\n"
              << escape_write_content_for_inspection(request->content) << '\n' << std::flush;
    if (!output_) return ApprovalDecision::unavailable;

    // Do not mistake input that arrived while rendering for consent; preserve
    // it for the REPL and refuse this operation instead.
    if (input_is_pending(input_fd_)) return ApprovalDecision::unavailable;
    std::string challenge;
    try { challenge = generate_approval_challenge(); }
    catch (...) { return ApprovalDecision::unavailable; }
    output_ << "Confirmation: type exactly 'yes " << challenge
            << "' to create this file; any other line refuses: " << std::flush;
    if (!output_) return ApprovalDecision::unavailable;

    std::string answer;
    bool too_long = false;
    for (;;) {
        if (cancelled_.load(std::memory_order_acquire)) return ApprovalDecision::cancelled;
        const auto now = std::chrono::steady_clock::now();
        if (now >= request->expires_at) return ApprovalDecision::expired;
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(request->expires_at - now);
        char c = 0;
        pollfd fd{input_fd_, POLLIN, 0};
        const int wait_ms = static_cast<int>(std::clamp<std::int64_t>(remaining.count(), 1, 1000));
        const int ready = ::poll(&fd, 1, wait_ms);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) return ApprovalDecision::unavailable;
        if (ready == 0) continue;
        const auto count = ::read(input_fd_, &c, 1);
        if (count == 0) return ApprovalDecision::cancelled;
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) return ApprovalDecision::unavailable;
        if (c == '\n') break;
        if (c == '\r') continue;
        if (answer.size() < 64) answer.push_back(c);
        else too_long = true;
    }
    output_ << '\n' << std::flush;
    if (too_long) return ApprovalDecision::denied;
    return matches_approval_confirmation(challenge, answer)
        ? ApprovalDecision::approved : ApprovalDecision::denied;
}

void ConsoleWriteApprovalProvider::cancel_pending() noexcept
{
    cancelled_.store(true, std::memory_order_release);
}
