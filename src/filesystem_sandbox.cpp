#include "filesystem_sandbox.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>

std::filesystem::path prepare_workspace_root(const std::filesystem::path& workspace_root)
{
    if (!workspace_root.is_absolute()) {
        throw std::runtime_error("workspace root must be an absolute path");
    }

    std::error_code error;
    auto status = std::filesystem::symlink_status(workspace_root, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("cannot inspect workspace root: " + error.message());
    }
    if (std::filesystem::is_symlink(status)) {
        throw std::runtime_error("workspace root must be a real directory and not a symlink");
    }
    if (status.type() == std::filesystem::file_type::not_found) {
        error.clear();
        std::filesystem::create_directory(workspace_root, error);
        if (error) {
            throw std::runtime_error("cannot create workspace directory: " + error.message());
        }
    } else if (!std::filesystem::is_directory(status)) {
        throw std::runtime_error("workspace root must be a real directory and not a symlink");
    }

    status = std::filesystem::symlink_status(workspace_root, error);
    if (error || std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
        throw std::runtime_error("workspace root must be a real directory and not a symlink");
    }
    return workspace_root;
}

#ifdef __linux__
#include <fcntl.h>
#include <linux/landlock.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

class Fd {
public:
    explicit Fd(int fd = -1) : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) ::close(fd_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const { return fd_; }
private:
    int fd_;
};

int create_ruleset(const landlock_ruleset_attr& attr, std::size_t size, std::uint32_t flags)
{
    return static_cast<int>(::syscall(SYS_landlock_create_ruleset, &attr, size, flags));
}

void add_path_rule(int ruleset, const std::filesystem::path& path, std::uint64_t access)
{
    Fd object(::open(path.c_str(), O_PATH | O_CLOEXEC));
    if (object.get() < 0) {
        throw std::runtime_error("cannot open Landlock allow path " + path.string() + ": "
                                 + std::system_error(errno, std::generic_category()).what());
    }
    landlock_path_beneath_attr rule{};
    rule.allowed_access = access;
    rule.parent_fd = object.get();
    if (::syscall(SYS_landlock_add_rule, ruleset, LANDLOCK_RULE_PATH_BENEATH, &rule, 0) < 0) {
        throw std::runtime_error(std::string("cannot add Landlock path rule for ") + path.string() + ": "
                                 + std::system_error(errno, std::generic_category()).what());
    }
}

void add_workspace_rule(int ruleset, const std::filesystem::path& path, std::uint64_t access)
{
    if (!path.is_absolute()) {
        throw std::runtime_error("workspace root must be an absolute path");
    }
    Fd object(::open(path.c_str(), O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (object.get() < 0) {
        throw std::runtime_error("workspace root must be a real directory and not a symlink: "
                                 + path.string());
    }
    struct stat info {};
    if (::fstat(object.get(), &info) != 0 || !S_ISDIR(info.st_mode)) {
        throw std::runtime_error("workspace root must be a real directory and not a symlink: "
                                 + path.string());
    }
    landlock_path_beneath_attr rule{};
    rule.allowed_access = access;
    rule.parent_fd = object.get();
    if (::syscall(SYS_landlock_add_rule, ruleset, LANDLOCK_RULE_PATH_BENEATH, &rule, 0) < 0) {
        throw std::runtime_error(std::string("cannot add Landlock workspace rule: ")
                                 + std::system_error(errno, std::generic_category()).what());
    }
}

} // namespace

void apply_filesystem_sandbox(const std::filesystem::path& workspace_root)
{
    const int abi = static_cast<int>(::syscall(SYS_landlock_create_ruleset, nullptr, 0,
                                               LANDLOCK_CREATE_RULESET_VERSION));
    if (abi < 1) {
        throw std::runtime_error(std::string("Landlock is unavailable (")
                                 + std::system_error(errno, std::generic_category()).what()
                                 + "); refusing to start without filesystem isolation");
    }

    std::uint64_t handled = LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE
        | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR
        | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE
        | LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR
        | LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SOCK
        | LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK
        | LANDLOCK_ACCESS_FS_MAKE_SYM;
    if (abi >= 2) handled |= LANDLOCK_ACCESS_FS_REFER;
    if (abi >= 3) handled |= LANDLOCK_ACCESS_FS_TRUNCATE;
    if (abi >= 5) handled |= LANDLOCK_ACCESS_FS_IOCTL_DEV;

    landlock_ruleset_attr attr{};
    attr.handled_access_fs = handled;
    Fd ruleset(create_ruleset(attr, sizeof(attr), 0));
    if (ruleset.get() < 0) {
        throw std::runtime_error(std::string("cannot create Landlock ruleset: ")
                                 + std::system_error(errno, std::generic_category()).what());
    }

    add_workspace_rule(ruleset.get(), workspace_root, handled);

    for (const auto* path : {"/etc/ssl", "/etc/hosts", "/etc/resolv.conf", "/etc/nsswitch.conf"}) {
        if (!std::filesystem::exists(path)) continue;
        const auto access = std::filesystem::is_directory(path)
            ? LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR
            : LANDLOCK_ACCESS_FS_READ_FILE;
        add_path_rule(ruleset.get(), path, access);
    }

    if (::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        throw std::runtime_error(std::string("cannot set no_new_privs for Landlock: ")
                                 + std::system_error(errno, std::generic_category()).what());
    }
    if (::syscall(SYS_landlock_restrict_self, ruleset.get(), 0) < 0) {
        throw std::runtime_error(std::string("cannot enforce Landlock ruleset: ")
                                 + std::system_error(errno, std::generic_category()).what());
    }
}

#else

void apply_filesystem_sandbox(const std::filesystem::path&)
{
    throw std::runtime_error("filesystem sandbox is only implemented on Linux; refusing unrestricted startup");
}

#endif
