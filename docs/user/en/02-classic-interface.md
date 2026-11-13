# Classic interface and REPL

[🇫🇷 Français](../fr/02-interface-classique.md) · 🇬🇧 English · [🇪🇸 Español](../es/02-interfaz-clasica.md)

The classic interface reads terminal lines, sends messages to the configured server, and writes the progressive response to standard output. Diagnostics are kept on `stderr`.

## Launch

From the directory containing `build/agent`:

```sh
./build/agent --ui=classic
```

Without an option, classic mode is selected. For the local package, use `lofiterm --ui=classic`. A source build uses a workspace relative to the current working directory; the package uses the XDG workspace described in the [getting-started guide](01-getting-started.md).

## Single-line exchanges

- Type a line and press Enter to submit it.
- Empty lines are ignored.
- The response is displayed as fragments arrive.
- Type exact `exit` then Enter to quit; Ctrl-D also ends input.
- HTTP, SSE, or agent errors are reported as failures. Partial data is not presented as a validated final assistant response.

## Multiline messages

Use the exact markers:

```text
> :multi
... First instruction.
...
... Last line.
... :end
```

`:multi` and `:end` must each be the entire line. The block is sent as one turn. Spaces, tabs, and blank lines are preserved; CRLF is normalized to LF. No extra final newline is appended. An empty block is not sent. EOF before `:end` abandons the block without submission. `exit` inside a block is content. Therefore, an exact `:end` line cannot be included as block content.

## Tool calls

The model may decide to call workspace tools during a response. The CLI shows a tool indicator and a summarized result; there are no dedicated tool commands to enter. `write_file` pauses the response and presents an approval request. Inspect the path and full content before entering the exact challenge.

## Output and scripts

Standard output contains the user-facing response; errors and diagnostics go to `stderr`. The CLI is not a JSON format or stable machine API. For automated checks, use the C++ components and mock tests rather than parsing display text.

Also see [workspace tools](04-workspace-tools.md), [security](05-security-and-limits.md), and the [FAQ](06-tutorials-troubleshooting-faq.md).
