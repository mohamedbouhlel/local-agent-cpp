#include "agent.hpp"
#include "test_support.hpp"
#include "tool_registry.hpp"
#include "ui/tui_navigation.hpp"
#include "ui/tui_view.hpp"
#include "ui/tui_view_data.hpp"
#include "mock_http_server.hpp"

#include <pty.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <algorithm>
#include <clocale>
#include <filesystem>
#include <poll.h>
#include <thread>
#include <signal.h>

namespace {
std::string make_delta(std::string text)
{
    nlohmann::json value{{"choices", nlohmann::json::array({{{"index", 0}, {"delta", {{"content", std::move(text)}}}}})}};
    return "data: " + value.dump() + "\n\n";
}
}
TEST(tui4a, function_keys_select_all_six_views)
{
    agent::ui::TuiNavigation navigation;
    for (int key = 1; key <= 6; ++key) {
        CHECK(navigation.select_function_key(key));
        CHECK(static_cast<int>(navigation.page()) == key - 1);
    }
    CHECK(!navigation.select_function_key(0));
    CHECK(!navigation.select_function_key(7));
}
TEST(tui4a, escape_returns_to_conversation_and_preserves_its_focus)
{
    agent::ui::TuiNavigation navigation;
    navigation.cycle_focus();
    CHECK(navigation.focus() == agent::ui::TuiFocus::menu);
    navigation.select(agent::ui::TuiPage::help);
    navigation.escape_to_conversation();
    CHECK(navigation.page() == agent::ui::TuiPage::conversation);
    CHECK(navigation.focus() == agent::ui::TuiFocus::menu);
}

TEST(tui4a, tab_cycles_focus_and_remembers_focus_per_view)
{
    agent::ui::TuiNavigation navigation;
    CHECK(navigation.focus() == agent::ui::TuiFocus::composer);
    navigation.cycle_focus();
    CHECK(navigation.focus() == agent::ui::TuiFocus::menu);
    navigation.cycle_focus();
    CHECK(navigation.focus() == agent::ui::TuiFocus::content);
    navigation.select(agent::ui::TuiPage::tools);
    CHECK(navigation.focus() == agent::ui::TuiFocus::menu);
    navigation.cycle_focus();
    CHECK(navigation.focus() == agent::ui::TuiFocus::content);
    navigation.select(agent::ui::TuiPage::conversation);
    CHECK(navigation.focus() == agent::ui::TuiFocus::content);
}

TEST(tui4a, arrows_move_menu_selection_only_when_menu_has_focus)
{
    agent::ui::TuiNavigation navigation;
    CHECK(!navigation.move_selection(1));
    navigation.cycle_focus();
    CHECK(navigation.move_selection(1));
    CHECK(navigation.page() == agent::ui::TuiPage::tools);
    CHECK(navigation.move_selection(-1));
    CHECK(navigation.page() == agent::ui::TuiPage::conversation);
}

TEST(tui4a, submit_is_available_only_in_conversation)
{
    agent::ui::TuiNavigation navigation;
    CHECK(navigation.can_submit());
    navigation.select(agent::ui::TuiPage::configuration);
    CHECK(!navigation.can_submit());
}

TEST(tui4a, every_page_has_a_compact_visible_projection)
{
    agent::ui::TuiView view;
    for (int page = 0; page < 6; ++page) {
        const auto rows = view.rendered_page_rows(static_cast<agent::ui::TuiPage>(page), 10);
        CHECK(!rows.empty());
        CHECK(std::string(agent::ui::TuiNavigation::page_name(static_cast<agent::ui::TuiPage>(page))).size() > 0);
    }
    const auto metrics = view.rendered_page_rows(agent::ui::TuiPage::metrics, 120);
    CHECK(std::any_of(metrics.begin(), metrics.end(), [](const std::string& row) {
        return row.find("Unavailable: no turn metrics observed yet.") != std::string::npos;
    }));
}

TEST(tui4a, configuration_projection_redacts_endpoint_credentials_and_api_key)
{
    ToolRegistry tools;
    AgentConfig config{"https://user:password@example.test:8443/v1/chat?token=secret", "test-model", "api-secret", 17};
    const auto data = agent::ui::make_tui_view_data(config, tools);
    agent::ui::TuiView view;
    view.set_view_data(data);
    const auto rows = view.rendered_page_rows(agent::ui::TuiPage::configuration, 120);
    const auto rendered = rows.empty() ? std::string{} : rows.front() + (rows.size() > 1 ? rows[1] : "");
    CHECK(data.endpoint_origin == "https://example.test:8443");
    CHECK(data.api_key_configured);
    bool has_password = false;
    bool has_api_secret = false;
    bool has_token = false;
    for (const auto& row : rows) {
        has_password = has_password || row.find("password") != std::string::npos;
        has_api_secret = has_api_secret || row.find("api-secret") != std::string::npos;
        has_token = has_token || row.find("secret") != std::string::npos;
    }
    CHECK(!has_password);
    CHECK(!has_api_secret);
    CHECK(!has_token);
    CHECK(rendered.find("test-model") != std::string::npos);
}

TEST(tui4a, malformed_endpoint_is_hidden)
{
    ToolRegistry tools;
    AgentConfig config{"not a URL?password=secret", "m", "", 60};
    const auto data = agent::ui::make_tui_view_data(config, tools);
    CHECK(data.endpoint_origin == "configured (details hidden)");
}

TEST(tui4a, tools_view_lists_declared_tools_without_arguments_or_results)
{
    ToolRegistry tools;
    tools.register_tool({"read_file", "Read a workspace file", nlohmann::json::object(),
                         [](nlohmann::json) { return "private-result-sentinel"; }});
    AgentConfig config{"http://localhost:8080/v1/chat/completions", "m", "", 60};
    agent::ui::TuiView view;
    view.set_view_data(agent::ui::make_tui_view_data(config, tools));
    view.add_tool_activity("Succeeded: read_file");
    const auto rows = view.rendered_page_rows(agent::ui::TuiPage::tools, 100);
    std::string joined;
    for (const auto& row : rows) joined += row + '\n';
    CHECK(joined.find("read_file") != std::string::npos);
    CHECK(joined.find("Read a workspace file") != std::string::npos);
    CHECK(joined.find("private-result-sentinel") == std::string::npos);
    CHECK(joined.find("arguments") == std::string::npos);
}

TEST(tui4a, diagnostics_projection_contains_only_observed_entries)
{
    agent::ui::TuiView view;
    CHECK(view.diagnostics().empty());
    CHECK(view.rendered_page_rows(agent::ui::TuiPage::diagnostics, 80).back()
          == "No diagnostic event observed in this session.");
    view.add_diagnostic("Request failed: HTTP.");
    view.add_diagnostic("hostile\x1b]0;title\x07 text");
    CHECK_EQ(view.diagnostics().size(), std::size_t(2));
    CHECK(view.diagnostics().front() == "Request failed: HTTP.");
    CHECK(view.diagnostics().back().find("\x1b") == std::string::npos);
}

TEST(tui4a, help_projection_matches_current_key_contract)
{
    agent::ui::TuiView view;
    const auto rows = view.rendered_page_rows(agent::ui::TuiPage::help, 120);
    std::string joined;
    for (const auto& row : rows) joined += row + '\n';
    CHECK(joined.find("F1-F6") != std::string::npos);
    CHECK(joined.find("Escape") != std::string::npos);
    CHECK(joined.find("F9") != std::string::npos);
    CHECK(joined.find("F10") != std::string::npos);
    CHECK(joined.find("Tab") != std::string::npos);
    CHECK(joined.find("Enter") != std::string::npos);
    CHECK(joined.find("PgUp/PgDn") != std::string::npos);
}

TEST(tui4a, conversation_transcript_and_anchor_survive_page_changes_and_resize)
{
    agent::ui::TuiView view;
    agent::ui::TuiNavigation navigation;
    view.on_resize(40, 5);
    for (int i = 0; i < 12; ++i) view.add_notice("row" + std::to_string(i));
    view.scroll_page_up(agent::ui::TuiPage::conversation);
    const auto prior_offset = view.scroll_offset();
    view.append_assistant_delta("new response line\n");
    const auto prior_message_count = view.messages().size();
    navigation.select(agent::ui::TuiPage::tools);
    view.on_resize(10, 2);
    navigation.select(agent::ui::TuiPage::conversation);
    CHECK_EQ(view.messages().size(), prior_message_count);
    CHECK(!view.follows_bottom());
    CHECK(view.scroll_offset() >= prior_offset);
    bool response_present = false;
    for (const auto& row : view.rendered_page_rows(navigation.page(), 80)) {
        response_present = response_present || row.find("new response line") != std::string::npos;
    }
    CHECK(response_present);
}

TEST(tui4a, utf8_accents_combining_and_wide_characters_survive_wrap_and_resize)
{
    const char* locale = std::setlocale(LC_CTYPE, "C.UTF-8");
    if (locale == nullptr) locale = std::setlocale(LC_CTYPE, "en_US.UTF-8");
    CHECK(locale != nullptr);
    if (locale == nullptr) return;

    const std::string text = "café 漢字 e\xCC\x81 👩‍💻";
    agent::ui::TuiView view;
    view.append_assistant_delta(text);
    const auto narrow = view.rendered_conversation_rows(10);
    std::string reconstructed;
    for (std::size_t i = 0; i < narrow.size(); ++i) {
        const auto& row = narrow[i];
        if (i == 0) {
            CHECK(row.starts_with("Agent: "));
            reconstructed += row.substr(7);
        } else {
            CHECK(row.starts_with("       "));
            reconstructed += row.substr(7);
        }
    }
    CHECK_EQ(reconstructed, text);

    const auto wide = view.rendered_conversation_rows(80);
    CHECK_EQ(wide.size(), std::size_t(1));
    CHECK_EQ(wide.front(), "Agent: " + text);
    view.on_resize(10, 2);
    view.on_resize(80, 18);
    const auto resized = view.rendered_conversation_rows(80);
    CHECK_EQ(resized.front(), "Agent: " + text);
}

TEST(tui4a, panel_scroll_position_is_kept_per_view)
{
    agent::ui::TuiView view;
    for (int i = 0; i < 20; ++i) view.add_diagnostic("diagnostic " + std::to_string(i));
    view.on_resize(40, 5);
    view.scroll_page_up(agent::ui::TuiPage::diagnostics);
    view.scroll_page_up(agent::ui::TuiPage::diagnostics);
    const auto diagnostics_offset = view.page_scroll_offset(agent::ui::TuiPage::diagnostics);
    view.scroll_page_down(agent::ui::TuiPage::tools);
    view.scroll_page_up(agent::ui::TuiPage::tools);
    CHECK(!view.diagnostics().empty());
    CHECK_EQ(view.page_scroll_offset(agent::ui::TuiPage::diagnostics), diagnostics_offset);
}

TEST(tui4a, pty_navigation_preserves_draft_and_receives_reply_off_conversation)
{
    MockHttpServer server;
    MockHttpServer::Response response;
    response.body = make_delta("réponse 漢字 👩‍💻 while viewing Tools") + "data: [DONE]\n\n";
    response.piece_size = 6;
    response.piece_delay_us = 80000;
    server.enqueue(std::move(response));

    int master = -1;
    winsize size{}; size.ws_row = 24; size.ws_col = 80;
    const pid_t child = ::forkpty(&master, nullptr, nullptr, &size);
    CHECK(child >= 0);
    if (child == 0) {
        const auto driver = std::filesystem::read_symlink("/proc/self/exe").parent_path() / "tui_pty_driver";
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("LC_ALL", "C.UTF-8", 1);
        ::execl(driver.c_str(), driver.c_str(), server.url("/v1/chat/completions").c_str(), "normal", nullptr);
        _exit(127);
    }

    auto send = [master](std::string_view bytes) {
        static_cast<void>(::write(master, bytes.data(), bytes.size()));
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    };
    auto read_for = [master](std::string& output, std::chrono::milliseconds duration) {
        const auto deadline = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < deadline) {
            pollfd descriptor{master, POLLIN, 0};
            if (::poll(&descriptor, 1, 20) > 0 && (descriptor.revents & POLLIN)) {
                char buffer[2048];
                const auto count = ::read(master, buffer, sizeof(buffer));
                if (count > 0) output.append(buffer, static_cast<std::size_t>(count));
            }
        }
    };

    std::string output;
    read_for(output, std::chrono::milliseconds(200));
    send("line1\r \rline3");
    send("\033OD"); // xterm application-mode left arrow
    send("\033OQ"); // F2 Tools
    send("\033[20~"); // F9 must not submit outside Conversation
    CHECK(server.received_bodies().empty());
    send("\033OP"); // F1 Conversation
    send("X");
    send("\033[20~"); // submit exactly once
    send("\033OQ"); // continue navigating while Agent::send is active
    read_for(output, std::chrono::milliseconds(750));
    send("\033OR"); // F3 Metrics while streaming
    read_for(output, std::chrono::milliseconds(100));
    CHECK(output.find("Text fragments:") != std::string::npos);
    send("\033OP"); // return to Conversation without affecting the active turn
    size.ws_row = 4; size.ws_col = 10;
    static_cast<void>(::ioctl(master, TIOCSWINSZ, &size));
    static_cast<void>(::kill(child, SIGWINCH));
    read_for(output, std::chrono::milliseconds(150));
    size.ws_row = 24; size.ws_col = 80;
    static_cast<void>(::ioctl(master, TIOCSWINSZ, &size));
    static_cast<void>(::kill(child, SIGWINCH));
    read_for(output, std::chrono::milliseconds(100));
    send("\033OP"); // return after the response has arrived off-view
    read_for(output, std::chrono::milliseconds(100));
    send("\033[21~"); // F10

    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (std::chrono::steady_clock::now() < deadline && ::waitpid(child, &status, WNOHANG) == 0) {
        read_for(output, std::chrono::milliseconds(25));
    }
    read_for(output, std::chrono::milliseconds(100));
    if (::waitpid(child, &status, WNOHANG) == 0) {
        static_cast<void>(::kill(child, SIGKILL));
        static_cast<void>(::waitpid(child, &status, 0));
    }
    ::close(master);
    CHECK(WIFEXITED(status));
    CHECK_EQ(WEXITSTATUS(status), 0);
    const auto requests = server.received_bodies();
    CHECK_EQ(requests.size(), std::size_t(1));
    if (!requests.empty()) {
        const auto payload = nlohmann::json::parse(requests.front());
        CHECK_EQ(payload.at("messages").at(0).at("content").get<std::string>(), std::string("line1\n \nlineX3"));
    }
    CHECK(output.find("Tools") != std::string::npos);
    CHECK(output.find("réponse 漢字 👩‍💻 while viewing Tools") != std::string::npos);
    CHECK(output.find("\033[?1049l") != std::string::npos);
}
