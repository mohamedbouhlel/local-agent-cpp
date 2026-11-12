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

TEST(ux11, configuration_and_help_subsections_are_selectable_and_reversible)
{
    agent::ui::TuiNavigation navigation;
    navigation.select(agent::ui::TuiPage::configuration);
    CHECK_EQ(agent::ui::TuiNavigation::subsection_count(navigation.page()), 3);
    CHECK(navigation.select_subsection(2));
    CHECK_EQ(navigation.subsection_index(), 2);
    CHECK(navigation.activate_subsection());
    CHECK(navigation.subsection_open());
    CHECK(navigation.close_subsection());
    CHECK(!navigation.subsection_open());
    navigation.select(agent::ui::TuiPage::help);
    CHECK_EQ(agent::ui::TuiNavigation::subsection_count(navigation.page()), 3);
    CHECK(navigation.select_subsection(1));
    CHECK(navigation.activate_subsection());
    navigation.escape_to_conversation();
    CHECK(!navigation.subsection_open());
    navigation.select(agent::ui::TuiPage::help);
    CHECK(navigation.subsection_open());
    CHECK(navigation.close_subsection());
}

TEST(ux11, help_about_discloses_unavailable_version_instead_of_inventing_one)
{
    agent::ui::TuiView view;
    const auto rows = view.rendered_page_rows(agent::ui::TuiPage::help, 120, 2, true);
    CHECK(std::any_of(rows.begin(), rows.end(), [](const std::string& row) {
        return row.find("Version: unavailable") != std::string::npos;
    }));
    CHECK(std::any_of(rows.begin(), rows.end(), [](const std::string& row) {
        return row.find("Platform: Linux") != std::string::npos;
    }));
    CHECK(std::any_of(rows.begin(), rows.end(), [](const std::string& row) {
        return row.find("write_file requires explicit approval") != std::string::npos;
    }));
}

TEST(ux11, help_explains_native_text_selection_in_interactive_views)
{
    agent::ui::TuiView view;
    const auto rows = view.rendered_page_rows(agent::ui::TuiPage::help, 120, 1, true);
    CHECK(std::any_of(rows.begin(), rows.end(), [](const std::string& row) {
        return row.find("Sélectionner du texte : Maj + clic-glisser") != std::string::npos;
    }));
}

TEST(ux11, tab_hit_test_uses_the_active_tab_geometry)
{
    agent::ui::TuiView view;
    for (const int columns : {60, 80, 100, 120}) {
        for (const bool menu_focus : {false, true}) {
            for (const auto page : {agent::ui::TuiPage::conversation, agent::ui::TuiPage::help}) {
                std::array<bool, 6> seen{};
                for (int x = 0; x < columns; ++x) {
                    const int hit = view.hit_test_tab(x, columns, page, menu_focus);
                    if (hit >= 0) seen[static_cast<std::size_t>(hit)] = true;
                }
                CHECK(seen[1]);
                CHECK_EQ(view.hit_test_tab(menu_focus ? 16 : 11, columns, page, menu_focus), 1);
                CHECK_EQ(view.hit_test_tab(-1, columns, page, menu_focus), -1);
            }
        }
    }
    CHECK_EQ(view.hit_test_tab(2, 80, agent::ui::TuiPage::conversation), 0);
    CHECK_EQ(view.hit_test_tab(11, 80, agent::ui::TuiPage::conversation), 1);
    CHECK_EQ(view.hit_test_tab(-1, 80, agent::ui::TuiPage::help), -1);
}

TEST(ux11, content_hit_test_accepts_text_cells_and_ignores_headers_and_empty_rows)
{
    agent::ui::TuiView view;
    CHECK(!view.hit_test_content(0, 3, 24, 80, agent::ui::TuiPage::conversation, 0, false));
    CHECK(!view.hit_test_content(0, 4, 24, 80, agent::ui::TuiPage::conversation, 0, false));
    view.add_notice("visible message");
    CHECK(view.hit_test_content(0, 4, 24, 80, agent::ui::TuiPage::conversation, 0, false));
    CHECK(!view.hit_test_content(79, 4, 24, 80, agent::ui::TuiPage::conversation, 0, false));
    CHECK(!view.hit_test_content(0, 5, 24, 80, agent::ui::TuiPage::conversation, 0, false));
    CHECK(!view.hit_test_content(0, 20, 24, 80, agent::ui::TuiPage::conversation, 0, false));

    agent::ui::TuiViewData data;
    data.tools.push_back({"read_file", "Read a workspace file"});
    view.set_view_data(std::move(data));
    CHECK(view.hit_test_content(2, 4, 24, 80, agent::ui::TuiPage::tools, 0, false));
    CHECK(!view.hit_test_content(2, 19, 24, 80, agent::ui::TuiPage::tools, 0, false));
}

TEST(ux11, subsection_enter_and_click_share_open_switch_and_close_transitions)
{
    agent::ui::TuiNavigation by_enter;
    agent::ui::TuiNavigation by_click;
    for (auto* nav : {&by_enter, &by_click}) {
        nav->select(agent::ui::TuiPage::configuration);
        nav->set_focus(agent::ui::TuiFocus::menu);
    }
    CHECK(by_enter.select_subsection(0));
    CHECK(by_enter.activate_subsection());
    CHECK(by_click.select_subsection(0));
    CHECK(by_click.activate_subsection());
    CHECK_EQ(by_enter.subsection_open(), by_click.subsection_open());
    CHECK(by_enter.select_subsection(1));
    CHECK(by_enter.activate_subsection());
    CHECK(by_click.select_subsection(1));
    CHECK(by_click.activate_subsection());
    CHECK_EQ(by_enter.subsection_index(), by_click.subsection_index());
    CHECK_EQ(by_enter.subsection_open(), by_click.subsection_open());
    CHECK(by_enter.select_subsection(2));
    CHECK(by_click.select_subsection(2));
    CHECK(by_enter.subsection_open() == false);
    CHECK(by_enter.close_subsection());
    CHECK(by_click.close_subsection());
    CHECK_EQ(by_enter.subsection_open(), by_click.subsection_open());
    CHECK(by_enter.select_subsection(0));
    CHECK(by_click.select_subsection(0));
    CHECK(by_enter.activate_subsection());
    CHECK(by_click.activate_subsection());
    CHECK(by_enter.activate_subsection());
    CHECK(by_click.activate_subsection());
    CHECK_EQ(by_enter.subsection_open(), by_click.subsection_open());
}

TEST(ux11, markdown_projection_formats_safe_subset_and_keeps_source_immutable)
{
    agent::ui::TuiView view;
    const std::string source = "# Heading\n\n**bold** and `code` [docs](https://example.test)\n- item\n> quote\n";
    view.append_assistant_delta(source);
    const auto before = view.messages().back().text;
    const auto rows = view.rendered_styled_conversation_rows(100);
    CHECK(rows[0].kind == agent::ui::MarkdownLineKind::heading);
    CHECK(rows[2].kind == agent::ui::MarkdownLineKind::emphasis);
    CHECK(rows[2].text.find("bold and [code] docs <https://example.test>") != std::string::npos);
    CHECK(rows[3].kind == agent::ui::MarkdownLineKind::list);
    CHECK(rows[4].kind == agent::ui::MarkdownLineKind::quote);
    CHECK_EQ(view.messages().back().text, before);
    view.set_markdown_enabled(false);
    const auto plain = view.rendered_styled_conversation_rows(100);
    CHECK_EQ(plain[0].text, "LOFITERM Bot: # Heading");
    CHECK_EQ(plain[2].text, "              **bold** and `code` [docs](https://example.test)");
    CHECK_EQ(view.messages().back().text, before);
    const auto underscore_identifier = agent::ui::present_markdown("LLM_MODEL_name", true);
    CHECK(underscore_identifier.front().kind == agent::ui::MarkdownLineKind::paragraph);
    CHECK_EQ(underscore_identifier.front().text, "LLM_MODEL_name");
    const auto italic = agent::ui::present_markdown("_italic_", true);
    CHECK(italic.front().kind == agent::ui::MarkdownLineKind::emphasis);
    CHECK_EQ(italic.front().text, "italic");
}

TEST(ux11, markdown_code_fences_are_language_tagged_and_unfinished_content_stays_literal)
{
    const auto complete = agent::ui::present_markdown("```CPP\nint x = 1;\n```", true);
    CHECK_EQ(complete.size(), std::size_t(1));
    CHECK(complete[0].kind == agent::ui::MarkdownLineKind::code);
    CHECK_EQ(complete[0].language, "cpp");
    const auto unfinished = agent::ui::present_markdown("```python\nprint(1)\n", true);
    CHECK_EQ(unfinished.size(), std::size_t(3));
    CHECK(unfinished[0].kind == agent::ui::MarkdownLineKind::raw);
    CHECK_EQ(unfinished[0].text, "```python");
}

TEST(ux11, markdown_stream_fragmentation_preserves_source_and_recognized_language_labels)
{
    agent::ui::TuiView view;
    view.append_assistant_delta("## Ti");
    view.append_assistant_delta("tle\n\n```py");
    view.append_assistant_delta("thon\nprint('café')\n```\n");
    CHECK_EQ(view.messages().back().text, "## Title\n\n```python\nprint('café')\n```\n");
    const auto rows = view.rendered_styled_conversation_rows(100);
    CHECK(rows[0].text.find("Title") != std::string::npos);
    CHECK(rows[2].kind == agent::ui::MarkdownLineKind::code);
    CHECK_EQ(rows[2].language, "python");
}

TEST(ux11, markdown_code_blocks_retain_the_requested_supported_language)
{
    for (const std::string_view language : {"cpp", "python", "bash", "json", "yaml", "diff"}) {
        const auto lines = agent::ui::present_markdown("```" + std::string(language) + "\nvalue\n```", true);
        CHECK_EQ(lines.size(), std::size_t(1));
        CHECK(lines.front().kind == agent::ui::MarkdownLineKind::code);
        CHECK_EQ(lines.front().language, language);
        CHECK_EQ(lines.front().text, "value");
    }
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
    config.http_timeouts = HttpTimeouts{7, 19, 241};
    const auto data = agent::ui::make_tui_view_data(config, tools);
    agent::ui::TuiView view;
    view.set_view_data(data);
    const auto rows = view.rendered_page_rows(agent::ui::TuiPage::configuration, 120, 0, true);
    std::string rendered;
    for (const auto& row : rows) rendered += row + '\n';
    CHECK(data.endpoint_origin == "https://example.test:8443");
    CHECK(data.api_key_configured);
    CHECK_EQ(data.connect_timeout_seconds, 7L);
    CHECK_EQ(data.idle_timeout_seconds, 19L);
    CHECK_EQ(data.total_timeout_seconds, 241L);
    CHECK(std::any_of(rows.begin(), rows.end(), [](const std::string& row) {
        return row.find("Timeouts: connect 7s, idle 19s, total 241s") != std::string::npos;
    }));
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
    const auto rows = view.rendered_page_rows(agent::ui::TuiPage::help, 120, 1, true);
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
            CHECK(row.starts_with("Bot: "));
            reconstructed += row.substr(5);
        } else {
            CHECK(row.starts_with("     "));
            reconstructed += row.substr(5);
        }
    }
    CHECK_EQ(reconstructed, text);

    const auto wide = view.rendered_conversation_rows(80);
    CHECK_EQ(wide.size(), std::size_t(1));
    CHECK_EQ(wide.front(), "LOFITERM Bot: " + text);
    view.on_resize(10, 2);
    view.on_resize(80, 18);
    const auto resized = view.rendered_conversation_rows(80);
    CHECK_EQ(resized.front(), "LOFITERM Bot: " + text);
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

TEST(ux11, pty_mouse_click_selects_tab_and_keyboard_still_quits)
{
    MockHttpServer server;
    int master = -1;
    winsize size{}; size.ws_row = 20; size.ws_col = 60;
    const pid_t child = ::forkpty(&master, nullptr, nullptr, &size);
    CHECK(child >= 0);
    if (child == 0) {
        const auto driver = std::filesystem::read_symlink("/proc/self/exe").parent_path() / "tui_pty_driver";
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("LC_ALL", "C.UTF-8", 1);
        ::execl(driver.c_str(), driver.c_str(), server.url("/v1/chat/completions").c_str(), "normal", nullptr);
        _exit(127);
    }
    std::string output;
    const auto read_for = [&](std::chrono::milliseconds duration) {
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
    read_for(std::chrono::milliseconds(200));
    CHECK(output.find("Declared tools:") == std::string::npos);
    const auto before_header_click = output.size();
    const std::string click_title = "\033[<0;20;4M\033[<0;20;4m";
    static_cast<void>(::write(master, click_title.data(), click_title.size()));
    read_for(std::chrono::milliseconds(100));
    CHECK_EQ(output.size(), before_header_click);
    CHECK(output.find("[VIEW]") == std::string::npos);
    const std::string click_empty_conversation = "\033[<0;20;5M\033[<0;20;5m";
    static_cast<void>(::write(master, click_empty_conversation.data(), click_empty_conversation.size()));
    read_for(std::chrono::milliseconds(100));
    CHECK_EQ(output.size(), before_header_click);

    const std::string click_f2 = "\033[<0;13;2M\033[<0;13;2m";
    const auto before_tab_click = output.size();
    static_cast<void>(::write(master, click_f2.data(), click_f2.size()));
    read_for(std::chrono::milliseconds(150));
    CHECK(output.substr(before_tab_click).find("MENU") != std::string::npos);
    const std::string click_tool_text = "\033[<0;3;5M\033[<0;3;5m";
    const auto before_content_click = output.size();
    static_cast<void>(::write(master, click_tool_text.data(), click_tool_text.size()));
    read_for(std::chrono::milliseconds(150));
    CHECK(output.substr(before_content_click).find("VIEW") != std::string::npos);
    const auto after_content_focus = output.size();
    const auto before_repeat_content_click = output.size();
    static_cast<void>(::write(master, click_tool_text.data(), click_tool_text.size()));
    read_for(std::chrono::milliseconds(100));
    CHECK_EQ(output.size(), before_repeat_content_click);
    const std::string click_empty_panel = "\033[<0;55;19M\033[<0;55;19m";
    static_cast<void>(::write(master, click_empty_panel.data(), click_empty_panel.size()));
    read_for(std::chrono::milliseconds(100));
    CHECK_EQ(output.size(), after_content_focus);

    static_cast<void>(::write(master, "\033[17~", 5)); // F6 Help
    read_for(std::chrono::milliseconds(100));
    const std::string click_help_section = "\033[<0;10;6M\033[<0;10;6m";
    static_cast<void>(::write(master, click_help_section.data(), click_help_section.size()));
    read_for(std::chrono::milliseconds(150));
    static_cast<void>(::write(master, "\033[21~", 5)); // keyboard F10 remains available
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline && ::waitpid(child, &status, WNOHANG) == 0)
        read_for(std::chrono::milliseconds(30));
    if (::waitpid(child, &status, WNOHANG) == 0) {
        static_cast<void>(::kill(child, SIGKILL));
        static_cast<void>(::waitpid(child, &status, 0));
    }
    ::close(master);
    CHECK(WIFEXITED(status));
    CHECK_EQ(WEXITSTATUS(status), 0);
    CHECK(output.find("Declared tools:") != std::string::npos);
    CHECK(output.find("1. Type a message in Chat.") != std::string::npos);
    CHECK(output.find("\033[?1003h") == std::string::npos);
    CHECK(output.find("\033[?1049l") != std::string::npos);
    CHECK(server.received_bodies().empty());
}
