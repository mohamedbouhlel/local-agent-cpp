#include "workspace_tools.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t max_read_chars = 50'000;
constexpr std::size_t max_write_chars = 200'000;
constexpr std::size_t binary_probe_bytes = 4096;
constexpr std::size_t max_search_results = 50;

std::filesystem::path canonical_workspace_root(const std::filesystem::path& workspace_root)
{
    auto root = std::filesystem::weakly_canonical(std::filesystem::absolute(workspace_root));
    std::filesystem::create_directories(root);
    root = std::filesystem::canonical(root);
    return root;
}

bool is_hidden_path(const std::filesystem::path& relative_path)
{
    return std::any_of(relative_path.begin(), relative_path.end(), [](const auto& part) {
        const auto name = part.string();
        return !name.empty() && name.front() == '.';
    });
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

bool appears_binary(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return true;
    }

    std::string buffer;
    buffer.resize(binary_probe_bytes);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    buffer.resize(static_cast<std::size_t>(file.gcount()));

    return std::find(buffer.begin(), buffer.end(), '\0') != buffer.end();
}

std::string list_files(const std::filesystem::path& root)
{
    std::ostringstream output;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const auto relative = relative_to_workspace(root, entry.path());
        if (is_hidden_path(relative)) {
            if (entry.is_directory()) {
                continue;
            }
            continue;
        }
        if (entry.is_regular_file()) {
            output << relative.generic_string() << '\n';
        }
    }
    return output.str();
}

std::string read_file(const std::filesystem::path& root, const nlohmann::json& args)
{
    const auto path = sandbox_path(root, require_string_arg(args, "path"));
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("file does not exist");
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("path is not a regular file");
    }
    if (appears_binary(path)) {
        throw std::runtime_error("cannot read binary file: " + relative_to_workspace(root, path).generic_string());
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("failed to open file for reading");
    }

    std::string content;
    content.resize(max_read_chars + 1);
    file.read(content.data(), static_cast<std::streamsize>(content.size()));
    const auto bytes_read = static_cast<std::size_t>(file.gcount());
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

    std::ostringstream output;
    std::size_t results = 0;
    bool truncated = false;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const auto relative = relative_to_workspace(root, entry.path());
        if (is_hidden_path(relative) || !entry.is_regular_file() || appears_binary(entry.path())) {
            continue;
        }

        std::ifstream file(entry.path());
        if (!file) {
            continue;
        }

        std::string line;
        std::size_t line_number = 0;
        while (std::getline(file, line)) {
            ++line_number;
            if (line.find(query) == std::string::npos) {
                continue;
            }

            if (results >= max_search_results) {
                truncated = true;
                break;
            }

            output << relative.generic_string() << ':' << line_number << ':' << line << '\n';
            ++results;
        }

        if (truncated) {
            break;
        }
    }

    if (truncated) {
        output << "[truncated: reached 50 search results]\n";
    }

    return output.str();
}

std::string write_file(const std::filesystem::path& root, const nlohmann::json& args)
{
    const auto path = sandbox_path(root, require_string_arg(args, "path"));
    const auto content = require_string_arg(args, "content");
    const auto overwrite = optional_bool_arg(args, "overwrite", false);
    const auto relative = relative_to_workspace(root, path).generic_string();

    if (content.size() > max_write_chars) {
        throw std::runtime_error(
            "content is " + std::to_string(content.size())
            + " characters, exceeding the maximum write size of "
            + std::to_string(max_write_chars)
            + "; write the file in smaller parts instead");
    }

    if (std::filesystem::is_directory(path)) {
        throw std::runtime_error("cannot write file: path is an existing directory: " + relative);
    }

    const bool exists = std::filesystem::exists(path);
    if (exists && !overwrite) {
        throw std::runtime_error(
            "file already exists: " + relative + " (pass overwrite: true to replace it)");
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        throw std::runtime_error("failed to create parent directories: " + error.message());
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("failed to open file for writing");
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) {
        throw std::runtime_error("failed to write file content");
    }

    file.close();
    if (!file) {
        throw std::runtime_error("failed to flush file content");
    }

    return "wrote " + std::to_string(content.size()) + " bytes to " + relative
        + (exists ? " (overwrote existing file)" : " (created new file)");
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
