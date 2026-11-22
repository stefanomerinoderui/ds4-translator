"""Run one command in its own process group with a deadline and streamed output."""

from __future__ import annotations

import os
import select
import signal
import subprocess
import time
from collections.abc import Callable
from dataclasses import dataclass


@dataclass
class Result:
    rc: int
    output: str
    timed_out: bool = False


def stream_process(
    argv: list[str], cwd: str, timeout: float, on_chunk: Callable[[str], None]
) -> Result:
    """Run argv (never through a shell), merging stderr into stdout.

    A command that exceeds `timeout` is killed with its whole process group.
    A command that cannot be started yields rc 127 and says why in the output.
    """
    chunks: list[str] = []

    def emit(text: str) -> None:
        chunks.append(text)
        on_chunk(text)

    try:
        proc = subprocess.Popen(
            argv,
            cwd=cwd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
    except OSError as exc:
        emit(f"cannot run {argv[0]}: {exc.strerror}\n")
        return Result(127, "".join(chunks))

    deadline = time.monotonic() + timeout
    timed_out = False
    fd = proc.stdout.fileno()
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            timed_out = True
            os.killpg(proc.pid, signal.SIGKILL)
            break
        ready, _, _ = select.select([fd], [], [], min(remaining, 0.5))
        if ready:
            data = os.read(fd, 4096)
            if not data:
                break
            emit(data.decode(errors="replace"))
    proc.wait()
    proc.stdout.close()
    return Result(proc.returncode, "".join(chunks), timed_out)
