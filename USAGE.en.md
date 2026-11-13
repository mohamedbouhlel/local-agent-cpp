# User manual — LOFITERM

[🇫🇷 Français](USAGE.md) · 🇬🇧 English · [🇪🇸 Español](USAGE.es.md)

This manual describes behavior verified in the documentation branch at SHA `39646ffade1c71ece9995fde7770dcf428e45d3a`. Manual trials and qualification of other SHAs remain archived separately and are not claimed as rerun here.

## 1. Choose an interface

From a source build, set the repository directory and launch from a dedicated user workspace:

```sh
cd /path/to/local-agent-cpp
repo_dir=$PWD
mkdir -p "$HOME/lofiterm-workspace"
cd "$HOME/lofiterm-workspace"
"$repo_dir/build/agent" --ui=classic
"$repo_dir/build/agent" --ui=tui
```

Without an option, the classic interface starts. The compiled executable is named `agent`; LOFITERM is the public interface name.

With the local Debian 13 package, run `lofiterm --ui=classic` or `lofiterm --ui=tui`. Its launcher places the process in an XDG user workspace; this differs from the source-build `workspace/` relative to the launch directory. See [Debian 13 installation](docs/INSTALL_DEBIAN13.md).

## 2. Configuration

The source-built executable reads environment variables through `std::getenv()`. It does not read `.env` itself.

```sh
repo_dir=$PWD
cp .env.example .env
chmod 600 .env
# Edit .env without sharing its key.
set -a; . ./.env; set +a
mkdir -p "$HOME/lofiterm-workspace"
cd "$HOME/lofiterm-workspace"
"$repo_dir/build/agent" --ui=tui
```

From a source build, the workspace is `workspace/` below the process current working directory. Launch from this user directory to reuse the same workspace.

| Variable | Executable default | Purpose |
|---|---:|---|
| `LLM_BASE_URL` | `http://localhost:8080/v1/chat/completions` | OpenAI-compatible Chat Completions HTTP endpoint. |
| `LLM_MODEL` | `local-model` | Model identifier sent to the server. |
| `LLM_API_KEY` | empty | If set, sent in a Bearer authorization header. |
| `LLM_CONNECT_TIMEOUT_SECONDS` | `min(LLM_TIMEOUT_SECONDS, 10)`; normally 10 | Connection-establishment limit. |
| `LLM_IDLE_TIMEOUT_SECONDS` | `LLM_TIMEOUT_SECONDS`; normally 60 | Maximum time without response headers or body bytes; received data resets it. |
| `LLM_TOTAL_TIMEOUT_SECONDS` | `300` | Hard total request-duration limit. |
| `LLM_TIMEOUT_SECONDS` | `60` | Legacy variable, used as the executable's idle-timeout fallback. |

Values must be positive integers. Invalid values cause a warning on `stderr` and use the executable's fallback. Connection, idle, and total timeouts are distinct. An active stream may exceed 60 seconds, but remains bounded by the total timeout.

**Debian package:** its user configuration is a `NAME=VALUE` file, with no shell syntax, expansion, or quoting; default path is `~/.config/lofiterm/config`. The installed example exposes `LLM_BASE_URL`, `LLM_MODEL`, `LLM_API_KEY`, and `LLM_TIMEOUT_SECONDS`. Environment variables take precedence. See the [configuration guide](docs/user/en/01-getting-started.md#llm-server-configuration).

## 3. Classic interface and REPL

The classic mode shows a `>` prompt, sends a non-empty line after Enter, and displays the response progressively. Exact `exit` quits the REPL; Ctrl-D also ends input. Empty lines do not trigger requests.

To send a block:

```text
> :multi
... Summarize the note files.
... Keep filenames in the answer.
... :end
```

`:multi` and `:end` must each be the entire line. The block is sent once; blank lines, spaces, and tabs are preserved, with CRLF normalized to LF. No extra final newline is added. An empty block is not sent; a block containing blank lines is. EOF before `:end` abandons the block without sending. Inside a block, `exit` is text.

## 4. LOFITERM TUI

Run the TUI from an interactive terminal. F1–F6 select the six views:

| Key | View | Information |
|---|---|---|
| F1 | Conversation | Transcript, streaming, and multiline composer. |
| F2 | Tools | Declared tools and observed activity. |
| F3 | Activity | Observed HTTP, SSE, text-fragment, and tool measurements for the current/last turn. |
| F4 | Diagnostics | Observed errors and diagnostics. |
| F5 | Configuration | Available values in read-only form; API key is masked. |
| F6 | Help | Getting started, keyboard, limits, and available version details. |

F9 sends once from Conversation; F10 requests exit. Tab changes focus; Escape returns to Conversation or closes a subsection; arrows and Page Up/Page Down navigate according to focus. Enter inserts a newline when the composer is focused. `M` toggles Markdown/plain text when Conversation content is focused. Tab/subsection clicks and the mouse wheel work when the terminal reports those events. Selecting text may require Shift-drag, depending on the terminal.

F3 does not show token throughput. `Text fragments` counts delta fragments, not tokens; SSE data frames and HTTP bytes are separate counters. `PARTIAL` means event details are incomplete; it does not prove task success.

## 5. Workspace tools and approval

The model can call these tools:

- `list_files`: recursively lists non-hidden files; the current code has no entry-count cap for this output.
- `read_file`: reads a text file relative to the workspace, up to 50,000 bytes; binary and multiply hard-linked files are refused.
- `search_files`: searches for a literal string and returns up to 50 matching lines. An individual line has no length cap. Hidden, special, symbolic, and multiply hard-linked files are skipped/refused according to the traversal.
- `write_file`: creates a missing file after explicit approval. Content is limited to 200,000 bytes; overwriting is disabled.

Tools are called by the model during a conversation; they are not manual CLI commands. Read operations do not ask for approval.

Before a creation, the interface shows the workspace, relative path, size, missing parent directories, and complete content in a reversible escaped representation. In the CLI, type the full `yes <current challenge>` and press Enter; any other line denies. In the TUI, enter the same challenge in the dialog. Each challenge belongs to one request. Approve only if the path and content match your intent. Denial, EOF, cancellation, expiry, or unavailability does not authorize a write. Parent directories are created only after approval.

## 6. Confinement and limits

On Linux, Landlock is enabled before the interface starts. Without kernel support or sufficient permission, startup fails; the program does not fall back to an unconfined mode. Process access is restricted to the workspace and the system read paths documented in [AGENTS.md](AGENTS.md). This does not isolate the process from other processes running as the same user. Concurrent workspace changes by the same UID are outside Landlock's guarantee. Writes are not promised durable across a crash (`fsync` is not used).

Conversation context is in memory for the lifetime of the process. Older complete turns are removed beyond the reference limit of 20 messages, without summarization; a large current turn may exceed that limit. There is no persistent cross-session memory or durable application cache.

## 7. Errors and troubleshooting

- **Cannot connect:** check `LLM_BASE_URL`, server availability, and timeouts. The client reports a transport error; do not paste an API key into an issue.
- **Truncated response or incomplete SSE:** missing `[DONE]`, malformed SSE JSON, or `finish_reason=length` fails the turn. Provisional text may remain marked incomplete; no incomplete assistant message is accepted as a final answer.
- **Landlock denied:** check the kernel and container execution policy; the application does not switch to an unconfined mode.
- **Tool result is unexpected:** check the relative path and effective workspace. Source builds use the launch directory; the package uses XDG data paths.
- **Creation denied:** this means no valid approval was received. Do not reuse an earlier confirmation; a new request gets a new challenge.

See [Troubleshooting and FAQ](docs/user/en/06-tutorials-troubleshooting-faq.md) and the [documentation index](docs/README.md) for all language versions.
