# Security and limitations

[🇫🇷 Français](../fr/05-securite-limites.md) · 🇬🇧 English · [🇪🇸 Español](../es/05-seguridad-y-limites.md)

## Linux confinement

Before the REPL starts, the application enables Landlock and refuses startup if the kernel or permissions do not allow it. In the workspace, the process receives the filesystem access needed for the intended reading, writing, creation, and removal operations; execution is denied. A small set of certificate and name-resolution paths is readable for TLS/network operation. See the normative policy in [`AGENTS.md`](../../../AGENTS.md).

Landlock restricts **this process**; it does not isolate it from another process running as the same UID. A same-user process can concurrently change workspace files. The application's safeguards are not a boundary against a hostile local account or peer process.

## Consent for writes

Every `write_file` creation requires an exact per-request challenge after the complete content is shown. Denial, unavailability, expiration, EOF, or shutdown cancels the operation. Overwriting is disabled. On Linux, publication uses an anonymous temporary file and no-replace publication when supported; it does not silently fall back to a weaker named temporary file. Since `fsync` is not used, a confirmed creation is not guaranteed to survive a crash or power loss.

Approval cannot prevent a poorly informed user decision. Inspect the target, parent directories, and content; do not approve solely because the model requested it.

## Data and privacy

- The API key comes from the environment or private package configuration and is not displayed in F5.
- Metrics retain bounded counters and identifiers; they do not retain conversation text, tool arguments, or tool results as metrics history.
- Content sent to the LLM is subject to the server and network policies. A local model does not automatically mean the server keeps no logs.
- Conversation history is in process memory; source-build workspace files remain on disk in the chosen location. LOFITERM has no persistent cross-session model memory.
- The repository does not declare a redistribution license. The Debian package is for local installation/evaluation, not public distribution approval.

## Established operational limits

File reads are capped at 50,000 bytes; search returns 50 lines, but a single very long line can still make a large result. `list_files` has no coded entry-count cap. Conversation context is limited by message count and trimmed by complete turns, not by a tokenizer or token budget. A large turn can exceed the reference limit.

Landlock, no-overwrite behavior, and per-request consent reduce risk, but do not prove LLM answers are correct, server handling is safe, or filesystem concurrency is absent. See [dated reports](../../README.md#reports) for qualification limits and their SHAs.
