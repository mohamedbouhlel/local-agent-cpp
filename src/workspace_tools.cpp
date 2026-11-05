#include "workspace_tools.hpp"
#include "workspace_search.hpp"
#include "workspace_write.hpp"

#include <nlohmann/json.hpp>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <cerrno>

namespace {

constexpr std::size_t max_read_chars = 50'000;
constexpr std::size_t max_write_chars = 200'000;
constexpr std::size_t binary_probe_bytes = 4096;

std::filesystem::path canonical_workspace_root(const std::filesystem::path& workspace_root)
{
    auto root = std::filesystem::weakly_canonical(std::filesystem::absolute(workspace_root));
    std::filesystem::create_directories(root);
    root = std::filesystem::canonical(root);
    return root;
}

bool is_inside_workspace(const std::filesystem::path& root, const std::filesystem::path& candidate)
{
    const auto [root_end, candidate_end] = std::mismatch(root.begin(), root.end(), candidate.begin(), candidate.end());
    return root_end == root.end() && candidate_end != candidate.end();
}

std::filesystem::path sandbox_path(const std::filesystem::path& root, const std::string& llm_path)
{
    if (llm_path.empty()) {
        throw std::invalid_argument("workspace path must not be empty");
    }
    if (llm_path.find('\0') != std::string::npos) {
        throw std::invalid_argument("workspace path must not contain NUL");
    }

    const std::filesystem::path relative_path(llm_path);
    if (relative_path.is_absolute()) {
        throw std::invalid_argument("absolute workspace paths are not allowed");
    }

    for (const auto& part : relative_path) {
        if (part == "..") {
            throw std::invalid_argument("workspace path must not contain '..'");
        }
    }

    const auto candidate = std::filesystem::weakly_canonical(root / relative_path);
    if (!is_inside_workspace(root, candidate)) {
        throw std::invalid_argument("workspace path escapes sandbox");
    }

    return candidate;
}

std::filesystem::path relative_to_workspace(const std::filesystem::path& root, const std::filesystem::path& path)
{
    return std::filesystem::relative(path, root);
}

std::string require_string_arg(const nlohmann::json& args, const char* name)
{
    if (!args.contains(name) || !args.at(name).is_string()) {
        throw std::invalid_argument(std::string("missing required string argument: ") + name);
    }
    return args.at(name).get<std::string>();
}

bool optional_bool_arg(const nlohmann::json& args, const char* name, bool default_value)
{
    if (!args.contains(name)) {
        return default_value;
    }
    if (!args.at(name).is_boolean()) {
        throw std::invalid_argument(std::string("argument must be a boolean: ") + name);
    }
    return args.at(name).get<bool>();
}

class ReadDescriptor {
public:
    explicit ReadDescriptor(int fd) : fd_(fd) {}
    ~ReadDescriptor() { if (fd_ >= 0) ::close(fd_); }
    ReadDescriptor(const ReadDescriptor&) = delete;
    ReadDescriptor& operator=(const ReadDescriptor&) = delete;
    int get() const { return fd_; }
    void replace(int fd) { if (fd_ >= 0) ::close(fd_); fd_ = fd; }
private:
    int fd_;
};

std::string list_files(const std::filesystem::path& root)
{
    return list_workspace(root);
}

std::string read_file(const std::filesystem::path& root, const nlohmann::json& args)
{
    const auto path = sandbox_path(root, require_string_arg(args, "path"));
    const auto relative = path.lexically_relative(root);
    ReadDescriptor file(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (file.get() < 0) throw std::runtime_error("failed to open workspace directory");
    for (auto part = relative.begin(); part != relative.end(); ++part) {
        auto next = part;
        ++next;
        const int flags = O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC
            | (next != relative.end() ? O_DIRECTORY : 0);
        const int child = ::openat(file.get(), part->c_str(), flags);
        if (child < 0) {
            if (errno == ENOENT) throw std::runtime_error("file does not exist");
            if (errno == ELOOP || errno == ENOTDIR) throw std::runtime_error("workspace path escapes sandbox or is not a directory");
            throw std::runtime_error("failed to open file for reading");
        }
        file.replace(child);
    }
    struct stat info {};
    if (::fstat(file.get(), &info) != 0 || !S_ISREG(info.st_mode)) {
        throw std::runtime_error("path is not a regular file");
    }
    if (info.st_nlink != 1) {
        throw std::runtime_error("refusing to read a file with multiple hard links");
    }
    std::string content(max_read_chars + 1, '\0');
    std::size_t bytes_read = 0;
    while (bytes_read < content.size()) {
        const auto count = ::read(file.get(), content.data() + bytes_read, content.size() - bytes_read);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw std::runtime_error("failed to read file");
        if (count == 0) break;
        bytes_read += static_cast<std::size_t>(count);
    }
    const auto probe_end = std::min(bytes_read, binary_probe_bytes);
    if (std::find(content.begin(), content.begin() + probe_end, '\0') != content.begin() + probe_end) {
        throw std::runtime_error("cannot read binary file: " + relative.generic_string());
    }
    content.resize(std::min(bytes_read, max_read_chars));
    if (bytes_read > max_read_chars) {
        content += "\n\n[truncated: file exceeds 50000 characters]\n";
    }

    return content;
}

std::string search_files(const std::filesystem::path& root, const nlohmann::json& args)
{
    const auto query = require_string_arg(args, "query");
    if (query.empty()) {
        throw std::invalid_argument("query must not be empty");
    }

    return search_workspace(root, query);
}

std::string write_file(const std::filesystem::path& root, const nlohmann::json& args)
{
    const auto path = sandbox_path(root, require_string_arg(args, "path"));
    const auto content = require_string_arg(args, "content");
    const auto overwrite = optional_bool_arg(args, "overwrite", false);
    if (content.size() > max_write_chars) {
        throw std::runtime_error(
            "content is " + std::to_string(content.size())
            + " characters, exceeding the maximum write size of "
            + std::to_string(max_write_chars)
            + "; write the file in smaller parts instead");
    }
    return write_workspace(root, path.lexically_relative(root), content, overwrite);
}

nlohmann::json object_schema(nlohmann::json properties, nlohmann::json required = nlohmann::json::array())
{
    return {
        {"type", "object"},
        {"properties", std::move(properties)},
        {"required", std::move(required)},
        {"additionalProperties", false},
    };
}

} // namespace

void register_workspace_tools(ToolRegistry& registry, std::filesystem::path workspace_root)
{
    const auto root = canonical_workspace_root(workspace_root);

    registry.register_tool({
        "list_files",
        "List all non-hidden files in the workspace recursively.",
        object_schema(nlohmann::json::object()),
        [root](nlohmann::json) {
            return list_files(root);
        },
    });

    registry.register_tool({
        "read_file",
        "Read a text file from the workspace by relative path.",
        object_schema(
            {{"path", {{"type", "string"}, {"description", "Relative path inside workspace."}}}},
            {"path"}),
        [root](nlohmann::json args) {
            return read_file(root, args);
        },
    });

    registry.register_tool({
        "search_files",
        "Search workspace text files for a literal query string.",
        object_schema(
            {{"query", {{"type", "string"}, {"description", "Literal string to search for."}}}},
            {"query"}),
        [root](nlohmann::json args) {
            return search_files(root, args);
        },
    });

    registry.register_tool({
        "write_file",
        "Write a text file inside the workspace, creating parent directories as needed.",
        object_schema(
            {
                {"path", {{"type", "string"}, {"description", "Relative path inside workspace."}}},
                {"content", {{"type", "string"}, {"description", "Full content to write to the file."}}},
                {"overwrite",
                 {{"type", "boolean"},
                  {"description",
                   "Set to true to replace a file that already exists. Defaults to false: "
                   "writing over an existing file is rejected until this is explicitly true."}}},
            },
            {"path", "content"}),
        [root](nlohmann::json args) {
            return write_file(root, args);
        },
    });
}
