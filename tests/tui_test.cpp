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
#include <fstream>
#include <poll.h>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace {
std::string sse_delta(std::string text)
{
    nlohmann::json value{{"choices", nlohmann::json::array({{{"index", 0}, {"delta", {{"content", std::move(text)}}}}})}};
    return "data: " + value.dump() + "\n\n";
}

std::string sse_write_call(std::string id, std::string path, std::string content)
{
    const nlohmann::json call{{"index", 0}, {"id", std::move(id)}, {"type", "function"},
        {"function", {{"name", "write_file"},
                      {"arguments", nlohmann::json{{"path", std::move(path)}, {"content", std::move(content)}}.dump()}}}};
    const nlohmann::json event{{"choices", nlohmann::json::array({{{"index", 0},
        {"delta", {{"tool_calls", nlohmann::json::array({call})}}}}})}};
    return "data: " + event.dump() + "\n\ndata: [DONE]\n\n";
}

struct TuiApprovalRun {
    int status = 0;
    bool exited = false;
    bool dialog_seen = false;
    std::string output;
};

std::string challenge_from_tui_output(const std::string& output)
{
    static const std::regex pattern(R"(Confirm: yes ([0-9a-f]{32}))");
    std::smatch match;
    return std::regex_search(output, match, pattern) ? match[1].str() : std::string{};
}

TuiApprovalRun run_tui_write_decision(MockHttpServer& server,
                                      const std::filesystem::path& working_directory,
                                      std::string_view decision,
                                      bool close_while_pending,
    bool compact_terminal = false,
    std::string_view prebuffered_key = {})
{
    TuiApprovalRun result;
    int master = -1;
    winsize size{}; size.ws_row = 24; size.ws_col = 80;
    const pid_t child = ::forkpty(&master, nullptr, nullptr, &size);
    if (child < 0) return result;
    if (child == 0) {
        (void)::chdir(working_directory.c_str());
        const auto executable = std::filesystem::read_symlink("/proc/self/exe").parent_path() / "tui_pty_driver";
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("LC_ALL", "C.UTF-8", 1);
        ::execl(executable.c_str(), executable.c_str(), server.url("/v1/chat/completions").c_str(), "normal", nullptr);
        _exit(127);
    }

    auto read_for = [&] (std::chrono::milliseconds duration) {
        const auto deadline = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < deadline) {
            pollfd descriptor{master, POLLIN, 0};
            if (::poll(&descriptor, 1, 20) > 0 && (descriptor.revents & POLLIN)) {
                char bytes[4096];
                const auto count = ::read(master, bytes, sizeof(bytes));
                if (count > 0) result.output.append(bytes, static_cast<std::size_t>(count));
            }
        }
    };
    auto write = [master](std::string_view bytes) {
        static_cast<void>(::write(master, bytes.data(), bytes.size()));
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    };

    read_for(std::chrono::milliseconds(200));
    write("create file");
    if (prebuffered_key.empty()) write("\033[20~"); // F9
    else write(std::string("\033[20~") + std::string(prebuffered_key));
    const auto dialog_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (std::chrono::steady_clock::now() < dialog_deadline
           && result.output.find("WRITE APPROVAL") == std::string::npos) {
        read_for(std::chrono::milliseconds(30));
    }
    result.dialog_seen = result.output.find("WRITE APPROVAL") != std::string::npos;
    if (result.dialog_seen) {
        if (compact_terminal) {
            size.ws_row = 4; size.ws_col = 10;
            (void)::ioctl(master, TIOCSWINSZ, &size);
            (void)::kill(child, SIGWINCH);
            read_for(std::chrono::milliseconds(120));
        }
        if (close_while_pending) write("\033[21~"); // F10 cancels pending approval and closes
        else if (decision == "approve") {
            const auto challenge = challenge_from_tui_output(result.output);
            if (!challenge.empty()) write(approval_confirmation(challenge) + "\n");
        } else if (decision == "wrong") {
            auto challenge = challenge_from_tui_output(result.output);
            if (!challenge.empty()) {
                challenge[0] = challenge[0] == '0' ? '1' : '0';
                write(approval_confirmation(challenge) + "\n");
            }
        } else if (decision == "y") write("y\n");
        else write(decision);
    }

    const auto exit_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
    while (std::chrono::steady_clock::now() < exit_deadline) {
        read_for(std::chrono::milliseconds(30));
        if (::waitpid(child, &result.status, WNOHANG) == child) { result.exited = true; break; }
        if (close_while_pending && server.received_bodies().size() >= 2) {
            // F10 already requested exit; wait for the agent to report the cancellation.
            continue;
        }
        if (!close_while_pending && server.received_bodies().size() >= 2) {
            write("\033[21~");
            close_while_pending = true; // only one close key is sent
        }
    }
    if (!result.exited) {
        static_cast<void>(::kill(child, SIGKILL));
        (void)::waitpid(child, &result.status, 0);
    }
    (void)::close(master);
    return result;
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
    view.append_assistant_delta("part");
    view.append_assistant_delta("ial\nfrag");
    view.replace_assistant_response("complete\nanswer");
    CHECK_EQ(view.messages().size(), std::size_t(1));
    CHECK_EQ(view.messages().back().text, std::string("complete\nanswer"));
    CHECK(!view.messages().back().incomplete);
    CHECK(view.rendered_conversation_rows(80)
          == std::vector<std::string>({"Agent: complete", "       answer"}));
}

TEST(tui3, multiline_messages_label_only_the_first_physical_line)
{
    agent::ui::TuiView view;
    view.add_user_message("first\n\n  third\n");
    view.append_assistant_delta("reply\nsecond");

    CHECK(view.rendered_conversation_rows(80) == std::vector<std::string>({
        "You: first", "     ", "       third", "     ",
        "Agent: reply", "       second",
    }));
    CHECK_EQ(view.messages().size(), std::size_t(2));
    CHECK_EQ(view.messages()[0].text, std::string("first\n\n  third\n"));
    CHECK_EQ(view.messages()[1].text, std::string("reply\nsecond"));
}

TEST(tui3, incomplete_multiline_message_preserves_blank_rows)
{
    agent::ui::TuiView view;
    view.append_assistant_delta("partial\n\n");
    view.finish_assistant(false);

    CHECK_EQ(view.messages().back().text, std::string("partial\n\n"));
    CHECK(view.rendered_conversation_rows(80)
          == std::vector<std::string>({"Agent: partial", "       ", "       ", "       [incomplete]"}));
}

TEST(tui3, sse_fragment_boundaries_do_not_change_multiline_rendering)
{
    agent::ui::TuiView view;
    view.append_assistant_delta("hel");
    view.append_assistant_delta("lo\n");
    view.append_assistant_delta("wo");
    view.append_assistant_delta("rld");

    CHECK_EQ(view.messages().size(), std::size_t(1));
    CHECK_EQ(view.messages().back().text, std::string("hello\nworld"));
    CHECK(view.rendered_conversation_rows(80)
          == std::vector<std::string>({"Agent: hello", "       world"}));
}

TEST(tui3, wrapped_continuations_are_indented_under_the_message_text)
{
    agent::ui::TuiView view;
    view.append_assistant_delta("abcdefghijklm");

    CHECK(view.rendered_conversation_rows(12)
          == std::vector<std::string>({"Agent: abc", "       def", "       ghi", "       jkl", "       m"}));
    CHECK_EQ(view.messages().back().text, std::string("abcdefghijklm"));
}

TEST(tui3, resize_during_streaming_reflows_without_changing_message_text)
{
    agent::ui::TuiView view;
    view.on_resize(40, 10);
    view.append_assistant_delta("alpha beta gamma");
    const auto before = view.messages().back().text;
    CHECK(view.rendered_conversation_rows(40) == std::vector<std::string>({"Agent: alpha beta gamma"}));

    view.on_resize(20, 4);
    view.append_assistant_delta(" delta");
    CHECK(view.rendered_conversation_rows(20)
          == std::vector<std::string>({"Agent: alpha beta ", "       gamma delta"}));
    CHECK_EQ(view.messages().back().text, before + " delta");
}

TEST(tui3, consecutive_turns_keep_role_and_message_boundaries)
{
    agent::ui::TuiView view;
    view.add_user_message("question one");
    view.append_assistant_delta("answer one");
    view.finish_assistant(true);
    view.add_user_message("question two");
    view.append_assistant_delta("answer two");

    CHECK(view.rendered_conversation_rows(80) == std::vector<std::string>({
        "You: question one", "Agent: answer one",
        "You: question two", "Agent: answer two",
    }));
    CHECK_EQ(view.messages().size(), std::size_t(4));
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

TEST(tui3, malformed_sse_is_rendered_as_protocol_failure_in_pty)
{
    MockHttpServer server;
    server.enqueue({200, sse_delta("provisional") + "data: {not-json}\n\ndata: [DONE]\n\n", 0, 0, false, false});
    int master = -1;
    winsize size{}; size.ws_row = 24; size.ws_col = 80;
    const pid_t child = ::forkpty(&master, nullptr, nullptr, &size);
    CHECK(child >= 0);
    if (child == 0) {
        const auto executable = std::filesystem::read_symlink("/proc/self/exe").parent_path() / "tui_pty_driver";
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("LC_ALL", "C.UTF-8", 1);
        ::execl(executable.c_str(), executable.c_str(), server.url("/v1/chat/completions").c_str(), "normal", nullptr);
        _exit(127);
    }
    std::string output;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    (void)::write(master, "request", 7);
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
        if (!sent_quit && output.find("Agent protocol error:") != std::string::npos) {
            (void)::write(master, "\033[21~", 5);
            sent_quit = true;
        }
        if (::waitpid(child, &status, WNOHANG) == child) {
            ::close(master);
            CHECK(WIFEXITED(status));
            CHECK_EQ(WEXITSTATUS(status), 0);
            CHECK(sent_quit);
            CHECK(output.find("provisional") != std::string::npos);
            CHECK(output.find("Agent protocol error:") != std::string::npos);
            CHECK(output.find("Turn completed technically") == std::string::npos);
            CHECK(output.find("not-json") == std::string::npos);
            CHECK_EQ(server.received_bodies().size(), std::size_t(1));
            return;
        }
    }
    (void)::kill(child, SIGKILL);
    (void)::waitpid(child, &status, 0);
    ::close(master);
    CHECK(false);
}

TEST(tui3, write_approval_dialog_creates_only_after_explicit_y)
{
    tests::TempDir working_directory("tui_write_approved");
    MockHttpServer server;
    MockHttpServer::Response call;
    call.body = sse_write_call("call-pty-yes", "nested/approved.txt", "approved body");
    server.enqueue(std::move(call));
    MockHttpServer::Response answer;
    answer.body = sse_delta("done") + "data: [DONE]\n\n";
    server.enqueue(std::move(answer));

    const auto run = run_tui_write_decision(server, working_directory.path(), "approve", false);
    CHECK(run.exited);
    CHECK(WIFEXITED(run.status));
    CHECK_EQ(WEXITSTATUS(run.status), 0);
    CHECK(run.dialog_seen);
    CHECK(run.output.find("approved body") != std::string::npos);
    const auto file = working_directory.path() / "workspace/nested/approved.txt";
    CHECK(std::filesystem::exists(file));
    if (std::filesystem::exists(file)) {
        std::ifstream in(file, std::ios::binary);
        CHECK_EQ(std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()),
                 std::string("approved body"));
    }
    const auto requests = server.received_bodies();
    CHECK_EQ(requests.size(), std::size_t(2));
}

TEST(tui3, closing_during_write_approval_cancels_without_creating_parent)
{
    constexpr int repetitions = 30;
    for (int iteration = 0; iteration < repetitions; ++iteration) {
        tests::TempDir working_directory("tui_write_cancelled");
        MockHttpServer server;
        MockHttpServer::Response call;
        call.body = sse_write_call("call-pty-close", "nested/cancelled.txt", "must not exist");
        server.enqueue(std::move(call));
        MockHttpServer::Response answer;
        answer.body = sse_delta("continued after cancellation") + "data: [DONE]\n\n";
        server.enqueue(std::move(answer));

        const auto run = run_tui_write_decision(server, working_directory.path(), {}, true);
        CHECK(run.exited);
        CHECK(WIFEXITED(run.status));
        CHECK_EQ(WEXITSTATUS(run.status), 0);
        CHECK(run.dialog_seen);
        CHECK(run.output.find("no file was created") != std::string::npos);
        CHECK(!std::filesystem::exists(working_directory.path() / "workspace/nested"));
        const auto requests = server.received_bodies();
        CHECK_EQ(requests.size(), std::size_t(2));
        if (requests.size() == 2) {
            const auto second = nlohmann::json::parse(requests[1]);
            bool found_cancelled = false;
            for (const auto& message : second.at("messages")) {
                if (message.value("role", "") == "tool") {
                    found_cancelled = message.value("content", "").find("cancelled") != std::string::npos;
                }
            }
            CHECK(found_cancelled);
        }
    }
}

TEST(tui3, single_y_and_prebuffered_y_do_not_approve_write)
{
    for (const bool prebuffer : {false, true}) {
        tests::TempDir working_directory("tui_write_stale_key");
        MockHttpServer server;
        MockHttpServer::Response call;
        call.body = sse_write_call("call-pty-stale-y", "nested/stale.txt", "must not be written");
        server.enqueue(std::move(call));
        MockHttpServer::Response answer;
        answer.body = sse_delta("write refused") + "data: [DONE]\n\n";
        server.enqueue(std::move(answer));

        const auto run = run_tui_write_decision(server, working_directory.path(), "y", false, false,
                                                prebuffer ? "y" : std::string_view{});
        CHECK(run.exited);
        CHECK(run.dialog_seen);
        CHECK(challenge_from_tui_output(run.output).size() == 32);
        CHECK(!std::filesystem::exists(working_directory.path() / "workspace/nested"));
        CHECK_EQ(server.received_bodies().size(), std::size_t(2));
    }
}

TEST(tui3, incorrect_current_challenge_does_not_create_file)
{
    tests::TempDir working_directory("tui_write_wrong_challenge");
    MockHttpServer server;
    MockHttpServer::Response call;
    call.body = sse_write_call("call-pty-wrong-challenge", "nested/wrong.txt", "must not be written");
    server.enqueue(std::move(call));
    MockHttpServer::Response answer;
    answer.body = sse_delta("write refused") + "data: [DONE]\n\n";
    server.enqueue(std::move(answer));

    const auto run = run_tui_write_decision(server, working_directory.path(), "wrong", false);
    CHECK(run.exited);
    CHECK(run.dialog_seen);
    CHECK(!std::filesystem::exists(working_directory.path() / "workspace/nested"));
    CHECK_EQ(server.received_bodies().size(), std::size_t(2));
}

TEST(tui3, compact_terminal_cannot_approve_uninspectable_write_content)
{
    tests::TempDir working_directory("tui_write_compact");
    MockHttpServer server;
    MockHttpServer::Response call;
    call.body = sse_write_call("call-pty-small", "nested/uninspectable.txt", "content requiring inspection");
    server.enqueue(std::move(call));
    MockHttpServer::Response answer;
    answer.body = sse_delta("continued without write") + "data: [DONE]\n\n";
    server.enqueue(std::move(answer));

    const auto run = run_tui_write_decision(server, working_directory.path(), "y", false, true);
    CHECK(run.exited);
    CHECK(WIFEXITED(run.status));
    CHECK_EQ(WEXITSTATUS(run.status), 0);
    CHECK(run.dialog_seen);
    CHECK(!std::filesystem::exists(working_directory.path() / "workspace/nested"));
    const auto requests = server.received_bodies();
    CHECK_EQ(requests.size(), std::size_t(2));
    if (requests.size() == 2) {
        const auto second = nlohmann::json::parse(requests[1]);
        bool found_unavailable = false;
        for (const auto& message : second.at("messages")) {
            if (message.value("role", "") == "tool") {
                found_unavailable = message.value("content", "").find("unavailable") != std::string::npos;
            }
        }
        CHECK(found_unavailable);
    }
}

TEST(tui3, approval_challenge_requires_visible_width_for_full_confirmation)
{
    CHECK(!agent::ui::TuiView::approval_inspectable(24, 47));
    CHECK(agent::ui::TuiView::approval_inspectable(24, 48));
    CHECK(!agent::ui::TuiView::approval_inspectable(5, 80));
    CHECK(agent::ui::TuiView::approval_inspectable(6, 48));
}
