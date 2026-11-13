#!/usr/bin/env python3
"""PTY smoke tests against an installed /usr/bin/lofiterm and local SSE mock."""

import fcntl
import http.server
import json
import os
import pathlib
import pty
import re
import select
import signal
import subprocess
import struct
import tempfile
import termios
import threading
import time


FINAL_TEXT = "LOFITERM_PACKAGE_SMOKE_DONE"
READ_TEXT = "read-only package fixture\n"
WRITE_TEXT = "approved package fixture\n"


def sse_tool_call(call_id, name, arguments):
    call = {
        "index": 0,
        "id": call_id,
        "type": "function",
        "function": {"name": name, "arguments": json.dumps(arguments, separators=(",", ":"))},
    }
    delta = {"choices": [{"index": 0, "delta": {"tool_calls": [call]}}]}
    finish = {"choices": [{"index": 0, "delta": {}, "finish_reason": "tool_calls"}]}
    return f"data: {json.dumps(delta)}\n\ndata: {json.dumps(finish)}\n\ndata: [DONE]\n\n"


def sse_text(text):
    delta = {"choices": [{"index": 0, "delta": {"content": text}}]}
    finish = {"choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}]}
    return f"data: {json.dumps(delta)}\n\ndata: {json.dumps(finish)}\n\ndata: [DONE]\n\n"


def strip_terminal_controls(data):
    data = re.sub(rb"\x1b\[[0-?]*[ -/]*[@-~]", b"", data)
    data = re.sub(rb"\x1b\][^\x07]*(?:\x07|\x1b\\)", b"", data)
    data = re.sub(rb"\x1b\([0-2A-Z]", b"", data)
    return data.replace(b"\r", b"")


class MockState:
    def __init__(self, deny=False):
        self.deny = deny
        self.requests = []
        self.lock = threading.Lock()

    def response(self, body):
        with self.lock:
            self.requests.append(body)
            count = len(self.requests)
        if self.deny:
            if count == 1:
                return sse_tool_call("package-deny", "write_file", {
                    "path": "nested/denied.txt", "content": WRITE_TEXT,
                })
            return sse_text(FINAL_TEXT)
        if count == 1:
            return sse_tool_call("package-read", "read_file", {"path": "source.txt"})
        if count == 2:
            return sse_tool_call("package-write", "write_file", {
                "path": "nested/approved.txt", "content": WRITE_TEXT,
            })
        return sse_text(FINAL_TEXT)


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_POST(self):
        size = int(self.headers.get("Content-Length", "0"))
        body = json.loads(self.rfile.read(size))
        payload = self.server.state.response(body).encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, _format, *_args):
        pass


def read_until(master, buffer, pattern, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if pattern(buffer):
            return True
        ready, _, _ = select.select([master], [], [], 0.1)
        if not ready:
            continue
        try:
            chunk = os.read(master, 8192)
        except OSError:
            return False
        if not chunk:
            return False
        buffer.extend(chunk)
    return pattern(buffer)


def wait_child(pid, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        waited, status = os.waitpid(pid, os.WNOHANG)
        if waited == pid:
            return os.waitstatus_to_exitcode(status)
        time.sleep(0.02)
    os.kill(pid, signal.SIGKILL)
    _, status = os.waitpid(pid, 0)
    return os.waitstatus_to_exitcode(status)


def run_classic(deny):
    state = MockState(deny)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.state = state
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    with tempfile.TemporaryDirectory(prefix="lofiterm-deb-smoke-") as temp:
        base = pathlib.Path(temp)
        home = base / "home"
        config_home = home / ".config"
        data_home = home / ".local" / "share"
        app_data = data_home / "lofiterm"
        away = base / "unrelated-cwd"
        (config_home / "lofiterm").mkdir(parents=True)
        away.mkdir()
        config = config_home / "lofiterm" / "config"
        config.write_text(
            f"LLM_BASE_URL=http://127.0.0.1:{server.server_port}/v1/chat/completions\n"
            "LLM_MODEL=package-smoke-model\n"
            "LLM_API_KEY=not-a-secret-smoke-token\n"
            "LLM_TIMEOUT_SECONDS=10\n",
            encoding="utf-8",
        )
        config.chmod(0o600)
        workspace = app_data / "workspace"
        workspace.mkdir(parents=True)
        if not deny:
            (workspace / "source.txt").write_text(READ_TEXT, encoding="utf-8")

        pid, master = pty.fork()
        if pid == 0:
            os.chdir(away)
            env = os.environ.copy()
            env.update({
                "HOME": str(home),
                "XDG_CONFIG_HOME": str(config_home),
                "XDG_DATA_HOME": str(data_home),
                "TERM": "xterm-256color",
                "LC_ALL": "C.UTF-8",
            })
            os.execve("/usr/bin/lofiterm", ["lofiterm", "--ui=classic"], env)

        buffer = bytearray()
        try:
            if not read_until(master, buffer, lambda b: b"agent-cpp ready" in strip_terminal_controls(b), 5):
                raise RuntimeError("classic interface did not reach its prompt")
            os.write(master, b"package smoke\n")
            challenge_pattern = re.compile(rb"Confirmation: type exactly 'yes ([0-9a-f]{32})'")
            if not read_until(master, buffer,
                              lambda b: challenge_pattern.search(strip_terminal_controls(b)) is not None, 8):
                raise RuntimeError("write approval challenge was not displayed")
            visible = strip_terminal_controls(buffer)
            challenge = challenge_pattern.search(visible).group(1).decode()
            target = workspace / ("nested/denied.txt" if deny else "nested/approved.txt")
            if target.parent.exists() or target.exists():
                raise RuntimeError("write side effect occurred before the user decision")
            os.write(master, ("no\n" if deny else f"yes {challenge}\n").encode())
            if not read_until(master, buffer,
                              lambda b: FINAL_TEXT.encode() in strip_terminal_controls(b), 10):
                raise RuntimeError("mock model did not complete the turn")
            os.write(master, b"exit\n")
            code = wait_child(pid, 5)
            pid = -1
            if code != 0:
                raise RuntimeError(f"classic application exited with status {code}")

            if deny:
                if target.exists() or target.parent.exists():
                    raise RuntimeError("denied write created a file or parent directory")
                tool_result = state.requests[-1]["messages"][-1]
                if tool_result.get("role") != "tool" or "authorization denied; no file was created" not in tool_result.get("content", ""):
                    raise RuntimeError("denial result was not sent to the mock model")
                print("CLI explicit denial: PASS; no target or parent was created")
            else:
                if target.read_text(encoding="utf-8") != WRITE_TEXT:
                    raise RuntimeError("approved file content differs from the requested bytes")
                observed = {p.relative_to(workspace).as_posix() for p in workspace.rglob("*")}
                if observed != {"source.txt", "nested", "nested/approved.txt"}:
                    raise RuntimeError(f"unexpected workspace contents: {sorted(observed)}")
                if len(state.requests) != 3:
                    raise RuntimeError("read, write and final-response requests were not all received")
                read_result = state.requests[1]["messages"][-1]
                write_result = state.requests[2]["messages"][-1]
                if read_result.get("role") != "tool" or read_result.get("content") != READ_TEXT:
                    raise RuntimeError("read_file result was not passed exactly to the model")
                if write_result.get("role") != "tool" or "created new file" not in write_result.get("content", ""):
                    raise RuntimeError("successful write result was not passed to the model")
                print("CLI read_file + approved write_file: PASS; exact file content and tool results verified")
            if (away / "workspace").exists():
                raise RuntimeError("launch from an unrelated directory created a workspace there")
            print("XDG workspace path and arbitrary-CWD isolation: PASS")
        finally:
            if pid > 0:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            os.close(master)
    server.shutdown()
    server.server_close()
    thread.join(timeout=2)


def run_tui_exit():
    with tempfile.TemporaryDirectory(prefix="lofiterm-tui-smoke-") as temp:
        base = pathlib.Path(temp)
        home = base / "home"
        config_home = home / ".config"
        data_home = home / ".local" / "share"
        away = base / "unrelated-cwd"
        config_home.mkdir(parents=True)
        away.mkdir()
        pid, master = pty.fork()
        if pid == 0:
            os.chdir(away)
            env = os.environ.copy()
            env.update({"HOME": str(home), "XDG_CONFIG_HOME": str(config_home),
                        "XDG_DATA_HOME": str(data_home), "TERM": "xterm-256color",
                        "LC_ALL": "C.UTF-8"})
            os.execve("/usr/bin/lofiterm", ["lofiterm", "--ui=tui"], env)
        fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
        buffer = bytearray()
        try:
            if not read_until(master, buffer, lambda b: b"LOFITERM" in strip_terminal_controls(b), 5):
                raise RuntimeError("TUI did not render its branded screen")
            os.write(master, b"\x1b[21~")
            code = wait_child(pid, 5)
            pid = -1
            if code != 0:
                raise RuntimeError(f"TUI exited with status {code}")
            if (data_home / "lofiterm" / "workspace").is_dir():
                print("TUI launch and F10 close: PASS; user workspace path created")
            else:
                raise RuntimeError("TUI did not use the XDG workspace path")
            if (away / "workspace").exists():
                raise RuntimeError("TUI launch from unrelated CWD created a workspace there")
        finally:
            if pid > 0:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            os.close(master)


def run_private_config_checks():
    with tempfile.TemporaryDirectory(prefix="lofiterm-config-smoke-") as temp:
        base = pathlib.Path(temp)
        home = base / "home"
        config_home = home / ".config"
        data_home = home / ".local" / "share"
        away = base / "unrelated-cwd"
        (config_home / "lofiterm").mkdir(parents=True)
        away.mkdir()
        config = config_home / "lofiterm" / "config"
        config.write_text("LLM_API_KEY=not-a-secret\n", encoding="utf-8")
        config.chmod(0o644)
        env = os.environ.copy()
        env.update({"HOME": str(home), "XDG_CONFIG_HOME": str(config_home),
                    "XDG_DATA_HOME": str(data_home), "TERM": "xterm-256color"})
        result = subprocess.run(["/usr/bin/lofiterm", "--ui=classic"], cwd=away,
                                env=env, input=b"", capture_output=True, timeout=5)
        if result.returncode != 2 or b"chmod 600" not in result.stderr:
            raise RuntimeError("package launcher did not reject group-readable configuration")
        if (data_home / "lofiterm").exists() or (away / "workspace").exists():
            raise RuntimeError("invalid configuration caused workspace creation")
        print("private user-config permission check: PASS; rejected before workspace creation")


if __name__ == "__main__":
    run_private_config_checks()
    run_classic(deny=False)
    run_classic(deny=True)
    run_tui_exit()
