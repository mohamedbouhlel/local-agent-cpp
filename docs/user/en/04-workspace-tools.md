# Workspace tools

[🇫🇷 Français](../fr/04-outils-workspace.md) · 🇬🇧 English · [🇪🇸 Español](../es/04-herramientas-workspace.md)

The agent declares four functions to the LLM server. The model decides when to call them during a turn; they are not shell commands in the REPL.

| Tool | Purpose | Limits visible in the code |
|---|---|---|
| `list_files` | Recursively list non-hidden files. | No coded entry-count cap on this output. Keep the workspace focused. |
| `read_file` | Read a text file by relative path. | Reads at most 50,000 bytes; binary and multiply hard-linked files are refused. |
| `search_files` | Search text files for a literal string. | Returns at most 50 matching lines; an individual line has no size cap. |
| `write_file` | Create a new text file and missing parent directories. | Content capped at 200,000 bytes; explicit approval for each call; existing targets and `overwrite:true` are refused. |

Paths must be relative to the workspace. Absolute paths and `..` components are refused. Traversal skips hidden names and avoids symbolic links; concurrent changes by another process under the same user remain a limitation (see [security](05-security-and-limits.md)).

## Approving a creation

The request shows the effective workspace, relative target, byte count, missing parents, and complete content in a reversible escaped form. In the CLI, enter exactly `yes <displayed challenge>` followed by Enter. In the TUI, enter the same full challenge in the dialog.

Before approval:

1. Check the workspace and target path.
2. Read the displayed content, including escaped control characters.
3. Approve only this exact request. Every call gets a fresh challenge.
4. Deny or cancel if anything is unexpected. No decision, EOF, or expiration fails closed.

Parent directories are created only after approval. Approval applies to one creation; it cannot authorize a different path, different content, or overwrite.

## A safer request example

Ask the model for one file at a specific relative path, limit the expected files, and describe the required content. This helps constrain the result but does not replace inspection: the model can interpret instructions unexpectedly. Refuse any request outside the stated scope.

The activity history in F2 and metrics in F3 are not guaranteed complete logs; event loss can make details partial. They do not retain sensitive tool arguments or results as tool history.
