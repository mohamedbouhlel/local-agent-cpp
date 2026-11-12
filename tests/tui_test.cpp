#include "mock_http_server.hpp"
#include "test_support.hpp"
#include "ui/tui_view.hpp"
#include "ui/terminal_session.hpp"
#include "ui/tui_view_data.hpp"
#include "ui/ui_options.hpp"

#include <pty.h>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <array>
#include <clocale>
#include <filesystem>
#include <fstream>
#include <poll.h>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
struct TuiScreenSnapshot {
    std::vector<std::string> rows;
    std::vector<chtype> title_attributes;
    std::vector<chtype> first_content_attributes;
    int cursor_row = -1;
    int cursor_column = -1;
    bool partial_bold = false;
};

TuiScreenSnapshot draw_tui_screen(agent::ui::TuiView& view, std::string_view input, std::size_t cursor,
                                  int rows, int columns,
                                  agent::ui::TuiPage page = agent::ui::TuiPage::conversation,
                                  bool force_no_color = false,
                                  agent::ui::TuiFocus focus = agent::ui::TuiFocus::composer,
                                  std::optional<agent::ui::TuiFocus> redraw_focus = std::nullopt)
{
    int master = -1, slave = -1;
    winsize size{};
    size.ws_row = static_cast<unsigned short>(rows);
    size.ws_col = static_cast<unsigned short>(columns);
    if (::openpty(&master, &slave, nullptr, nullptr, &size) != 0) return {};
    FILE* input_stream = ::fdopen(::dup(slave), "r");
    FILE* output_stream = ::fdopen(::dup(slave), "w");
    ::close(slave);
    if (!input_stream || !output_stream) {
        if (input_stream) std::fclose(input_stream);
        if (output_stream) std::fclose(output_stream);
        ::close(master);
        return {};
    }

    const char* old_term_value = std::getenv("TERM");
    const std::string old_term = old_term_value ? old_term_value : "";
    const bool had_term = old_term_value != nullptr;
    const char* old_no_color_value = std::getenv("NO_COLOR");
    const std::string old_no_color = old_no_color_value ? old_no_color_value : "";
    const bool had_no_color = old_no_color_value != nullptr;
    const char* old_locale_value = std::setlocale(LC_ALL, nullptr);
    const std::string old_locale = old_locale_value ? old_locale_value : "C";
    ::setenv("TERM", "xterm-256color", 1);
    if (force_no_color) ::setenv("NO_COLOR", "1", 1);
    else if (!had_no_color) ::unsetenv("NO_COLOR");
    static_cast<void>(std::setlocale(LC_ALL, "C.UTF-8"));
    SCREEN* screen = ::newterm("xterm-256color", output_stream, input_stream);
    TuiScreenSnapshot result;
    if (screen != nullptr) {
        static_cast<void>(::set_term(screen));
        static_cast<void>(::resizeterm(rows, columns));
        agent::ui::TuiNavigation navigation;
        navigation.select(page);
        navigation.set_focus(focus);
        view.draw(stdscr, true, navigation, input, cursor);
        if (redraw_focus) {
            navigation.set_focus(*redraw_focus);
            view.draw(stdscr, true, navigation, input, cursor);
        }
        getyx(stdscr, result.cursor_row, result.cursor_column);
        result.rows.resize(static_cast<std::size_t>(rows));
        for (int y = 0; y < rows; ++y) {
            auto& line = result.rows[static_cast<std::size_t>(y)];
            line.reserve(static_cast<std::size_t>(columns));
            for (int x = 0; x < columns; ++x) {
                const chtype cell = mvwinch(stdscr, y, x);
                if (y == 3) result.title_attributes.push_back(cell & (A_ATTRIBUTES | A_COLOR));
                if (y == 4) result.first_content_attributes.push_back(cell & (A_ATTRIBUTES | A_COLOR));
                if (y == 2 && x < columns && (cell & A_CHARTEXT) != 0
                    && (cell & A_CHARTEXT) != static_cast<chtype>(' ')
                    && (cell & A_BOLD) != 0) result.partial_bold = true;
                const auto ch = static_cast<char>(cell & A_CHARTEXT);
                line.push_back(ch >= 32 && ch < 127 ? ch : ' ');
            }
        }
        (void)::endwin();
        ::delscreen(screen);
    }
    if (had_term) ::setenv("TERM", old_term.c_str(), 1);
    else ::unsetenv("TERM");
    if (had_no_color) ::setenv("NO_COLOR", old_no_color.c_str(), 1);
    else ::unsetenv("NO_COLOR");
    static_cast<void>(std::setlocale(LC_ALL, old_locale.c_str()));
    std::fclose(input_stream);
    std::fclose(output_stream);
    ::close(master);
    return result;
}

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

class PtyScreen final {
public:
    PtyScreen(int rows = 24, int columns = 80)
        : cells_(static_cast<std::size_t>(rows), std::string(static_cast<std::size_t>(columns), ' ')),
          rows_(rows), columns_(columns) {}

    void feed(std::string_view bytes)
    {
        pending_.append(bytes);
        std::size_t i = 0;
        while (i < pending_.size()) {
            const std::string_view input(pending_);
            const auto value = static_cast<unsigned char>(input[i]);
            if (value == 0x1b) {
                if (i + 1 >= input.size()) break;
                if (input[i + 1] == '[') {
                    const auto begin = i + 2;
                    auto end = begin;
                    while (end < input.size()) {
                        const auto c = static_cast<unsigned char>(input[end]);
                        if (c >= 0x40 && c <= 0x7e) break;
                        ++end;
                    }
                    if (end == input.size()) break;
                    const char final = input[end];
                    const auto parameters = input.substr(begin, end - begin);
                    remember_control(parameters, final);
                    apply_csi(parameters, final);
                    i = end + 1;
                    continue;
                }
                if (input[i + 1] == ']') {
                    const auto sequence_start = i;
                    i += 2;
                    bool terminated = false;
                    while (i < input.size()) {
                        if (input[i] == '\a') { ++i; terminated = true; break; }
                        if (input[i] == '\x1b' && i + 1 < input.size() && input[i + 1] == '\\') {
                            i += 2;
                            terminated = true;
                            break;
                        }
                        ++i;
                    }
                    if (!terminated) { i = sequence_start; break; }
                    continue;
                }
                // ISO-2022 charset designation, e.g. ESC ( B or ESC ) 0.
                // Do not turn the final designator into screen text.
                if (input[i + 1] == '(' || input[i + 1] == ')') {
                    if (i + 2 >= input.size()) break;
                    i += 3;
                    continue;
                }
                // Other two-byte ESC controls (including cursor save/restore).
                i += 2;
                continue;
            }
            if (value == '\r') column_ = 0;
            else if (value == '\n') line_feed();
            else if (value == '\b') column_ = std::max(0, column_ - 1);
            else if (value == '\t') column_ = std::min(columns_ - 1, (column_ / 8 + 1) * 8);
            else if (value >= 0x20 && value != 0x7f) {
                ++printable_bytes_seen_;
                put(static_cast<char>(value));
            }
            ++i;
        }
        pending_.erase(0, i);
    }

    std::string row(int index) const
    {
        if (index < 0 || index >= rows_) return {};
        auto result = cells_[static_cast<std::size_t>(index)];
        while (!result.empty() && result.back() == ' ') result.pop_back();
        return result;
    }

    bool approval_header_visible() const
    {
        return std::any_of(cells_.begin(), cells_.end(), [](const std::string& line) {
            return line.find("WRITE APPROVAL") != std::string::npos;
        });
    }

    bool input_prompt_visible() const { return cells_[static_cast<std::size_t>(rows_ - 1)].starts_with("Input: "); }
    bool input_is_no_visible() const { return row(rows_ - 1) == "Input: no"; }
    std::size_t printable_bytes_seen() const { return printable_bytes_seen_; }
    std::string safe_input_row() const
    {
        const auto line = row(rows_ - 1);
        if (!line.starts_with("Input: ")) return "<Input prompt not visible>";
        // The test's expected input is fixed. Never dump the screen, challenge,
        // path, content, or arbitrary model output on a failing test.
        const auto value = line.substr(7);
        if (value == "no") return "Input: no";
        std::string mask;
        for (const char c : value) {
            mask.push_back(
                c == 'n' || c == 'o' ? c : c == ' ' ? '_' : '?'
            );
        }
        return "Input: <" + std::to_string(value.size())
            + " chars; mask=" + mask + ">";
    }

    std::string recent_controls() const
    {
        std::ostringstream out;
        for (const auto& control : controls_) out << control << ' ';
        return out.str();
    }

    std::string safe_screen_summary() const
    {
        std::ostringstream out;
        for (int y = 0; y < rows_; ++y) {
            const auto line = row(y);
            if (line.find("WRITE APPROVAL") != std::string::npos) out << " header_row=" << y;
            if (line.starts_with("Input: ")) out << " input_row=" << y << " input_chars=" << line.size() - 7;
            if (line.starts_with("Confirm: yes ")) out << " challenge_row=" << y;
        }
        out << " cursor=" << row_ << ',' << column_;
        return out.str();
    }

private:
    static int parameter(std::string_view text, std::size_t index, int fallback)
    {
        if (!text.empty() && (text.front() == '?' || text.front() == '>')) text.remove_prefix(1);
        std::size_t begin = 0;
        for (std::size_t current = 0; current < index; ++current) {
            const auto separator = text.find(';', begin);
            if (separator == std::string_view::npos) return fallback;
            begin = separator + 1;
        }
        const auto end = text.find(';', begin);
        const auto part = text.substr(begin, end == std::string_view::npos ? text.size() - begin : end - begin);
        if (part.empty()) return fallback;
        int value = 0;
        for (const char c : part) {
            if (c < '0' || c > '9') return fallback;
            value = value * 10 + (c - '0');
        }
        return value;
    }

    void remember_control(std::string_view parameters, char final)
    {
        std::string item = "CSI ";
        for (const char c : parameters) {
            if ((c >= '0' && c <= '9') || c == ';' || c == '?' || c == '>') item.push_back(c);
        }
        item.push_back(final);
        controls_.push_back(std::move(item));
        if (controls_.size() > 24) controls_.erase(controls_.begin());
    }

    void apply_csi(std::string_view params, char final)
    {
        const int first = parameter(params, 0, 1);
        const int second = parameter(params, 1, 1);
        switch (final) {
        case 'H': case 'f': row_ = std::clamp(first - 1, 0, rows_ - 1); column_ = std::clamp(second - 1, 0, columns_ - 1); break;
        case 'A': row_ = std::max(0, row_ - first); break;
        case 'B': row_ = std::min(rows_ - 1, row_ + first); break;
        case 'C': column_ = std::min(columns_ - 1, column_ + first); break;
        case 'D': column_ = std::max(0, column_ - first); break;
        case 'E': row_ = std::min(rows_ - 1, row_ + first); column_ = 0; break;
        case 'F': row_ = std::max(0, row_ - first); column_ = 0; break;
        case 'G': column_ = std::clamp(first - 1, 0, columns_ - 1); break;
        case 'd': row_ = std::clamp(first - 1, 0, rows_ - 1); break;
        case 'J': erase_display(parameter(params, 0, 0)); break;
        case 'K': erase_line(parameter(params, 0, 0)); break;
        case 'X': erase_cells(first); break;
        case 'm': case 'h': case 'l': case 'r': case 's': case 'u': break;
        default: break;
        }
    }

    void put(char c)
    {
        if (column_ >= columns_) { column_ = 0; line_feed(); }
        cells_[static_cast<std::size_t>(row_)][static_cast<std::size_t>(column_)] = c;
        ++column_;
    }

    void line_feed()
    {
        if (row_ + 1 < rows_) { ++row_; return; }
        std::rotate(cells_.begin(), cells_.begin() + 1, cells_.end());
        cells_.back().assign(static_cast<std::size_t>(columns_), ' ');
    }

    void erase_line(int mode)
    {
        int begin = mode == 1 ? 0 : column_;
        int end = mode == 0 ? columns_ : mode == 1 ? column_ + 1 : columns_;
        for (int x = begin; x < end; ++x) cells_[static_cast<std::size_t>(row_)][static_cast<std::size_t>(x)] = ' ';
    }

    void erase_display(int mode)
    {
        if (mode == 2 || mode == 3) {
            for (auto& line : cells_) line.assign(static_cast<std::size_t>(columns_), ' ');
        } else if (mode == 0) {
            erase_line(0);
            for (int y = row_ + 1; y < rows_; ++y) cells_[static_cast<std::size_t>(y)].assign(static_cast<std::size_t>(columns_), ' ');
        } else if (mode == 1) {
            for (int y = 0; y < row_; ++y) cells_[static_cast<std::size_t>(y)].assign(static_cast<std::size_t>(columns_), ' ');
            erase_line(1);
        }
    }

    void erase_cells(int count)
    {
        for (int x = column_; x < std::min(columns_, column_ + count); ++x)
            cells_[static_cast<std::size_t>(row_)][static_cast<std::size_t>(x)] = ' ';
    }

    std::vector<std::string> cells_;
    std::string pending_;
    int rows_;
    int columns_;
    int row_ = 0;
    int column_ = 0;
    std::size_t printable_bytes_seen_ = 0;
    std::vector<std::string> controls_;
};

struct TuiApprovalRun {
    int status = 0;
    bool exited = false;
    bool dialog_seen = false;
    bool approval_prompt_ready = false;
    bool dialog_kept_typed_no = false;
    bool approval_prompt_emitted_before_n = false;
    bool approval_prompt_emitted_before_o = false;
    bool no_tool_request_before_typed_no_enter = false;
    bool no_file_before_typed_no_enter = false;
    bool expiry_notice_seen = false;
    bool mouse_reporting_disabled = false;
    bool mouse_reporting_restored = false;
    bool writer_error_seen = false;
    std::string output;
    std::vector<std::pair<std::string, long long>> pty_timeline_ms;
    std::string failure_screen_input_line;
    std::string failure_recent_controls;
};

void capture_mouse_reporting_transitions(TuiApprovalRun& result)
{
    constexpr std::string_view enabled = "\033[?1006;1000h";
    constexpr std::string_view disabled = "\033[?1006;1000l";
    const auto first_enabled = result.output.find(enabled);
    const auto suspended = result.output.find(disabled);
    result.mouse_reporting_disabled = suspended != std::string::npos;
    result.mouse_reporting_restored = first_enabled != std::string::npos && suspended != std::string::npos
        && result.output.find(enabled, suspended + disabled.size()) != std::string::npos;
}

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
    const auto started_at = std::chrono::steady_clock::now();
    PtyScreen screen;
    auto mark_event = [&](std::string label) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at).count();
        result.pty_timeline_ms.emplace_back(std::move(label), elapsed);
    };
    int master = -1;
    winsize size{}; size.ws_row = 24; size.ws_col = 80;
    const pid_t child = ::forkpty(&master, nullptr, nullptr, &size);
    if (child < 0) return result;
    if (child == 0) {
        (void)::chdir(working_directory.c_str());
        const auto executable = std::filesystem::read_symlink("/proc/self/exe").parent_path() / "tui_pty_driver";
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("LC_ALL", "C.UTF-8", 1);
        const char* mode = decision == "expire-typed" ? "short-expiry" : "normal";
        ::execl(executable.c_str(), executable.c_str(), server.url("/v1/chat/completions").c_str(), mode, nullptr);
        _exit(127);
    }

    auto read_for = [&] (std::chrono::milliseconds duration) {
        const auto deadline = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < deadline) {
            pollfd descriptor{master, POLLIN, 0};
            if (::poll(&descriptor, 1, 20) > 0 && (descriptor.revents & POLLIN)) {
                char bytes[4096];
                const auto count = ::read(master, bytes, sizeof(bytes));
                if (count > 0) {
                    const auto view = std::string_view(bytes, static_cast<std::size_t>(count));
                    result.output.append(view);
                    screen.feed(view);
                }
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
    if (result.dialog_seen) mark_event("approval header captured");
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
        } else if (decision == "approve-existing") {
            const auto challenge = challenge_from_tui_output(result.output);
            const auto target = working_directory / "workspace/nested/denied.txt";
            std::filesystem::create_directories(target.parent_path());
            std::ofstream existing(target, std::ios::binary);
            existing << "preserve-existing-content";
            existing.close();
            if (!challenge.empty()) write(approval_confirmation(challenge) + "\n");
        } else if (decision == "typed-approve" || decision == "fast-approve") {
            const auto challenge = challenge_from_tui_output(result.output);
            if (!challenge.empty()) {
                const auto confirmation = approval_confirmation(challenge) + "\n";
                if (decision == "typed-approve") {
                    for (const char character : confirmation) write(std::string_view(&character, 1));
                } else {
                    static_cast<void>(::write(master, confirmation.data(), confirmation.size()));
                }
            }
        } else if (decision == "wrong") {
            auto challenge = challenge_from_tui_output(result.output);
            if (!challenge.empty()) {
                challenge[0] = challenge[0] == '0' ? '1' : '0';
                write(approval_confirmation(challenge) + "\n");
            }
        } else if (decision == "typed-no") {
            // The header can reach the PTY before ncurses has drawn the active
            // input row. Synchronize on the modeled screen, not the byte stream.
            while (!screen.input_prompt_visible()
                   && std::chrono::steady_clock::now() < dialog_deadline) {
                read_for(std::chrono::milliseconds(20));
            }
            // The exact prompt label is emitted only by draw_approval. The PTY
            // screen model then checks the edited value after ncurses redraws it.
            result.approval_prompt_ready = screen.input_prompt_visible();
            if (result.approval_prompt_ready) mark_event("approval Input prompt emitted");
            if (result.approval_prompt_ready) {
                result.approval_prompt_emitted_before_n = screen.input_prompt_visible();
                mark_event("inject n");
                write("n");
                result.approval_prompt_emitted_before_o = screen.input_prompt_visible();
                mark_event("inject o");
                write("o");
                // Evaluate modeled terminal cells, including cursor moves and erases.
                const auto rendered_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                while (!screen.input_is_no_visible()
                       && std::chrono::steady_clock::now() < rendered_deadline) {
                    read_for(std::chrono::milliseconds(20));
                }
                result.dialog_kept_typed_no = screen.input_is_no_visible();
                if (result.dialog_kept_typed_no) mark_event("Input line shows no before Enter");
                result.failure_screen_input_line = screen.safe_input_row();
                result.failure_recent_controls = screen.recent_controls();
                result.no_tool_request_before_typed_no_enter = server.received_bodies().size() == 1;
                result.no_file_before_typed_no_enter =
                    !std::filesystem::exists(working_directory / "workspace/nested/denied.txt");
                write("\n");
                const auto followup_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
                while (std::chrono::steady_clock::now() < followup_deadline
                       && result.output.find("TUI_FOLLOWUP_DONE") == std::string::npos) {
                    read_for(std::chrono::milliseconds(20));
                }
                read_for(std::chrono::milliseconds(160));
                write("\033[20~"); // Probe that approval characters were not left in the composer.
            } else {
                result.failure_screen_input_line = screen.safe_input_row();
                result.failure_recent_controls = screen.recent_controls();
                write("\033[21~"); // Fail closed if the approval prompt never became visible.
            }
        } else if (decision == "partial-close") {
            write("y");
            write("e");
            write("s");
            write("\033[21~");
        } else if (decision == "mouse-only") {
            const std::string click = "\033[<0;20;10M\033[<0;20;10m";
            write(click);
            result.no_file_before_typed_no_enter =
                !std::filesystem::exists(working_directory / "workspace/nested/denied.txt");
            write("\033[21~");
            close_while_pending = true;
        } else if (decision == "expire-typed") {
            write("y");
            write("e");
            write("s");
            read_for(std::chrono::milliseconds(700));
            result.expiry_notice_seen = result.output.find("Approval expired; no file was created.") != std::string::npos;
            write("x");
            write("\n"); // Dismiss the expired modal; queued approval characters must stay out of the composer.
            const auto followup_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
            while (std::chrono::steady_clock::now() < followup_deadline
                   && result.output.find("TUI_FOLLOWUP_DONE") == std::string::npos) {
                read_for(std::chrono::milliseconds(20));
            }
            read_for(std::chrono::milliseconds(160));
            write("\033[20~");
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
    capture_mouse_reporting_transitions(result);
    result.writer_error_seen = result.output.find("Tool failed: write_file") != std::string::npos;
    if (decision == "typed-no" && (!result.approval_prompt_ready
                                    || !result.dialog_kept_typed_no
                                    || !result.approval_prompt_emitted_before_n
                                    || !result.approval_prompt_emitted_before_o)) {
        std::cerr << "[PTY typed-no failure trace] screen_rows=24 cols=80"
                  << " approval_header_visible=" << (screen.approval_header_visible() ? "yes" : "no")
                  << " approval_prompt_ready=" << (result.approval_prompt_ready ? "yes" : "no")
                  << " Input_emitted_before_n=" << (result.approval_prompt_emitted_before_n ? "yes" : "no")
                  << " Input_emitted_before_o=" << (result.approval_prompt_emitted_before_o ? "yes" : "no")
                  << " model_input_visible_before_n=" << (screen.input_prompt_visible() ? "yes" : "no")
                  << " input_screen=\"" << result.failure_screen_input_line << "\""
                  << " raw_input_marker=" << (result.output.find("Input: ") != std::string::npos ? "yes" : "no")
                  << " printable_bytes=" << screen.printable_bytes_seen()
                  << " screen_summary=\"" << screen.safe_screen_summary() << "\""
                  << " requests_before_enter=" << (result.no_tool_request_before_typed_no_enter ? 1 : 0)
                  << " target_absent_before_enter=" << (result.no_file_before_typed_no_enter ? "yes" : "no")
                  << "\n[PTY timing ms]";
        for (const auto& [event, elapsed] : result.pty_timeline_ms)
            std::cerr << ' ' << event << '=' << elapsed;
        std::cerr << "\n[PTY recent CSI] " << result.failure_recent_controls << '\n';
    }
    (void)::close(master);
    return result;
}
}

TEST(tui3, typed_no_is_retained_until_enter_without_leaking_to_composer)
{
    tests::TempDir working_directory("tui_write_typed_no");
    MockHttpServer server;
    MockHttpServer::Response call;
    call.body = sse_write_call("call-pty-typed-no", "nested/denied.txt", "must not be written");
    server.enqueue(std::move(call));
    MockHttpServer::Response answer;
    answer.body = sse_delta("TUI_FOLLOWUP_DONE") + "data: [DONE]\n\n";
    server.enqueue(std::move(answer));

    const auto run = run_tui_write_decision(server, working_directory.path(), "typed-no", false);
    CHECK(run.exited);
    CHECK(run.dialog_seen);
    CHECK(run.approval_prompt_ready);
    CHECK(run.approval_prompt_emitted_before_n);
    CHECK(run.approval_prompt_emitted_before_o);
    CHECK(run.dialog_kept_typed_no);
    CHECK(run.mouse_reporting_disabled);
    CHECK(run.mouse_reporting_restored);
    CHECK(run.no_tool_request_before_typed_no_enter);
    CHECK(run.no_file_before_typed_no_enter);
    CHECK(!std::filesystem::exists(working_directory.path() / "workspace/nested"));
    CHECK_EQ(server.received_bodies().size(), std::size_t(2));
}

TEST(tui3, pty_screen_oracle_applies_split_ncurses_cursor_updates)
{
    PtyScreen screen;
    screen.feed("\033[24;");
    screen.feed("1HInput: ");
    CHECK(screen.input_prompt_visible());
    CHECK(!screen.input_is_no_visible());
    screen.feed("\033[24;8");
    screen.feed("Hno\033[?25h");
    CHECK(screen.input_prompt_visible());
    CHECK(screen.input_is_no_visible());
    CHECK_EQ(screen.safe_input_row(), std::string("Input: no"));
    screen.feed("t");
    CHECK(!screen.input_is_no_visible());
}

TEST(tui3, pty_screen_ignores_split_charset_designators)
{
    PtyScreen screen;
    screen.feed("\033[24;1HInput: \033(");
    CHECK(screen.input_prompt_visible());

    // ESC ( B is split between independent PTY reads.
    screen.feed("Bn\033");
    screen.feed("(Bo\033(");
    screen.feed("B");

    CHECK(screen.input_prompt_visible());
    CHECK(screen.input_is_no_visible());
    CHECK_EQ(screen.safe_input_row(), std::string("Input: no"));
}

TEST(tui3, multiline_composer_keeps_cursor_and_visible_content_inside_its_viewport)
{
    const std::string prompt = "Utilise write_file pour créer uniquement le fichier\n"
        "preuve-accord.txt avec exactement ce contenu :\n\nACCORD_V1_2026\n\nNe crée aucun autre fichier.";
    agent::ui::TuiView view;
    const auto at_start = draw_tui_screen(view, prompt, 0, 20, 60);
    CHECK_EQ(at_start.rows.size(), std::size_t(20));
    CHECK(at_start.cursor_row >= 17);
    CHECK(at_start.cursor_row <= 19);
    CHECK(at_start.rows[17].find("Utilise write_file") != std::string::npos);

    const auto at_end = draw_tui_screen(view, prompt, prompt.size(), 20, 60);
    CHECK(at_end.cursor_row >= 17);
    CHECK(at_end.cursor_row <= 19);
    CHECK(at_end.rows[static_cast<std::size_t>(at_end.cursor_row)].find("Ne cr") != std::string::npos);
    const auto logical_end = view.rendered_composer(prompt, prompt.size(), 47, 3);
    CHECK(!logical_end.rows.empty());
    CHECK_EQ(logical_end.rows.back(), std::string("Ne crée aucun autre fichier."));
    for (int row = 4; row < 16; ++row) {
        CHECK(at_end.rows[static_cast<std::size_t>(row)].find("Utilise write_file") == std::string::npos);
        CHECK(at_end.rows[static_cast<std::size_t>(row)].find("Ne crée aucun autre fichier.") == std::string::npos);
    }
}

TEST(tui4a, lofiterm_layout_supports_reference_sizes_and_reports_smaller_terminals)
{
    CHECK(agent::ui::TuiView::layout_supported(40, 120));
    CHECK(agent::ui::TuiView::layout_supported(30, 100));
    CHECK(agent::ui::TuiView::layout_supported(24, 80));
    CHECK(agent::ui::TuiView::layout_supported(20, 60));
    CHECK(!agent::ui::TuiView::layout_supported(19, 60));
    CHECK(!agent::ui::TuiView::layout_supported(20, 59));

    agent::ui::TuiView view;
    for (const auto& [rows, columns] : {std::pair{40, 120}, std::pair{30, 100},
                                        std::pair{24, 80}, std::pair{20, 60}}) {
        const auto screen = draw_tui_screen(view, "draft café", 10, rows, columns);
        CHECK_EQ(screen.rows.size(), static_cast<std::size_t>(rows));
        CHECK(screen.rows[0].find("LOFITERM") != std::string::npos);
        CHECK(screen.rows[1].find("F1") != std::string::npos);
        CHECK(screen.rows[1].find("F6") != std::string::npos);
        CHECK(screen.rows[3].find("CONVERSATION") != std::string::npos);
    }
    const auto too_small = draw_tui_screen(view, "draft retained", 14, 19, 59);
    CHECK(too_small.rows[0].find("LOFITERM") != std::string::npos);
    CHECK(too_small.rows[2].find("Terminal too small") != std::string::npos);
    CHECK(too_small.rows[3].find("60 columns x 20") != std::string::npos);
    const auto tiny = draw_tui_screen(view, "draft retained", 14, 4, 10);
    CHECK_EQ(tiny.rows.size(), std::size_t(4));
    CHECK(tiny.rows[0].find("LOFITERM") != std::string::npos);
    CHECK(tiny.rows[1].find("MIN 60x20") != std::string::npos);
    CHECK(tiny.rows[2].find("TOO SMALL") != std::string::npos);
    CHECK(tiny.rows[3].find("F10 QUIT") != std::string::npos);
}

TEST(tui4a, all_six_pages_show_their_active_navigation_and_section_title)
{
    agent::ui::TuiView view;
    const std::array<std::string, 6> titles = {
        "CONVERSATION", "TOOLS", "ACTIVITY", "DIAGNOSTICS", "CONFIGURATION", "HELP"
    };
    const std::array<std::string, 6> selected = {
        "[F1 CHAT]", "[F2 TOOL]", "[F3 ACTI]", "[F4 DIAG]", "[F5 CONF]", "[F6 HELP]"
    };
    for (int i = 0; i < 6; ++i) {
        const auto screen = draw_tui_screen(view, "draft retained", 15, 24, 80,
            static_cast<agent::ui::TuiPage>(i));
        CHECK(screen.rows[0].find("LOFITERM") != std::string::npos);
        CHECK(screen.rows[1].find(selected[static_cast<std::size_t>(i)]) != std::string::npos);
        CHECK(screen.rows[3].find(titles[static_cast<std::size_t>(i)]) != std::string::npos);
    }
}

TEST(ux11, focus_redraw_keeps_conversation_title_style_unchanged)
{
    agent::ui::TuiView view;
    const auto baseline = draw_tui_screen(view, "text remains unchanged", 0, 24, 80,
        agent::ui::TuiPage::conversation, false, agent::ui::TuiFocus::composer);
    const auto after_content_focus = draw_tui_screen(view, "text remains unchanged", 0, 24, 80,
        agent::ui::TuiPage::conversation, false, agent::ui::TuiFocus::composer,
        agent::ui::TuiFocus::content);
    CHECK_EQ(after_content_focus.rows[3], baseline.rows[3]);
    CHECK(after_content_focus.title_attributes == baseline.title_attributes);
    CHECK(after_content_focus.first_content_attributes == baseline.first_content_attributes);
}

TEST(tui4a, partial_is_explicit_and_bold_without_color)
{
    agent::ui::TuiView view;
    agent::ui::TurnMetrics metrics;
    metrics.event_details_complete = false;
    view.set_metrics(std::move(metrics));
    const auto screen = draw_tui_screen(view, {}, 0, 24, 80, agent::ui::TuiPage::metrics, true);
    CHECK(screen.rows[2].find("PARTIAL") != std::string::npos);
    CHECK(screen.partial_bold);
}

TEST(tui4a, tools_and_metrics_projection_is_visible_in_the_panel_layout)
{
    agent::ui::TuiView view;
    agent::ui::TuiViewData data;
    data.tools.push_back({"read_file", "Read a workspace file"});
    view.set_view_data(std::move(data));
    const auto tools = draw_tui_screen(view, {}, 0, 24, 80, agent::ui::TuiPage::tools);
    CHECK(tools.rows[3].find("TOOLS") != std::string::npos);
    CHECK(tools.rows[4].find("Declared tools:") != std::string::npos);
    CHECK(tools.rows[5].find("read_file") != std::string::npos);

    agent::ui::TurnMetrics metrics;
    metrics.turn_id = agent::ui::TurnId{7};
    metrics.event_details_complete = false;
    metrics.http_requests_started = 1;
    view.set_metrics(std::move(metrics));
    const auto activity = draw_tui_screen(view, {}, 0, 24, 80, agent::ui::TuiPage::metrics, true);
    CHECK(activity.rows[3].find("ACTIVITY") != std::string::npos);
    CHECK(activity.rows[2].find("PARTIAL") != std::string::npos);
    CHECK(activity.rows[4].find("Observed metrics") != std::string::npos);
    CHECK(activity.rows[5].find("PARTIAL:") != std::string::npos);
    CHECK(activity.rows[6].find("Turn ID: 7") != std::string::npos);
}

TEST(tui4a, f9_cannot_submit_from_a_terminal_below_the_minimum_layout)
{
    tests::TempDir working_directory("tui_too_small_no_submit");
    MockHttpServer server;
    int master = -1;
    winsize size{};
    size.ws_row = 19;
    size.ws_col = 59;
    const pid_t child = ::forkpty(&master, nullptr, nullptr, &size);
    CHECK(child >= 0);
    if (child == 0) {
        (void)::chdir(working_directory.path().c_str());
        const auto executable = std::filesystem::read_symlink("/proc/self/exe").parent_path() / "tui_pty_driver";
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("LC_ALL", "C.UTF-8", 1);
        ::execl(executable.c_str(), executable.c_str(), server.url("/v1/chat/completions").c_str(), "normal", nullptr);
        _exit(127);
    }
    std::string output;
    const auto drain = [&](std::chrono::milliseconds duration) {
        const auto deadline = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < deadline) {
            pollfd descriptor{master, POLLIN, 0};
            if (::poll(&descriptor, 1, 20) > 0 && (descriptor.revents & POLLIN)) {
                char bytes[2048];
                const auto count = ::read(master, bytes, sizeof(bytes));
                if (count > 0) output.append(bytes, static_cast<std::size_t>(count));
            }
        }
    };
    drain(std::chrono::milliseconds(150));
    static_cast<void>(::write(master, "\033[20~", 5));
    drain(std::chrono::milliseconds(100));
    CHECK(output.find("LOFITERM") != std::string::npos);
    CHECK(output.find("Terminal too small") != std::string::npos);
    CHECK(server.received_bodies().empty());
    static_cast<void>(::write(master, "\033[21~", 5));
    int status = 0;
    bool exited = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        if (::waitpid(child, &status, WNOHANG) == child) { exited = true; break; }
        drain(std::chrono::milliseconds(20));
    }
    if (!exited) {
        static_cast<void>(::kill(child, SIGKILL));
        (void)::waitpid(child, &status, 0);
    }
    (void)::close(master);
    CHECK(exited);
    if (exited) CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(server.received_bodies().empty());
}

TEST(tui3, composer_vertical_navigation_tracks_wrapped_and_blank_visual_rows)
{
    agent::ui::TuiView view;
    const std::string input = "abcdefghij\n\nXYZ";
    CHECK_EQ(view.move_composer_cursor_vertical(input, 8, -1, 3, 6), std::size_t(3));
    CHECK_EQ(view.move_composer_cursor_vertical(input, 8, 1, 3, 6), std::size_t(11));
    CHECK_EQ(view.move_composer_cursor_vertical(input, 11, 1, 3, 6), std::size_t(15));
    CHECK_EQ(view.move_composer_cursor_to_row_edge(input, 13, false, 6), std::size_t(12));
    CHECK_EQ(view.move_composer_cursor_to_row_edge(input, 12, true, 6), std::size_t(15));
    CHECK_EQ(view.move_composer_cursor_vertical(input, 0, -1, 0, 6), std::size_t(0));
    CHECK_EQ(view.move_composer_cursor_vertical(input, input.size(), 1, 3, 6), input.size());
}

TEST(tui3, composer_wrap_respects_terminal_cell_width_and_keeps_utf8_text)
{
    const char* old_locale_value = std::setlocale(LC_ALL, nullptr);
    const std::string old_locale = old_locale_value ? old_locale_value : "C";
    static_cast<void>(std::setlocale(LC_ALL, "C.UTF-8"));
    agent::ui::TuiView view;
    const std::string input = "Aé界BC";
    const auto layout = view.rendered_composer(input, input.size(), 6, 4);
    CHECK_EQ(layout.rows.size(), std::size_t(2));
    CHECK_EQ(layout.rows[0], std::string("Aé界B"));
    CHECK_EQ(layout.rows[1], std::string("C"));
    CHECK_EQ(view.move_composer_cursor_to_row_edge(input, input.size(), false, 6), std::size_t(7));
    CHECK_EQ(view.move_composer_cursor_to_row_edge(input, 7, true, 6), input.size());
    static_cast<void>(std::setlocale(LC_ALL, old_locale.c_str()));
}

TEST(tui3, pasted_multiline_prompt_survives_cursor_navigation_resize_and_f9_exactly)
{
    tests::TempDir working_directory("tui_multiline_prompt_exact");
    MockHttpServer server;
    MockHttpServer::Response response;
    response.body = sse_delta("prompt received") + "data: [DONE]\n\n";
    server.enqueue(std::move(response));

    const std::string prompt = "Utilise write_file pour créer uniquement le fichier\n"
        "preuve-accord.txt avec exactement ce contenu :\n\nACCORD_V1_2026\n\nNe crée aucun autre fichier.";
    int master = -1;
    winsize size{}; size.ws_row = 24; size.ws_col = 80;
    const pid_t child = ::forkpty(&master, nullptr, nullptr, &size);
    CHECK(child >= 0);
    if (child == 0) {
        (void)::chdir(working_directory.path().c_str());
        const auto executable = std::filesystem::read_symlink("/proc/self/exe").parent_path() / "tui_pty_driver";
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("LC_ALL", "C.UTF-8", 1);
        ::execl(executable.c_str(), executable.c_str(), server.url("/v1/chat/completions").c_str(), "normal", nullptr);
        _exit(127);
    }

    std::string output;
    auto drain = [&] (std::chrono::milliseconds duration) {
        const auto deadline = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < deadline) {
            pollfd descriptor{master, POLLIN, 0};
            if (::poll(&descriptor, 1, 20) > 0 && (descriptor.revents & POLLIN)) {
                char bytes[4096];
                const auto count = ::read(master, bytes, sizeof(bytes));
                if (count > 0) output.append(bytes, static_cast<std::size_t>(count));
            }
        }
    };
    drain(std::chrono::milliseconds(200));
    static_cast<void>(::write(master, prompt.data(), prompt.size())); // terminal paste, including blank lines
    static_cast<void>(::write(master, "\033OH\033OF", 6)); // xterm Home then End; no text should be inserted
    size.ws_row = 20; size.ws_col = 60;
    static_cast<void>(::ioctl(master, TIOCSWINSZ, &size));
    static_cast<void>(::kill(child, SIGWINCH));
    drain(std::chrono::milliseconds(80));
    static_cast<void>(::write(master, "\033[20~", 5)); // F9

    bool sent_close = false;
    bool exited = false;
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
    while (std::chrono::steady_clock::now() < deadline) {
        drain(std::chrono::milliseconds(30));
        if (!sent_close && !server.received_bodies().empty()) {
            static_cast<void>(::write(master, "\033[21~", 5)); // F10
            sent_close = true;
        }
        if (::waitpid(child, &status, WNOHANG) == child) { exited = true; break; }
    }
    if (sent_close && !exited) {
        const auto close_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < close_deadline) {
            if (::waitpid(child, &status, WNOHANG) == child) { exited = true; break; }
            drain(std::chrono::milliseconds(20));
        }
    }
    if (!exited) {
        static_cast<void>(::kill(child, SIGKILL));
        (void)::waitpid(child, &status, 0);
        CHECK(false);
    }
    (void)::close(master);
    CHECK(WIFEXITED(status));
    if (WIFEXITED(status)) CHECK_EQ(WEXITSTATUS(status), 0);
    CHECK(sent_close);
    const auto bodies = server.received_bodies();
    CHECK_EQ(bodies.size(), std::size_t(1));
    if (bodies.size() == 1) {
        const auto payload = nlohmann::json::parse(bodies.front());
        CHECK_EQ(payload.at("messages").at(0).at("content").get<std::string>(), prompt);
    }
}

TEST(tui3, typed_confirmation_approves_only_after_enter)
{
    tests::TempDir working_directory("tui_write_typed_approval");
    MockHttpServer server;
    MockHttpServer::Response call;
    call.body = sse_write_call("call-pty-typed-approve", "nested/typed.txt", "typed approval body");
    server.enqueue(std::move(call));
    MockHttpServer::Response answer;
    answer.body = sse_delta("typed approval completed") + "data: [DONE]\n\n";
    server.enqueue(std::move(answer));

    const auto run = run_tui_write_decision(server, working_directory.path(), "typed-approve", false);
    CHECK(run.exited);
    CHECK(run.dialog_seen);
    CHECK(run.mouse_reporting_disabled);
    CHECK(run.mouse_reporting_restored);
    const auto file = working_directory.path() / "workspace/nested/typed.txt";
    CHECK(std::filesystem::exists(file));
    if (std::filesystem::exists(file)) {
        std::ifstream in(file, std::ios::binary);
        CHECK_EQ(std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()),
                 std::string("typed approval body"));
    }
}

TEST(tui3, rapid_confirmation_paste_is_accepted)
{
    tests::TempDir working_directory("tui_write_fast_approval");
    MockHttpServer server;
    MockHttpServer::Response call;
    call.body = sse_write_call("call-pty-fast-approve", "nested/fast.txt", "fast approval body");
    server.enqueue(std::move(call));
    MockHttpServer::Response answer;
    answer.body = sse_delta("fast approval completed") + "data: [DONE]\n\n";
    server.enqueue(std::move(answer));

    const auto run = run_tui_write_decision(server, working_directory.path(), "fast-approve", false);
    CHECK(run.exited);
    CHECK(run.dialog_seen);
    CHECK(std::filesystem::exists(working_directory.path() / "workspace/nested/fast.txt"));
}

TEST(tui3, partial_confirmation_then_f10_cancels_without_creation)
{
    tests::TempDir working_directory("tui_write_partial_f10");
    MockHttpServer server;
    MockHttpServer::Response call;
    call.body = sse_write_call("call-pty-partial-f10", "nested/partial.txt", "must not be written");
    server.enqueue(std::move(call));
    MockHttpServer::Response answer;
    answer.body = sse_delta("partial confirmation cancelled") + "data: [DONE]\n\n";
    server.enqueue(std::move(answer));

    const auto run = run_tui_write_decision(server, working_directory.path(), "partial-close", false);
    CHECK(run.exited);
    CHECK(run.dialog_seen);
    CHECK(run.mouse_reporting_disabled);
    CHECK(run.mouse_reporting_restored);
    CHECK(!std::filesystem::exists(working_directory.path() / "workspace/nested"));
    CHECK_EQ(server.received_bodies().size(), std::size_t(2));
}

TEST(tui3, expiry_during_partial_confirmation_quarantines_input)
{
    tests::TempDir working_directory("tui_write_expired_input");
    MockHttpServer server;
    MockHttpServer::Response call;
    call.body = sse_write_call("call-pty-expiry-input", "nested/expired.txt", "must not be written");
    server.enqueue(std::move(call));
    MockHttpServer::Response answer;
    answer.body = sse_delta("TUI_FOLLOWUP_DONE") + "data: [DONE]\n\n";
    server.enqueue(std::move(answer));

    const auto run = run_tui_write_decision(server, working_directory.path(), "expire-typed", false);
    CHECK(run.exited);
    CHECK(run.dialog_seen);
    CHECK(run.expiry_notice_seen);
    CHECK(run.mouse_reporting_disabled);
    CHECK(run.mouse_reporting_restored);
    CHECK(!std::filesystem::exists(working_directory.path() / "workspace/nested"));
    CHECK_EQ(server.received_bodies().size(), std::size_t(2));
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
          == std::vector<std::string>({"LOFITERM Bot: complete", "              answer"}));
}

TEST(tui3, multiline_messages_label_only_the_first_physical_line)
{
    agent::ui::TuiView view;
    view.add_user_message("first\n\n  third\n");
    view.append_assistant_delta("reply\nsecond");

    CHECK(view.rendered_conversation_rows(80) == std::vector<std::string>({
        "You: first", "     ", "       third", "     ",
        "LOFITERM Bot: reply", "              second",
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
          == std::vector<std::string>({"LOFITERM Bot: partial", "              ", "              ", "              [incomplete]"}));
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
          == std::vector<std::string>({"LOFITERM Bot: hello", "              world"}));
}

TEST(tui3, wrapped_continuations_are_indented_under_the_message_text)
{
    agent::ui::TuiView view;
    view.append_assistant_delta("abcdefghijklm");

    const auto rows = view.rendered_conversation_rows(12);
    CHECK(rows.front().starts_with("Bot: "));
    std::string reconstructed = rows.front().substr(5);
    for (std::size_t i = 1; i < rows.size(); ++i) {
        CHECK(rows[i].starts_with("     "));
        reconstructed += rows[i].substr(5);
    }
    CHECK_EQ(reconstructed, "abcdefghijklm");
    CHECK_EQ(view.messages().back().text, std::string("abcdefghijklm"));
}

TEST(tui3, resize_during_streaming_reflows_without_changing_message_text)
{
    agent::ui::TuiView view;
    view.on_resize(40, 10);
    view.append_assistant_delta("alpha beta gamma");
    const auto before = view.messages().back().text;
    CHECK(view.rendered_conversation_rows(40) == std::vector<std::string>({"LOFITERM Bot: alpha beta gamma"}));

    view.on_resize(20, 4);
    view.append_assistant_delta(" delta");
    const auto rows = view.rendered_conversation_rows(20);
    CHECK(rows.front().starts_with("LOFITERM Bot: "));
    std::string reconstructed = rows.front().substr(std::string("LOFITERM Bot: ").size());
    for (std::size_t i = 1; i < rows.size(); ++i) {
        CHECK(rows[i].starts_with("              "));
        reconstructed += rows[i].substr(14);
    }
    CHECK_EQ(reconstructed, "alpha beta gamma delta");
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
        "You: question one", "LOFITERM Bot: answer one",
        "You: question two", "LOFITERM Bot: answer two",
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

TEST(ux11, mouse_click_cannot_approve_write_request)
{
    tests::TempDir working_directory("tui_write_mouse_denied");
    MockHttpServer server;
    MockHttpServer::Response call;
    call.body = sse_write_call("call-pty-mouse", "nested/denied.txt", "must not be written");
    server.enqueue(std::move(call));
    MockHttpServer::Response answer;
    answer.body = sse_delta("write cancelled") + "data: [DONE]\n\n";
    server.enqueue(std::move(answer));

    const auto run = run_tui_write_decision(server, working_directory.path(), "mouse-only", false);
    CHECK(run.exited);
    CHECK(WIFEXITED(run.status));
    CHECK_EQ(WEXITSTATUS(run.status), 0);
    CHECK(run.dialog_seen);
    CHECK(run.no_file_before_typed_no_enter);
    CHECK(!std::filesystem::exists(working_directory.path() / "workspace/nested"));
    CHECK_EQ(server.received_bodies().size(), std::size_t(2));
}

TEST(ux11, approval_error_after_explicit_confirmation_restores_mouse_reporting)
{
    tests::TempDir working_directory("tui_write_approval_writer_error");
    MockHttpServer server;
    MockHttpServer::Response call;
    call.body = sse_write_call("call-pty-existing-target", "nested/denied.txt", "must not overwrite");
    server.enqueue(std::move(call));
    MockHttpServer::Response answer;
    answer.body = sse_delta("write error handled") + "data: [DONE]\n\n";
    server.enqueue(std::move(answer));

    const auto run = run_tui_write_decision(server, working_directory.path(), "approve-existing", false);
    CHECK(run.exited);
    CHECK(run.dialog_seen);
    CHECK(run.mouse_reporting_disabled);
    CHECK(run.mouse_reporting_restored);
    CHECK(run.writer_error_seen);
    std::ifstream existing(working_directory.path() / "workspace/nested/denied.txt", std::ios::binary);
    CHECK_EQ(std::string(std::istreambuf_iterator<char>(existing), std::istreambuf_iterator<char>()),
             std::string("preserve-existing-content"));
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
        CHECK(run.mouse_reporting_disabled);
        CHECK(run.mouse_reporting_restored);
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
    CHECK(run.mouse_reporting_disabled);
    CHECK(run.mouse_reporting_restored);
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
