#include "workspace_write.hpp"
#include "workspace_write_internal.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <optional>
#include <string>
#include <stdexcept>
#include <utility>

namespace {
class Fd {
public:
    explicit Fd(int fd) : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) ::close(fd_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const { return fd_; }
    void replace(int fd) { if (fd_ >= 0) ::close(fd_); fd_ = fd; }
private:
    int fd_;
};

class Temporary {
public:
    Temporary(int dir, std::string name) : dir_(dir), name_(std::move(name)) {}
    ~Temporary() { if (active_) ::unlinkat(dir_, name_.c_str(), 0); }
    void release() { active_ = false; }
private:
    int dir_;
    std::string name_;
    bool active_ = true;
};

std::string exists_error(const std::string& relative)
{
    return "file already exists: " + relative
        + " (overwriting is disabled until workspace access uses an isolated system identity)";
}
} // namespace

void workspace_write_internal::verify_published_file(int directory_fd, const std::string& name, int verified_fd)
{
    struct stat verified {};
    struct stat published {};
    if (::fstat(verified_fd, &verified) != 0
        || ::fstatat(directory_fd, name.c_str(), &published, AT_SYMLINK_NOFOLLOW) != 0
        || !S_ISREG(published.st_mode)
        || published.st_dev != verified.st_dev || published.st_ino != verified.st_ino) {
        throw std::runtime_error("published file differs from verified temporary file");
    }
}

std::string write_workspace(const std::filesystem::path& root,
                            const std::filesystem::path& relative,
                            const std::string& content)
{
    if (relative.empty() || relative.is_absolute() || relative.filename().empty()
        || relative.filename() == "." || relative.native().find('\0') != std::string::npos) {
        throw std::invalid_argument("invalid workspace path");
    }
    for (const auto& component : relative) {
        if (component == "..") throw std::invalid_argument("workspace path must not contain '..'");
    }
    const auto display = relative.generic_string();
    const auto filename = relative.filename().string();
    Fd dir(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (dir.get() < 0) throw std::runtime_error("failed to open workspace directory");
    for (const auto& component : relative.parent_path()) {
        const auto name = component.string();
        if (name.empty() || name == ".") continue;
        int child = ::openat(dir.get(), name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (child < 0 && errno == ENOENT) {
            if (::mkdirat(dir.get(), name.c_str(), 0777) != 0 && errno != EEXIST)
                throw std::runtime_error("failed to create parent directories");
            child = ::openat(dir.get(), name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        }
        if (child < 0) throw std::runtime_error("workspace path escapes sandbox or parent is not a directory");
        dir.replace(child);
    }

    struct stat target {};
    const bool exists = ::fstatat(dir.get(), filename.c_str(), &target, AT_SYMLINK_NOFOLLOW) == 0;
    if (!exists && errno != ENOENT) throw std::runtime_error("failed to inspect target file");
    if (exists && S_ISDIR(target.st_mode)) throw std::runtime_error("cannot write file: path is an existing directory: " + display);
    if (exists && !S_ISREG(target.st_mode)) throw std::runtime_error("workspace path is not a regular file");
    if (exists) throw std::runtime_error(exists_error(display));

    static std::atomic<unsigned long> counter{0};
    std::string temporary;
    bool anonymous_temporary = false;
    int raw = -1;
#ifdef __linux__
    raw = ::openat(dir.get(), ".", O_TMPFILE | O_RDWR | O_CLOEXEC, 0666);
    if (raw < 0)
        throw std::runtime_error("failed to open file for writing: anonymous temporary file unavailable");
    anonymous_temporary = true;
#endif
    if (raw < 0) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            temporary = ".agent-tmp-" + std::to_string(::getpid()) + "-" + std::to_string(counter.fetch_add(1));
            raw = ::openat(dir.get(), temporary.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0666);
            if (raw >= 0 || errno != EEXIST) break;
        }
    }
    if (raw < 0) throw std::runtime_error("failed to open file for writing");
    Fd file(raw);
    std::optional<Temporary> cleanup;
    if (!anonymous_temporary) cleanup.emplace(dir.get(), temporary);
    std::size_t written = 0;
    while (written < content.size()) {
        const auto count = ::write(file.get(), content.data() + written, content.size() - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw std::runtime_error("failed to write file content");
        written += static_cast<std::size_t>(count);
    }
    if (::lseek(file.get(), 0, SEEK_SET) < 0) throw std::runtime_error("post-write verification failed");
    std::string actual(content.size(), '\0');
    std::size_t read_bytes = 0;
    while (read_bytes < actual.size()) {
        const auto count = ::read(file.get(), actual.data() + read_bytes, actual.size() - read_bytes);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw std::runtime_error("post-write verification failed");
        read_bytes += static_cast<std::size_t>(count);
    }
    struct stat written_stat {};
    if (::fstat(file.get(), &written_stat) != 0 || written_stat.st_size != static_cast<off_t>(content.size()) || actual != content)
        throw std::runtime_error("post-write verification failed");

    if (anonymous_temporary) {
        if (::linkat(file.get(), "", dir.get(), filename.c_str(), AT_EMPTY_PATH) != 0) {
            if (errno == EEXIST) throw std::runtime_error(exists_error(display));
            throw std::runtime_error("failed to publish anonymous temporary file");
        }
    } else {
        if (::linkat(dir.get(), temporary.c_str(), dir.get(), filename.c_str(), 0) != 0) {
            if (errno == EEXIST) throw std::runtime_error(exists_error(display));
            throw std::runtime_error("failed to publish target file");
        }
    }
    workspace_write_internal::verify_published_file(dir.get(), filename, file.get());
    return "wrote " + std::to_string(content.size()) + " bytes to " + display
        + " (created new file)";
}
