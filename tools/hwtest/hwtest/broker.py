"""One polkit prompt per run: a root helper driven over a pipe.

`pkexec` asks for authorisation on every invocation, so instead of wrapping each
command the runner starts a single helper (`run --broker`) through pkexec and
sends it all the privileged commands. The helper reads JSON lines from its
inherited stdin and exits on EOF; there is no socket and nothing outlives the
run, and only the process holding the pipe's other end can talk to it.

Requests:   {"id": n, "argv": [...], "cwd": "...", "timeout": seconds}
Responses:  {"hello": uid} once, then per request {"id": n, "chunk": "..."}*
            and finally {"id": n, "rc": code, "timed_out": bool}.
Only argv lists are accepted, never a shell string.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from collections.abc import Callable

from .process import Result, stream_process


class BrokerError(Exception):
    """The root helper could not be started or stopped answering."""


class BadRequest(ValueError):
    """A request the helper refuses to run."""


def _validate(request: object) -> tuple[list[str], str, float]:
    if not isinstance(request, dict):
        raise BadRequest("request must be an object")
    argv = request.get("argv")
    if (
        not isinstance(argv, list)
        or not argv
        or not all(isinstance(a, str) for a in argv)
    ):
        raise BadRequest("argv must be a non-empty list of strings")
    cwd = request.get("cwd") or "/"
    timeout = request.get("timeout", 60)
    if (
        not isinstance(cwd, str)
        or isinstance(timeout, bool)
        or not isinstance(timeout, (int, float))
    ):
        raise BadRequest("bad cwd or timeout")
    return argv, cwd, float(timeout)


def _send(out, message: dict) -> None:
    out.write(json.dumps(message) + "\n")
    out.flush()


def _execute(
    out, request_id: int | None, argv: list[str], cwd: str, timeout: float
) -> None:
    result = stream_process(
        argv, cwd, timeout, lambda text: _send(out, {"id": request_id, "chunk": text})
    )
    _send(out, {"id": request_id, "rc": result.rc, "timed_out": result.timed_out})


def serve(stdin=None, stdout=None, require_root: bool = True) -> int:
    """The helper's main loop (the `--broker` side)."""
    stdin = stdin or sys.stdin
    stdout = stdout or sys.stdout
    if require_root and os.geteuid() != 0:
        print("broker: must run as root (through pkexec)", file=sys.stderr)
        return 2
    _send(stdout, {"hello": int(os.environ.get("PKEXEC_UID", "-1"))})
    for line in stdin:
        request_id = None
        try:
            request = json.loads(line)  # JSONDecodeError is a ValueError
            if isinstance(request, dict):
                request_id = request.get("id")
            argv, cwd, timeout = _validate(request)
        except ValueError as exc:
            _send(stdout, {"id": request_id, "chunk": f"refused: {exc}\n"})
            _send(stdout, {"id": request_id, "rc": 126, "timed_out": False})
            continue
        _execute(stdout, request_id, argv, cwd, timeout)
    return 0


class RootBroker:
    """Parent side: starts the helper through pkexec and runs commands on it."""

    def __init__(self, command: list[str] | None = None, expect_uid: int | None = None):
        self._command = command
        self._expect_uid = os.getuid() if expect_uid is None else expect_uid
        self._proc: subprocess.Popen | None = None
        self._next_id = 0

    def _launch_command(self) -> list[str]:
        if self._command is not None:
            return self._command
        if shutil.which("pkexec") is None:
            raise BrokerError(
                "pkexec (polkit) is not installed; cannot run the root steps"
            )
        entry = os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "run"
        )
        return ["pkexec", sys.executable, entry, "--broker"]

    def start(self) -> None:
        if self._proc is not None:
            return
        try:
            self._proc = subprocess.Popen(
                self._launch_command(),
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                text=True,
                bufsize=1,
            )
        except OSError as exc:
            raise BrokerError(f"cannot start the root helper: {exc.strerror}") from exc
        hello = self._read()
        if hello is None:
            self.close()
            raise BrokerError("authorisation was refused or cancelled (polkit)")
        if hello.get("hello") != self._expect_uid:
            self.close()
            raise BrokerError(
                "the root helper was started for a different user; refusing to use it"
            )

    def _read(self) -> dict | None:
        line = self._proc.stdout.readline()
        return json.loads(line) if line else None

    def run(
        self,
        argv: list[str],
        cwd: str,
        timeout: float,
        on_chunk: Callable[[str], None] | None = None,
    ) -> Result:
        self.start()
        self._next_id += 1
        request_id = self._next_id
        try:
            self._proc.stdin.write(
                json.dumps(
                    {"id": request_id, "argv": argv, "cwd": cwd, "timeout": timeout}
                )
                + "\n"
            )
            self._proc.stdin.flush()
        except OSError as exc:
            raise BrokerError("the root helper is gone") from exc
        output: list[str] = []
        while True:
            message = self._read()
            if message is None:
                raise BrokerError("the root helper stopped answering")
            if message.get("id") != request_id:
                continue
            if "chunk" in message:
                output.append(message["chunk"])
                if on_chunk:
                    on_chunk(message["chunk"])
            if "rc" in message:
                return Result(
                    message["rc"], "".join(output), bool(message.get("timed_out"))
                )

    def close(self) -> None:
        if self._proc is None:
            return
        try:
            self._proc.stdin.close()
        except OSError:
            pass
        try:
            self._proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self._proc.kill()
            self._proc.wait()
        self._proc.stdout.close()
        self._proc = None
