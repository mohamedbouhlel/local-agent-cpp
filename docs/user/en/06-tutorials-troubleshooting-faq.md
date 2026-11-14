# Tutorials, troubleshooting, and FAQ

[🇫🇷 Français](../fr/06-tutoriels-depannage-faq.md) · 🇬🇧 English · [🇪🇸 Español](../es/06-tutoriales-solucion-problemas-faq.md)

## Tutorial: ask about a file

1. Put a non-sensitive copy of the file in the effective workspace.
2. Ask the model to read that relative path and summarize a specific point.
3. Check the path if the model calls `read_file`.
4. Compare the answer with the source file.

The read limit is 50,000 bytes; this tool does not support binary files.

## Tutorial: find a phrase

Put relevant text files in the workspace, then request a literal search for the phrase. `search_files` returns up to 50 matching lines; for a large corpus, narrow the workspace or query.

## Tutorial: create a new file

Ask for one file at a relative path and describe the intended content. When approval appears, compare the target and each line with your request. Refuse if the target already exists or the displayed request differs. Never reuse a previous challenge.

## Troubleshooting

| Symptom | Check |
|---|---|
| `Could not contact LLM_BASE_URL` or HTTP failure | Check the full URL, port, server, model, and authentication. Do not paste secrets into shared logs. |
| Stream starts and then fails | Check server logs; SSE must end with `[DONE]`, without protocol errors or truncation. |
| `write_file` unavailable in CLI | Approval requires interactive stdin and stderr. Pipes and missing TTY fail closed. |
| Files cannot be found | Check the effective workspace: launch directory for source builds, XDG data path for the package. |
| TUI is unreadable or keys do not work | Use an interactive UTF-8 terminal with ncursesw support; try a larger window. Mouse capabilities depend on the terminal. |
| Startup refused | Check kernel Landlock support and container restrictions. LOFITERM does not run unconfined. |

## FAQ

**Is LOFITERM the executable name?** Not in a source build: the public product name is LOFITERM, the source executable is `agent`, and the local package provides the `lofiterm` command.

**Can tools execute shell commands?** No shell tool is declared.

**Can I ask it to overwrite a file?** No. `overwrite:true` and existing targets are refused.

**Does F3 show tokens per second?** No. It reports observed bytes, SSE frames, text fragments, and durations without treating them as tokens.

**Are conversations saved after exit?** Conversation history is in process memory and is not restored at the next launch. Files created in the workspace remain on disk.

**Can the package be redistributed?**

LOFITERM's original code is distributed under the 0BSD license, which permits redistribution. The repository contains a locally qualified Debian 13 amd64 package recipe, but currently publishes neither an official release nor a public APT repository. Any binary redistribution must comply with third-party dependency licenses and notices and receive appropriate qualification.

Open the [documentation index](../../README.md) for the other languages and guides.
