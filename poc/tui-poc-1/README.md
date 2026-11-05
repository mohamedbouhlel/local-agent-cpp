# TUI-POC-1: FTXUI / ncursesw

Two standalone C++20 prototypes. Neither is linked to `agent_core`; both use
synthetic text events only. Builds live outside the checkout.

## Dependencies and build

* FTXUI v7.0.3, pinned to commit
  `f921fad208912747c17d129a8ef75ec7624b6eec`, fetched by the isolated CMake
  project. No product CMake or system installation is changed.
* Debian ncursesw 6.5 (`libncursesw6`, development headers and `libtinfo`),
  discovered with CMake `FindCurses`; configure requests wide-character mode.
* C++20, GCC, CMake, pthreads, Python 3 with `pty`, `termios`, `fcntl`.

```sh
cmake -S poc/tui-poc-1/poc-ftxui -B /tmp/tui-poc-ftxui-build
cmake --build /tmp/tui-poc-ftxui-build -j2
cmake -S poc/tui-poc-1/poc-ncursesw -B /tmp/tui-poc-ncurses-build
cmake --build /tmp/tui-poc-ncurses-build -j2
python3 poc/tui-poc-1/harness/test_pocs.py \
  /tmp/tui-poc-ftxui-build/poc-ftxui \
  /tmp/tui-poc-ncurses-build/poc-ncursesw \
  /tmp/tui-poc-ftxui-build/sanitizer-tests \
  /tmp/tui-poc-ncurses-build/sanitizer-tests
```

The first configure needs network access to clone the exact FTXUI commit. For
offline rebuilds, point CMake `FETCHCONTENT_SOURCE_DIR_FTXUI` at a checkout of
that commit. The harness uses a PTY, `/proc` sampling and a private pipe for
machine-readable acknowledgements; those acknowledgements are not rendered
pixels or end-to-end UI latency.

## Prototype controls

F1–F6 select the six named views; Tab switches focus between navigation and
the composer; arrows navigate or move the editing cursor according to focus;
PageUp/PageDown scroll the conversation while navigation has focus. Enter adds
a newline, F9 submits the buffer, F10/Escape exits, and F12 throws the
controlled failure.
This keymap exists only for this POC. A synthetic 1,000-delta stream runs at
100 deltas/s. `--consumer-pause-ms=2000` injects the overload scenario.
`TERM=dumb` and non-TTY use are refused explicitly.

The UI panels are deliberately skeletal. This is an experiment, not the final
application interface. See the durable comparison report for measured results
and untested cases.
