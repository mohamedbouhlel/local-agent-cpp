#include "agent.hpp"
#include "mock_http_server.hpp"
#include "prompt_reader.hpp"
#include "test_support.hpp"
#include "tool_registry.hpp"
#include "ui/console_renderer.hpp"
#include "ui/console_session.hpp"
#include "ui/presentation_controller.hpp"
#include "ui/terminal_capabilities.hpp"
#include "ui/terminal_text_sanitizer.hpp"
#include "ui/plain_renderer.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <istream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::string sse(const nlohmann::json& event)
{
    return "data: " + event.dump() + "\n\n";
}

std::string text_event(std::string_view text)
{
    return sse({{"choices", nlohmann::json::array({{{"index", 0}, {"delta", {{"content", text}}}}})}});
}

std::string tool_event(std::string_view id, std::string_view name)
{
    const nlohmann::json call{{"index", 0},
                              {"id", id},
                              {"type", "function"},
                              {"function", {{"name", name}, {"arguments", "{}"}}}};
    return sse({{"choices", nlohmann::json::array({{{"index", 0},
                                                    {"delta", {{"tool_calls", nlohmann::json::array({call})}}}}})}});
}

MockHttpServer::Response response(std::string body, int status = 200)
{
    MockHttpServer::Response result;
    result.status = status;
    result.body = std::move(body);
    return result;
}

AgentConfig config_for(const MockHttpServer& server)
{
    return {server.url("/v1/chat/completions"), "local-model", "", 3};
}

agent::ui::TerminalCapabilities capabilities(bool stdout_tty,
                                             bool stderr_tty,
                                             std::string term = "xterm-256color",
                                             bool no_color = false,
                                             std::string locale = "en_US.UTF-8",
                                             unsigned int columns = 100)
{
    agent::ui::TerminalProbe probe;
    probe.stdout_is_tty = stdout_tty;
    probe.stderr_is_tty = stderr_tty;
    probe.term = std::move(term);
    probe.no_color_present = no_color;
    probe.locale = std::move(locale);
    probe.stdout_columns = columns;
    probe.stderr_columns = columns;
    return agent::ui::terminal_capabilities_from_probe(probe);
}

struct RenderHarness {
    explicit RenderHarness(const agent::ui::TerminalCapabilities& caps)
        : controller(std::make_shared<agent::ui::PresentationController>(
              agent::ui::make_console_renderer(output, diagnostics, caps)))
    {
    }

    std::ostringstream output;
    std::ostringstream diagnostics;
    std::shared_ptr<agent::ui::PresentationController> controller;
};

class ThrowOnceRenderer final : public agent::ui::ConsoleRenderer {
public:
    ThrowOnceRenderer(std::ostream& output, std::ostream& diagnostics)
        : output_(output), diagnostics_(diagnostics)
    {
    }

    agent::ui::RendererMode mode() const noexcept override { return agent::ui::RendererMode::plain; }
    void render_text(std::string_view text) override { output_ << text; }
    void render_status(agent::ui::StatusKind, std::string_view message) override
    {
        if (throw_next_) {
            throw_next_ = false;
            throw std::runtime_error("renderer test failure");
        }
        diagnostics_ << message << '\n';
    }
    void flush_text() override { output_.flush(); }

private:
    std::ostream& output_;
    std::ostream& diagnostics_;
    bool throw_next_ = true;
};

class ThrowOnTextRenderer final : public agent::ui::ConsoleRenderer {
public:
    ThrowOnTextRenderer(std::ostream& output, std::ostream& diagnostics)
        : output_(output), diagnostics_(diagnostics)
    {
    }

    agent::ui::RendererMode mode() const noexcept override { return agent::ui::RendererMode::plain; }
    void render_text(std::string_view text) override
    {
        if (throw_on_text_) {
            throw_on_text_ = false;
            throw std::runtime_error("text renderer failed");
        }
        output_ << text;
    }
    void render_status(agent::ui::StatusKind, std::string_view message) override
    {
        diagnostics_ << message << '\n';
    }
    void flush_text() override { output_.flush(); }

private:
    std::ostream& output_;
    std::ostream& diagnostics_;
    bool throw_on_text_ = true;
};

std::size_t count_occurrences(std::string_view text, std::string_view needle)
{
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string_view::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

} // namespace

TEST(ui3, plain_renderer_has_no_ansi_and_keeps_diagnostics_separate)
{
    std::ostringstream output;
    std::ostringstream diagnostics;
    agent::ui::PlainRenderer renderer(output, diagnostics);
    renderer.render_text("hello\nworld");
    renderer.render_status(agent::ui::StatusKind::info, "waiting");

    CHECK_EQ(output.str(), std::string("hello\nworld"));
    CHECK(diagnostics.str().find("[info] waiting\n") != std::string::npos);
    CHECK(output.str().find('\x1b') == std::string::npos);
    CHECK(diagnostics.str().find('\x1b') == std::string::npos);
}

TEST(ui3, renderer_selection_respects_dumb_no_color_and_redirected_stdout)
{
    std::ostringstream output;
    std::ostringstream diagnostics;

    auto dumb = agent::ui::make_console_renderer(output, diagnostics, capabilities(true, true, "dumb"));
    CHECK(dumb->mode() == agent::ui::RendererMode::plain);

    auto redirected = agent::ui::make_console_renderer(output, diagnostics, capabilities(false, true));
    CHECK(redirected->mode() == agent::ui::RendererMode::plain);
    redirected->render_status(agent::ui::StatusKind::error, "separate stream");
    CHECK(diagnostics.str().find('\x1b') == std::string::npos);

    auto no_color = agent::ui::make_console_renderer(output, diagnostics, capabilities(true, true, "xterm", true));
    CHECK(no_color->mode() == agent::ui::RendererMode::rich);
    no_color->render_status(agent::ui::StatusKind::info, "no colors");
    CHECK(diagnostics.str().find('\x1b') == std::string::npos);
}

TEST(ui3, rich_renderer_uses_semantic_color_and_ascii_fallback)
{
    std::ostringstream output;
    std::ostringstream diagnostics;
    auto renderer = agent::ui::make_console_renderer(output,
                                                      diagnostics,
                                                      capabilities(true, true, "xterm", false, "C"));
    CHECK(renderer->mode() == agent::ui::RendererMode::rich);
    renderer->render_status(agent::ui::StatusKind::model, "streaming");
    CHECK(diagnostics.str().find("\x1b[35m[model]\x1b[0m") != std::string::npos);
    CHECK(diagnostics.str().find("◆") == std::string::npos);
}

TEST(ui3, rich_renderer_uses_unicode_marker_and_compact_narrow_layout)
{
    std::ostringstream output;
    std::ostringstream diagnostics;
    auto caps = capabilities(true, true, "xterm", true, "fr_FR.UTF-8", 79);
    auto renderer = agent::ui::make_console_renderer(output, diagnostics, caps);
    renderer->render_status(agent::ui::StatusKind::success, "done");
    CHECK(diagnostics.str().find("✓ done\n") != std::string::npos);
    CHECK(diagnostics.str().find("  done") == std::string::npos);
}

TEST(ui3, rich_renderer_keeps_unknown_width_unknown)
{
    std::ostringstream output;
    std::ostringstream diagnostics;
    agent::ui::TerminalProbe probe;
    probe.stdout_is_tty = true;
    probe.stderr_is_tty = true;
    probe.term = "xterm";
    probe.locale = "C";
    auto renderer = agent::ui::make_console_renderer(
        output, diagnostics, agent::ui::terminal_capabilities_from_probe(probe));
    renderer->render_status(agent::ui::StatusKind::info, "width unavailable");
    CHECK(renderer->mode() == agent::ui::RendererMode::rich);
    CHECK(diagnostics.str().find("  width unavailable") != std::string::npos);
}

TEST(ui3, sanitizer_handles_split_escape_sequences_controls_and_utf8)
{
    agent::ui::TerminalTextSanitizer sanitizer;
    std::string output;
    output += sanitizer.feed("A\x1b");
    output += sanitizer.feed("]0;window-title");
    output += sanitizer.feed("\x07" "B\x1b[");
    output += sanitizer.feed("2J" "C\x1bPpayload");
    output += sanitizer.feed("\x1b\\D");
    output += sanitizer.feed("\x01\r\t");
    output += sanitizer.feed("\xC2");
    output += sanitizer.feed("\x9B"); // UTF-8 encoded C1 control, rendered visibly.
    output += sanitizer.feed("\xE2\x80");
    output += sanitizer.feed("\xAE"); // Right-to-left override.
    output += sanitizer.finish();

    CHECK_EQ(output, std::string("ABCD^A^M\\t\\u009B\\u202E"));
    CHECK(output.find('\x1b') == std::string::npos);
    CHECK(output.find('\x9b') == std::string::npos);
}

TEST(ui3, sanitizer_discards_incomplete_sequences_and_preserves_ordinary_utf8)
{
    agent::ui::TerminalTextSanitizer sanitizer;
    std::string output = sanitizer.feed("cafe ");
    output += sanitizer.feed("\xC3");
    output += sanitizer.feed("\xA9");
    output += sanitizer.feed("x\x1b[");
    output += sanitizer.finish();

    CHECK_EQ(output, std::string("cafe \xC3\xA9x"));
    CHECK(output.find('\x1b') == std::string::npos);

    agent::ui::TerminalTextSanitizer unfinished_osc;
    auto osc_output = unfinished_osc.feed("visible\x1b]0;unfinished-title");
    osc_output += unfinished_osc.finish();
    CHECK_EQ(osc_output, std::string("visible"));

    agent::ui::TerminalTextSanitizer unfinished_dcs;
    auto dcs_output = unfinished_dcs.feed("\x1bPunfinished-payload");
    dcs_output += unfinished_dcs.finish();
    CHECK(dcs_output.empty());
}

TEST(ui3, sanitizer_discards_raw_c1_csi_and_osc_sequences)
{
    agent::ui::TerminalTextSanitizer sanitizer;
    std::string input = "A";
    input += static_cast<char>(0x9BU);
    input += "2JB";
    input += static_cast<char>(0x9DU);
    input += "8;;https://untrusted.invalid";
    input += static_cast<char>(0x9CU);
    input += "C";
    const auto output = sanitizer.feed(input) + sanitizer.finish();
    CHECK_EQ(output, std::string("ABC"));
}

TEST(ui3, tool_names_are_sanitized_before_rich_status_rendering)
{
    RenderHarness harness(capabilities(true, true));
    std::uint64_t sequence = 0;
    const auto emit = [&](agent::ui::EventPayload payload,
                          std::optional<agent::ui::RequestId> request_id = std::nullopt,
                          std::optional<agent::ui::ToolCallId> tool_call_id = std::nullopt) {
        agent::ui::EventMetadata metadata;
        metadata.session_id = {1};
        metadata.turn_id = agent::ui::TurnId{1};
        metadata.request_id = request_id;
        metadata.tool_call_id = tool_call_id;
        metadata.sequence = ++sequence;
        harness.controller->on_event({std::move(metadata), std::move(payload)});
    };
    emit(agent::ui::RequestStarted{});
    // Exercise status sanitization through the actual renderer API. Model- and
    // tool-controlled strings are never allowed to emit terminal control bytes.
    const agent::ui::RequestId request_id{1};
    emit(agent::ui::HttpRequestStarted{1}, request_id);
    emit(agent::ui::HttpRequestFinished{std::chrono::milliseconds(1), 0,
                                        agent::ui::HttpRequestOutcome::completed},
         request_id);
    emit(agent::ui::ToolStarted{std::string("unknown\x1b]0;pwn\x07")}, request_id,
         agent::ui::ToolCallId{"call"});
    CHECK(harness.diagnostics.str().find("pwn") == std::string::npos);
    CHECK(harness.diagnostics.str().find("\x1b]") == std::string::npos);
}

TEST(ui3, failure_on_later_turn_does_not_reuse_previous_turn_partial_text)
{
    RenderHarness harness(capabilities(false, false));
    const auto emit = [&](std::uint64_t sequence, agent::ui::EventPayload payload,
                          std::optional<agent::ui::RequestId> request_id = std::nullopt) {
        agent::ui::EventMetadata metadata;
        metadata.session_id = {5};
        metadata.turn_id = agent::ui::TurnId{1};
        metadata.request_id = request_id;
        metadata.sequence = sequence;
        harness.controller->on_event({std::move(metadata), std::move(payload)});
    };
    const agent::ui::RequestId request_id{1};
    emit(1, agent::ui::RequestStarted{});
    emit(2, agent::ui::HttpRequestStarted{1}, request_id);
    emit(3, agent::ui::TextDelta{"partial"}, request_id);
    emit(4, agent::ui::HttpRequestFinished{std::chrono::milliseconds(1), 7,
                                           agent::ui::HttpRequestOutcome::completed},
         request_id);
    emit(5, agent::ui::TurnFinished{agent::ui::TurnOutcome::completed, 1, 0, 0});
    CHECK(harness.controller->turn_had_text());

    // Simulate a later send after Agent permanently disabled the observer.
    // Its presentation projection remains on the prior terminal turn.
    harness.controller->begin_turn();
    harness.controller->report_failure("later request failed");
    CHECK(harness.diagnostics.str().find("later request failed") != std::string::npos);
    CHECK(harness.diagnostics.str().find("Partial model output was shown") == std::string::npos);
}

TEST(ui3, cli_turn_runner_streams_model_content_exactly_once)
{
    MockHttpServer server;
    server.enqueue(response(text_event("first") + text_event(" second") + "data: [DONE]\n\n"));
    ToolRegistry tools;
    RenderHarness harness(capabilities(false, false));
    Agent agent(tools, config_for(server), harness.controller);

    const auto result = agent::ui::run_console_turn(agent, "unchanged prompt", server.url("/v1/chat/completions"),
                                                    *harness.controller);
    CHECK(result == agent::ui::ConsoleTurnResult::completed);
    CHECK_EQ(harness.output.str(), std::string("first second\n"));
    CHECK_EQ(count_occurrences(harness.output.str(), "first"), std::size_t(1));
    CHECK(harness.controller->snapshot().phase == agent::ui::PresentationPhase::completed);
    CHECK(server.received_bodies().size() == 1);
    CHECK(server.received_bodies()[0].find("unchanged prompt") != std::string::npos);
    CHECK(harness.diagnostics.str().find("task success is not independently verified") != std::string::npos);
}

TEST(ui3, prompt_reader_input_uses_the_same_cli_rendering_path)
{
    MockHttpServer server;
    server.enqueue(response(text_event("reply") + "data: [DONE]\n\n"));
    ToolRegistry tools;
    RenderHarness harness(capabilities(false, false));
    Agent agent(tools, config_for(server), harness.controller);
    std::istringstream user_input("exact prompt\nexit\n");
    std::ostringstream prompt_output;
    PromptReader reader(user_input, prompt_output);

    const auto prompt = reader.read();
    CHECK(prompt.status == PromptStatus::Line);
    CHECK_EQ(prompt.content, std::string("exact prompt"));
    CHECK(agent::ui::run_console_turn(agent, prompt.content, server.url("/v1/chat/completions"),
                                      *harness.controller)
          == agent::ui::ConsoleTurnResult::completed);
    const auto exit_prompt = reader.read();
    CHECK(exit_prompt.status == PromptStatus::Line);
    CHECK_EQ(exit_prompt.content, std::string("exit"));
    CHECK_EQ(prompt_output.str(), std::string("> > "));
    CHECK_EQ(harness.output.str(), std::string("reply\n"));
    CHECK(server.received_bodies().front().find("exact prompt") != std::string::npos);
}

TEST(ui3, observer_failure_before_text_uses_one_complete_response_fallback)
{
    MockHttpServer server;
    server.enqueue(response(text_event("fallback response") + "data: [DONE]\n\n"));
    ToolRegistry tools;
    std::ostringstream output;
    std::ostringstream diagnostics;
    auto controller = std::make_shared<agent::ui::PresentationController>(
        std::make_unique<ThrowOnceRenderer>(output, diagnostics));
    Agent agent(tools, config_for(server), controller);

    CHECK(agent::ui::run_console_turn(agent, "request", server.url("/v1/chat/completions"), *controller)
          == agent::ui::ConsoleTurnResult::completed);
    CHECK(controller->observer_failed());
    CHECK(!controller->turn_had_text());
    CHECK_EQ(output.str(), std::string("fallback response\n"));
    CHECK_EQ(count_occurrences(output.str(), "fallback response"), std::size_t(1));
    CHECK(diagnostics.str().find("UI observer disabled") != std::string::npos);
    CHECK(diagnostics.str().find("event observation became unavailable") != std::string::npos);
}

TEST(ui3, observer_failure_after_text_marks_display_as_incomplete)
{
    MockHttpServer server;
    server.enqueue(response(text_event("render failure") + "data: [DONE]\n\n"));
    ToolRegistry tools;
    std::ostringstream output;
    std::ostringstream diagnostics;
    auto controller = std::make_shared<agent::ui::PresentationController>(
        std::make_unique<ThrowOnTextRenderer>(output, diagnostics));
    Agent agent(tools, config_for(server), controller);

    CHECK(agent::ui::run_console_turn(agent, "request", server.url("/v1/chat/completions"), *controller)
          == agent::ui::ConsoleTurnResult::completed);
    CHECK(controller->observer_failed());
    CHECK(controller->turn_had_text());
    CHECK_EQ(output.str(), std::string());
    CHECK(diagnostics.str().find("UI observer disabled") != std::string::npos);
    CHECK(diagnostics.str().find("displayed output may be incomplete") != std::string::npos);
}

TEST(ui3, split_sse_deltas_cannot_smuggle_csi_or_osc_controls)
{
    MockHttpServer server;
    server.enqueue(response(text_event("A") + text_event("\x1b[") + text_event("31mred")
                           + text_event("\x1b]0;evil") + text_event("\x07" "B")
                           + "data: [DONE]\n\n"));
    ToolRegistry tools;
    RenderHarness harness(capabilities(true, true));
    Agent agent(tools, config_for(server), harness.controller);

    CHECK(agent::ui::run_console_turn(agent, "show text", server.url("/v1/chat/completions"),
                                      *harness.controller)
          == agent::ui::ConsoleTurnResult::completed);
    CHECK_EQ(harness.output.str(), std::string("AredB\n"));
    CHECK(harness.output.str().find('\x1b') == std::string::npos);
    CHECK(harness.output.str().find("evil") == std::string::npos);
    CHECK(harness.diagnostics.str().find("\x1b]") == std::string::npos);
}

TEST(ui3, recovered_tool_failure_is_visible_without_false_global_failure)
{
    MockHttpServer server;
    server.enqueue(response(tool_event("call-1", "broken") + "data: [DONE]\n\n"));
    server.enqueue(response(text_event("recovered") + "data: [DONE]\n\n"));

    ToolRegistry tools;
    const auto schema = nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()}};
    tools.register_tool(Tool{"broken", "failure", schema, [](nlohmann::json) -> std::string {
                                 throw std::runtime_error("private tool result");
                             }});
    RenderHarness harness(capabilities(false, false));
    Agent agent(tools, config_for(server), harness.controller);

    CHECK(agent::ui::run_console_turn(agent, "use tool", server.url("/v1/chat/completions"),
                                      *harness.controller)
          == agent::ui::ConsoleTurnResult::completed);
    CHECK(harness.controller->snapshot().phase == agent::ui::PresentationPhase::completed);
    CHECK_EQ(harness.controller->snapshot().tools_failed, std::uint64_t(1));
    CHECK(harness.diagnostics.str().find("Tool failed: broken") != std::string::npos);
    CHECK(harness.diagnostics.str().find("private tool result") == std::string::npos);
    CHECK(harness.diagnostics.str().find("task success is not independently verified") != std::string::npos);
    const auto tool_start = harness.diagnostics.str().find("Running tool: broken");
    const auto tool_failure = harness.diagnostics.str().find("Tool failed: broken");
    const auto turn_end = harness.diagnostics.str().find("Turn completed technically");
    CHECK(tool_start < tool_failure);
    CHECK(tool_failure < turn_end);
}

TEST(ui3, http_failure_is_reported_on_stderr_without_success_status)
{
    MockHttpServer server;
    server.enqueue(response("failure body", 503));
    ToolRegistry tools;
    RenderHarness harness(capabilities(false, false));
    Agent agent(tools, config_for(server), harness.controller);

    CHECK(agent::ui::run_console_turn(agent, "request", server.url("/v1/chat/completions"),
                                      *harness.controller)
          == agent::ui::ConsoleTurnResult::failed);
    CHECK(harness.controller->snapshot().phase == agent::ui::PresentationPhase::failed);
    CHECK(harness.diagnostics.str().find("Impossible de contacter LLM_BASE_URL") != std::string::npos);
    CHECK(harness.diagnostics.str().find("task success is not independently verified") == std::string::npos);
    CHECK(harness.output.str().find("failure body") == std::string::npos);
}

TEST(ui3, incomplete_sse_marks_already_streamed_text_as_unvalidated)
{
    MockHttpServer server;
    server.enqueue(response(text_event("partial answer"))); // No mandatory [DONE].
    ToolRegistry tools;
    RenderHarness harness(capabilities(false, false));
    Agent agent(tools, config_for(server), harness.controller);

    CHECK(agent::ui::run_console_turn(agent, "request", server.url("/v1/chat/completions"),
                                      *harness.controller)
          == agent::ui::ConsoleTurnResult::failed);
    CHECK(harness.controller->snapshot().phase == agent::ui::PresentationPhase::failed);
    CHECK_EQ(harness.output.str(), std::string("partial answer\n"));
    CHECK(harness.diagnostics.str().find("incomplete model stream") != std::string::npos);
    CHECK(harness.diagnostics.str().find("Streamed model output may be incomplete; it is not a completed response.")
          != std::string::npos);
    CHECK(harness.diagnostics.str().find("task success is not independently verified") == std::string::npos);
}

TEST(ui3, malformed_sse_marks_streamed_text_as_unvalidated)
{
    MockHttpServer server;
    server.enqueue(response(text_event("provisional") + "data: {not-json}\n\n" + "data: [DONE]\n\n"));
    ToolRegistry tools;
    RenderHarness harness(capabilities(false, false));
    Agent agent(tools, config_for(server), harness.controller);

    CHECK(agent::ui::run_console_turn(agent, "request", server.url("/v1/chat/completions"),
                                      *harness.controller)
          == agent::ui::ConsoleTurnResult::failed);
    CHECK(harness.controller->snapshot().phase == agent::ui::PresentationPhase::failed);
    CHECK_EQ(harness.output.str(), std::string("provisional\n"));
    CHECK(harness.diagnostics.str().find("malformed JSON in model stream event") != std::string::npos);
    CHECK(harness.diagnostics.str().find("Streamed model output may be incomplete; it is not a completed response.")
          != std::string::npos);
    CHECK(harness.diagnostics.str().find("task success is not independently verified") == std::string::npos);
    CHECK(harness.diagnostics.str().find("not-json") == std::string::npos);
}

TEST(ui3, hostile_tool_name_is_neutralized_and_failure_can_recover)
{
    const std::string hostile_name = std::string("not-real\x1b]0;pwn\x07", 16);
    MockHttpServer server;
    server.enqueue(response(tool_event("call-x", hostile_name) + "data: [DONE]\n\n"));
    server.enqueue(response(text_event("done") + "data: [DONE]\n\n"));
    ToolRegistry tools;
    RenderHarness harness(capabilities(true, true));
    Agent agent(tools, config_for(server), harness.controller);

    CHECK(agent::ui::run_console_turn(agent, "request", server.url("/v1/chat/completions"),
                                      *harness.controller)
          == agent::ui::ConsoleTurnResult::completed);
    CHECK(harness.controller->snapshot().phase == agent::ui::PresentationPhase::completed);
    CHECK(harness.diagnostics.str().find("pwn") == std::string::npos);
    CHECK(harness.diagnostics.str().find("\x1b]") == std::string::npos);
}

TEST(ui3, one_line_diagnostics_escape_line_breaks_and_c0_controls)
{
    CHECK_EQ(agent::ui::sanitize_terminal_line("first\nsecond\r\x07"),
             std::string("first\\nsecond^M^G"));
}
