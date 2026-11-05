#include "filesystem_sandbox.hpp"
#include "test_support.hpp"

#include <cerrno>
#include <filesystem>
#include <fstream>

#ifdef __linux__
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

TEST(sandbox, denies_filesystem_access_outside_workspace)
{
    tests::TempDir root("landlock");
    const auto workspace = root.path() / "workspace";
    const auto inside = workspace / "inside.txt";
    const auto outside = root.path() / "outside.txt";
    std::filesystem::create_directories(workspace);
    { std::ofstream file(inside); file << "inside"; }
    { std::ofstream file(outside); file << "outside"; }

    const pid_t child = ::fork();
    CHECK(child >= 0);
    if (child < 0) return;
    if (child == 0) {
        try {
            apply_filesystem_sandbox(workspace);
        } catch (...) {
            ::_exit(10);
        }
        const int denied_read = ::open(outside.c_str(), O_RDONLY | O_CLOEXEC);
        if (denied_read >= 0) {
            ::close(denied_read);
            ::_exit(11);
        }
        if (errno != EACCES && errno != EPERM) ::_exit(12);

        const int allowed_read = ::open(inside.c_str(), O_RDONLY | O_CLOEXEC);
        if (allowed_read < 0) ::_exit(13);
        ::close(allowed_read);

        const int denied_write = ::open(outside.c_str(), O_WRONLY | O_TRUNC | O_CLOEXEC);
        if (denied_write >= 0) {
            ::close(denied_write);
            ::_exit(14);
        }
        if (errno != EACCES && errno != EPERM) ::_exit(15);

        const int allowed_write = ::open((workspace / "created.txt").c_str(),
                                         O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (allowed_write < 0) ::_exit(16);
        ::close(allowed_write);

        const int denied_system_tree = ::open("/usr", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (denied_system_tree >= 0) {
            ::close(denied_system_tree);
            ::_exit(17);
        }
        if (errno != EACCES && errno != EPERM) ::_exit(18);
        ::_exit(0);
    }

    int status = 0;
    CHECK_EQ(::waitpid(child, &status, 0), child);
    CHECK(WIFEXITED(status));
    CHECK_EQ(WEXITSTATUS(status), 0);
}

TEST(sandbox, refuses_symlink_workspace_root)
{
    tests::TempDir root("landlock_symlink_root");
    const auto target = root.path() / "target";
    const auto link = root.path() / "workspace-link";
    std::filesystem::create_directories(target);
    std::error_code error;
    std::filesystem::create_directory_symlink(target, link, error);
    if (error) {
        std::cout << "    (SKIP: creation de lien symbolique impossible: " << error.message() << ")\n";
        return;
    }

    CHECK_THROWS_MSG(apply_filesystem_sandbox(link), "workspace root must be a real directory");
}

TEST(sandbox, preparation_rejects_dangling_symlink_without_creating_target)
{
    tests::TempDir root("landlock_dangling_symlink");
    const auto target = root.path() / "outside-target";
    const auto link = root.path() / "workspace-link";
    std::error_code error;
    std::filesystem::create_directory_symlink(target, link, error);
    if (error) {
        std::cout << "    (SKIP: creation de lien symbolique impossible: " << error.message() << ")\n";
        return;
    }

    CHECK_THROWS_MSG(prepare_workspace_root(link), "workspace root must be a real directory");
    CHECK(!std::filesystem::exists(target));
}

TEST(sandbox, preparation_creates_missing_workspace_directory)
{
    tests::TempDir root("landlock_create_workspace");
    const auto workspace = root.path() / "workspace";
    CHECK_EQ(prepare_workspace_root(workspace), workspace);
    CHECK(std::filesystem::is_directory(workspace));
}
#endif
