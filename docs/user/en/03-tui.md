# LOFITERM TUI

[🇫🇷 Français](../fr/03-interface-tui.md) · 🇬🇧 English · [🇪🇸 Español](../es/03-tui.md)

The ncursesw TUI is a full-screen interface for an interactive terminal. It uses the same agent, transport, and tool policies as the classic CLI.

## Six views

| Key | View | Content |
|---|---|---|
| F1 | Conversation | Transcript, streaming, and composer. |
| F2 | Tools | Declared tools and activity actually observed. |
| F3 | Activity | HTTP/SSE counters, durations, text fragments, and retained tool details. |
| F4 | Diagnostics | Observed errors and diagnostics. |
| F5 | Configuration | Read-only available values; secrets are masked. |
| F6 | Help | Getting started, shortcuts, About, and limits. |

Views display available information and do not invent progress. F3 states that text fragments are not tokens and displays `PARTIAL` when event details are incomplete.

## Keyboard and mouse

- F1–F6 select views.
- Tab changes focus. Escape closes an open subsection or returns to Conversation.
- Arrow keys move the composer cursor or menu selection according to focus.
- Enter inserts a newline when the composer is focused; F9 submits once from Conversation.
- Page Up/Page Down scroll the current view. F10 requests exit.
- `M` toggles Markdown/plain text when Conversation content has focus.
- Clicking a tab selects its view; clicking the composer focuses it; F5/F6 subsection clicks open or select a section. The wheel scrolls the active content.

Native text selection depends on the terminal; Ghostty may require Shift-drag. In `WRITE APPROVAL`, challenge entry is separate from the composer. No click or navigation shortcut grants write approval.

## Markdown, code, and Unicode

Responses use a safe Markdown subset (headings, lists, quotes, emphasis, code, and textual links). Links are not opened, HTML is not interpreted, and model-supplied ANSI sequences are not executed. `M` toggles formatted and plain text. Closed code fences can receive light syntax styling for C++, Python, Bash, JSON, YAML, and diff; unfinished fences remain provisional. Appearance depends on terminal capabilities, font, and locale. Universal Unicode conformance is not claimed.

## Small terminals and exit

The layout supports compact dimensions and reports when the terminal is too small to submit. Resize before F9 if prompted. F10 requests shutdown; an active network/tool turn may finish before the application exits. ncurses session cleanup restores terminal modes, subject to terminal behavior.

## Reference capture

![Conversation captured from the real binary in a tmux PTY](../../assets/screenshots/tui-f1-pty-capture.svg)

This no-data capture is generated from the actual ncurses grid emitted by the binary at the SHA documented in the README. It is not a Ghostty screenshot or physical qualification. Other views are in the [README](../../../README.en.md#tui-preview).

See [getting started](01-getting-started.md), [tools](04-workspace-tools.md), and [security](05-security-and-limits.md).
