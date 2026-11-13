# Getting started, installation, and configuration

[🇫🇷 Français](../fr/01-demarrage-installation.md) · 🇬🇧 English · [🇪🇸 Español](../es/01-primeros-pasos.md)

## Requirements

- Linux with usable Landlock support; otherwise the application refuses to start.
- CMake 3.20 or newer and a C++20 compiler.
- libcurl and ncursesw to build the application.
- Network access during CMake configuration to fetch `nlohmann/json` 3.11.3 if it is not already available.
- An LLM server compatible with OpenAI Chat Completions and SSE streaming.

## Build from source

On Debian/Ubuntu, install system dependencies and build:

```sh
sudo apt install build-essential cmake libcurl4-openssl-dev libncurses-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

This creates `build/agent`; it is not a system installation. There is no `make install` target.

## Set up the environment

The source-built executable reads environment variables only. To use a local file:

```sh
cp .env.example .env
chmod 600 .env
# Edit .env in a local editor.
set -a; . ./.env; set +a
```

Do not copy the API key into an issue, screenshot, or shared shell history. `.env` is sourced by your shell; the program does not parse it.

## First run without creating a workspace in the repository

For a source build, `workspace/` is created relative to the process's current working directory. Run from a dedicated user directory:

```sh
repo_dir=$PWD
mkdir -p "$HOME/lofiterm-run"
cd "$HOME/lofiterm-run"
"$repo_dir/build/agent" --ui=tui
```

Use `--ui=classic` for the classic CLI; it is also the default. The agent creates `~/lofiterm-run/workspace` when needed, keeping test data separate from the source tree.

## Install the local Debian package

A local Debian 13 amd64 package recipe is documented in [`docs/INSTALL_DEBIAN13.md`](../../INSTALL_DEBIAN13.md). The installed package provides `/usr/bin/lofiterm`; install it with `sudo apt install ./lofiterm_1.0.0-1_amd64.deb`. This is neither an APT repository nor a package approved for public redistribution: the source tree does not declare a redistribution license.

The package separates the application from user data: configuration is under `${XDG_CONFIG_HOME:-~/.config}/lofiterm/config`, and the workspace under `${XDG_DATA_HOME:-~/.local/share}/lofiterm/workspace`. `LOFITERM_CONFIG_FILE` and `LOFITERM_DATA_DIR` accept absolute paths to move these locations. The launcher changes into the data directory before starting the agent, so the caller's working directory does not determine the package workspace.

## LLM server configuration

| Variable | Default | Meaning |
|---|---|---|
| `LLM_BASE_URL` | `http://localhost:8080/v1/chat/completions` | Full endpoint URL. |
| `LLM_MODEL` | `local-model` | Model identifier expected by the server. |
| `LLM_API_KEY` | empty | Optional Bearer key. |
| `LLM_CONNECT_TIMEOUT_SECONDS` | 10 s (or `min(LLM_TIMEOUT_SECONDS,10)`) | Connection timeout. |
| `LLM_IDLE_TIMEOUT_SECONDS` | normally 60 s | Maximum idle period between response data. |
| `LLM_TOTAL_TIMEOUT_SECONDS` | 300 s | Maximum total duration. |
| `LLM_TIMEOUT_SECONDS` | 60 s | Compatibility fallback for executable idle timeout. |

The three current timeouts are independent; values must be positive integers. The client requires a complete SSE stream according to its contract and reports incomplete streams as failures.

The package config file has a simple `NAME=VALUE` grammar and allows four keys: `LLM_BASE_URL`, `LLM_MODEL`, `LLM_API_KEY`, `LLM_TIMEOUT_SECONDS`. Environment variables take precedence. The example is installed at `/usr/share/doc/lofiterm/lofiterm.conf.example`; copy it with private permissions as described by the Debian manual.

## Check the server before use

Start with a question that does not request file operations. If the server fails, verify the endpoint, model, and server-side authentication. Not every OpenAI-compatible provider has been qualified by this repository; SSE and tool-calling behavior can differ.

Next: [classic interface](02-classic-interface.md), [TUI](03-tui.md), and the [documentation index](../../README.md).
