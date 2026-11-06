#include "mock_http_server.hpp"
#include "test_support.hpp"
#include "ui/tui_view.hpp"
#include "ui/terminal_session.hpp"
#include "ui/ui_options.hpp"

#include <pty.h>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <poll.h>
#include <string>
#include <thread>
#include <vector>

namespace {
std::string sse_delta(std::string text)
{
    nlohmann::json value{{"choices", nlohmann::json::array({{{"index", 0}, {"delta", {{"content", std::move(text)}}}}})}};
    return "data: " + value.dump() + "\n\n";
}
}

TEST(tui3, explicit_ui_mode_is_backward_compatible)
{
    char a0[] = "agent", a1[] = "--legacy";
    char* classic[] = {a0, a1};
    CHECK(agent::ui::parse_interface_mode(2, classic) == agent::ui::InterfaceMode::classic);
    char b0[] = "agent", b1[] = "--ui=tui";
    char* tui[] = {b0, b1};
    CHECK(agent::ui::parse_interface_mode(2, tui) == agent::ui::InterfaceMode::tui);
}

TEST(tui3, terminal_session_refuses_non_tty)
{
    const pid_t child = ::fork();
    CHECK(child >= 0);
    if (child == 0) {
        const int null_fd = ::open("/dev/null", O_RDWR);
        if (null_fd < 0) _exit(2);
        (void)::dup2(null_fd, STDIN_FILENO);
        (void)::dup2(null_fd, STDOUT_FILENO);
        try { agent::ui::TerminalSession terminal; }
        catch (const std::exception& error) { _exit(std::string(error.what()).find("terminals") != std::string::npos ? 0 : 3); }
        _exit(4);
    }
    int status = 0;
    (void)::waitpid(child, &status, 0);
    CHECK(WIFEXITED(status));
    CHECK_EQ(WEXITSTATUS(status), 0);
}

TEST(tui3, terminal_session_restores_after_controlled_exception_on_pty)
{
    int master = -1;
    winsize size{}; size.ws_row = 24; size.ws_col = 80;
    const pid_t child = ::forkpty(&master, nullptr, nullptr, &size);
    CHECK(child >= 0);
    if (child == 0) {
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("LC_ALL", "C.UTF-8", 1);
        try {
            agent::ui::TerminalSession terminal;
            throw std::runtime_error("controlled renderer exception");
        } catch (const std::runtime_error&) { _exit(0); }
        _exit(2);
    }
    std::string output;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    int status = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd descriptor{master, POLLIN, 0};
        if (::poll(&descriptor, 1, 25) > 0 && (descriptor.revents & POLLIN)) {
            char bytes[1024]; const auto count = ::read(master, bytes, sizeof(bytes));
            if (count > 0) output.append(bytes, static_cast<std::size_t>(count));
        }
        if (::waitpid(child, &status, WNOHANG) == child) break;
    }
    if (::waitpid(child, &status, WNOHANG) == 0) { (void)::kill(child, SIGKILL); (void)::waitpid(child, &status, 0); }
    ::close(master);
    CHECK(WIFEXITED(status));
    CHECK_EQ(WEXITSTATUS(status), 0);
    CHECK(output.find("\033[?1049l") != std::string::npos);
}

TEST(tui3, viewport_stays_anchored_and_returns_to_follow_mode)
{
    agent::ui::TuiView view;
    view.on_resize(40, 5);
    for (int i = 0; i < 12; ++i) view.add_notice("row" + std::to_string(i));
    view.scroll_up();
    const auto offset = view.scroll_offset();
    view.append_assistant_delta("new line\n");
    CHECK(!view.follows_bottom());
    CHECK(view.scroll_offset() >= offset);
    view.scroll_down();
    view.scroll_down();
    CHECK(view.follows_bottom());
}

TEST(tui3, replaces_partial_response_instead_of_appending)
{
    agent::ui::TuiView view;
    view.append_assistant_delta("partial");
    view.replace_assistant_response("complete");
    CHECK_EQ(view.messages().back().text, std::string("complete"));
    CHECK(!view.messages().back().incomplete);
}

TEST(tui3, real_agent_streams_through_fullscreen_pty_and_restores_terminal)
{
    MockHttpServer server;
    MockHttpServer::Response response;
    response.body = sse_delta("answer ") + sse_delta("safe\x1b]0;BAD_TITLE\x07text")
        + sse_delta("tail") + "data: [DONE]\n\n";
    response.piece_size = 5;
    response.piece_delay_us = 50000;
    server.enqueue(std::move(response));

    int master = -1;
    winsize size{}; size.ws_row = 24; size.ws_col = 80;
    const pid_t child = ::forkpty(&master, nullptr, nullptr, &size);
    CHECK(child >= 0);
    if (child == 0) {
        const auto executable = std::filesystem::read_symlink("/proc/self/exe").parent_path() / "tui_pty_driver";
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("LC_ALL", "C.UTF-8", 1);
        ::execl(executable.c_str(), executable.c_str(), server.url("/v1/chat/completions").c_str(), nullptr);
        _exit(127);
    }

    std::string output;
    auto write_all = [master](std::string_view value) { (void)::write(master, value.data(), value.size()); };
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    write_all("hello\nworld");
    write_all("\033[20~"); // xterm F9
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    write_all("\033[21~"); // request close while the HTTP/SSE turn is active
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool sent_quit = false;
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd descriptor{master, POLLIN, 0};
        if (::poll(&descriptor, 1, 50) > 0 && (descriptor.revents & POLLIN)) {
            char buffer[4096];
            const auto count = ::read(master, buffer, sizeof(buffer));
            if (count > 0) output.append(buffer, static_cast<std::size_t>(count));
        }
        if (!sent_quit && output.find("answer") != std::string::npos) {
            size.ws_row = 4; size.ws_col = 10;
            (void)::ioctl(master, TIOCSWINSZ, &size);
            (void)::kill(child, SIGWINCH);
            sent_quit = true;
        }
        int status = 0;
        if (::waitpid(child, &status, WNOHANG) == child) {
            ::close(master);
            CHECK(WIFEXITED(status));
            CHECK_EQ(WEXITSTATUS(status), 0);
            CHECK(sent_quit);
            CHECK(!output.empty());
            CHECK(output.find("tail") != std::string::npos); // exact final response recovered after dropped deltas
            CHECK(output.find("\033[?1049l") != std::string::npos);
            CHECK(output.find("Waiting for active request/tool") != std::string::npos);
            CHECK(output.find("event details were incomplete") != std::string::npos);
            CHECK(output.find("BAD_TITLE") == std::string::npos);
            CHECK_EQ(server.received_bodies().size(), std::size_t(1));
            const auto payload = nlohmann::json::parse(server.received_bodies().front());
            CHECK_EQ(payload.at("messages").at(0).at("content").get<std::string>(), std::string("hello\nworld"));
            return;
        }
    }
    (void)::kill(child, SIGKILL);
    int status = 0; (void)::waitpid(child, &status, 0); ::close(master);
    CHECK(false);
}

TEST(tui3, http_failure_is_rendered_as_failure_in_pty)
{
    MockHttpServer server;
    server.enqueue({500, "mock HTTP failure", 0, 0, false, false});
    int master = -1;
    winsize size{}; size.ws_row = 24; size.ws_col = 80;
    const pid_t child = ::forkpty(&master, nullptr, nullptr, &size);
    CHECK(child >= 0);
    if (child == 0) {
        const auto executable = std::filesystem::read_symlink("/proc/self/exe").parent_path() / "tui_pty_driver";
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("LC_ALL", "C.UTF-8", 1);
        ::execl(executable.c_str(), executable.c_str(), server.url("/v1/chat/completions").c_str(), nullptr);
        _exit(127);
    }
    std::string output;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    (void)::write(master, "fail", 4);
    (void)::write(master, "\033[20~", 5);
    bool sent_quit = false;
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd descriptor{master, POLLIN, 0};
        if (::poll(&descriptor, 1, 50) > 0 && (descriptor.revents & POLLIN)) {
            char bytes[4096]; const auto count = ::read(master, bytes, sizeof(bytes));
            if (count > 0) output.append(bytes, static_cast<std::size_t>(count));
        }
        if (!sent_quit && output.find("HTTP error:") != std::string::npos) {
            (void)::write(master, "\033[21~", 5);
            sent_quit = true;
        }
        if (::waitpid(child, &status, WNOHANG) == child) {
            ::close(master);
            CHECK(WIFEXITED(status));
            CHECK_EQ(WEXITSTATUS(status), 0);
            CHECK(sent_quit);
            CHECK(output.find("HTTP error:") != std::string::npos);
            CHECK_EQ(server.received_bodies().size(), std::size_t(1));
            return;
        }
    }
    (void)::kill(child, SIGKILL);
    (void)::waitpid(child, &status, 0);
    ::close(master);
    CHECK(false);
}
