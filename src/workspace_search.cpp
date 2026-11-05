#include "workspace_search.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
class Fd {
public:
    explicit Fd(int value) : value_(value) {}
    ~Fd() { if (value_ >= 0) ::close(value_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const { return value_; }
private:
    int value_;
};

struct State {
    std::ostringstream output;
    std::size_t count = 0;
    bool truncated = false;
};

struct DirCloser { void operator()(DIR* value) const { if (value) ::closedir(value); } };
struct FileCloser { void operator()(FILE* value) const { if (value) ::fclose(value); } };

void match_line(State& state, const std::string& path, std::size_t number,
                const std::string& line, const std::string& query)
{
    if (line.find(query) == std::string::npos) return;
    if (state.count == 50) { state.truncated = true; return; }
    state.output << path << ':' << number << ':' << line << '\n';
    ++state.count;
}

void walk(int dir, const std::string& prefix, const std::string* query, State& state)
{
    const int copy = ::dup(dir);
    if (copy < 0) throw std::runtime_error("failed to duplicate workspace directory");
    std::unique_ptr<DIR, DirCloser> entries(::fdopendir(copy));
    if (!entries) { ::close(copy); throw std::runtime_error("failed to enumerate workspace"); }
    while (!state.truncated) {
        errno = 0;
        const auto* entry = ::readdir(entries.get());
        if (!entry) {
            if (errno) throw std::runtime_error("failed to enumerate workspace");
            break;
        }
        const std::string name(entry->d_name);
        if (name.empty() || name.front() == '.') continue;
        const std::string path = prefix.empty() ? name : prefix + '/' + name;
        if (!query) {
            struct stat status {};
            if (::fstatat(dir, name.c_str(), &status, AT_SYMLINK_NOFOLLOW) != 0) continue;
            if (S_ISREG(status.st_mode)) { state.output << path << '\n'; continue; }
            if (!S_ISDIR(status.st_mode)) continue;
        }
        Fd child(::openat(dir, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
        if (child.get() < 0) continue;
        struct stat info {};
        if (::fstat(child.get(), &info) != 0) continue;
        if (S_ISDIR(info.st_mode)) { walk(child.get(), path, query, state); continue; }
        if (!query) continue;
        if (!S_ISREG(info.st_mode)) continue;
        if (info.st_nlink != 1) continue;

        std::array<char, 4096> probe{};
        const auto n = ::read(child.get(), probe.data(), probe.size());
        if (n < 0 || std::find(probe.begin(), probe.begin() + n, '\0') != probe.begin() + n
            || ::lseek(child.get(), 0, SEEK_SET) < 0) continue;
        const int file_copy = ::dup(child.get());
        if (file_copy < 0) throw std::runtime_error("failed to duplicate workspace file");
        std::unique_ptr<FILE, FileCloser> file(::fdopen(file_copy, "r"));
        if (!file) { ::close(file_copy); continue; }
        std::string line;
        std::size_t number = 0;
        int c;
        while (!state.truncated && (c = ::fgetc(file.get())) != EOF) {
            if (c != '\n') { line.push_back(static_cast<char>(c)); continue; }
            match_line(state, path, ++number, line, *query);
            line.clear();
        }
        if (!state.truncated && !line.empty()) match_line(state, path, number + 1, line, *query);
    }
}
} // namespace

std::string search_workspace(const std::filesystem::path& root, const std::string& query)
{
    Fd directory(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (directory.get() < 0) throw std::runtime_error("failed to open workspace directory");
    State state;
    walk(directory.get(), "", &query, state);
    if (state.truncated) state.output << "[truncated: reached 50 search results]\n";
    return state.output.str();
}

std::string list_workspace(const std::filesystem::path& root)
{
    Fd directory(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (directory.get() < 0) throw std::runtime_error("failed to open workspace directory");
    State state;
    walk(directory.get(), "", nullptr, state);
    return state.output.str();
}
