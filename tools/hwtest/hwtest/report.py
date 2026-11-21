"""Terminal output, the interactive prompts and the run log."""

from __future__ import annotations

import os
import select
import sys
import time
from collections.abc import Callable
from typing import TextIO

TAIL_LINES = 15


class Console:
    def __init__(
        self,
        log: TextIO | None = None,
        color: bool | None = None,
        out: TextIO | None = None,
    ):
        self.out = out or sys.stdout
        self.log = log
        self.color = self.out.isatty() if color is None else color
        self._interactive = sys.stdin.isatty()
        self._line_open = False

    # ----------------------------------------------------------- helpers
    def _paint(self, text: str, code: str) -> str:
        return f"\033[{code}m{text}\033[0m" if self.color else text

    def _write(self, text: str) -> None:
        self.out.write(text)
        self.out.flush()

    def _log(self, text: str) -> None:
        if self.log:
            self.log.write(text)
            self.log.flush()

    def _end_line(self) -> None:
        if self._line_open:
            self._write("\n")
            self._line_open = False

    # --------------------------------------------------------- UI protocol
    def info(self, text: str) -> None:
        self._end_line()
        self._write(text + "\n")
        self._log(text + "\n")

    def step_start(self, index: int, total: int, name: str, root: bool) -> None:
        self._end_line()
        tag = self._paint(" [root]", "35") if root else ""
        self._write(f"[{index}/{total}] {name}{tag} ... ")
        self._line_open = True
        self._log(f"\n=== [{index}/{total}] {name}{' [root]' if root else ''}\n")

    def step_end(self, status: str, detail: str) -> None:
        colors = {"ok": "32", "fail": "31", "skip": "33"}
        label = self._paint(status, colors.get(status, "0"))
        self._write(f"{label}{(' (' + detail + ')') if detail else ''}\n")
        self._line_open = False
        self._log(f"--- {status} {detail}\n")

    def chunk(self, text: str) -> None:
        self._end_line()
        self._write(text)
        self._log(text)

    def output(self, text: str) -> None:
        """Command output of a failed step: logged in full, last lines shown."""
        self._log(text if text.endswith("\n") else text + "\n")
        lines = text.rstrip("\n").splitlines()
        shown = lines[-TAIL_LINES:]
        if len(lines) > len(shown):
            self._write(
                f"    ... ({len(lines) - len(shown)} earlier lines in the log)\n"
            )
        for line in shown:
            self._write(f"    | {line}\n")

    def _read_enter(self, wait: float) -> bool:
        ready, _, _ = select.select([sys.stdin], [], [], wait)
        if ready:
            sys.stdin.readline()
            return True
        return False

    def prompt(
        self, text: str, probe: Callable[[], bool] | None, timeout: float
    ) -> str:
        """Tell the user what to do, then wait for Enter or for the probe to turn true."""
        self._end_line()
        width = min(max((len(line) for line in text.splitlines()), default=0) + 4, 78)
        bar = "=" * width
        hint = (
            "press Enter when done"
            if probe is None
            else "continuing automatically once it is detected (Enter to continue now)"
        )
        block = (
            f"\n{bar}\n  DO THIS NOW:\n\n"
            + "".join(f"  {line}\n" for line in text.splitlines())
            + f"\n  ({hint})\n{bar}\n"
        )
        self._write(self._paint(block, "1;36"))
        self._log(block)
        if probe is None and not self._interactive:
            self.info("no terminal to ask on; cannot wait for a person")
            return "abort"
        deadline = time.monotonic() + timeout
        while True:
            if probe is not None and probe():
                return "done"
            if time.monotonic() >= deadline:
                return "timeout"
            if self._interactive and self._read_enter(
                1.0 if probe else min(1.0, deadline - time.monotonic())
            ):
                return "done"
            if not self._interactive:
                time.sleep(1.0)

    def choose(self, question: str, options: str) -> str:
        """Ask for one of the letters in `options`; without a terminal, the last one."""
        if not self._interactive:
            return options[-1]
        while True:
            self._write(f"{question} [{'/'.join(options)}]: ")
            answer = sys.stdin.readline().strip().lower()[:1]
            if answer in options:
                return answer

    # ------------------------------------------------------------ summary
    def summary(self, outcomes, log_path: str | None) -> None:
        self._end_line()
        passed = sum(o.status == "ok" for o in outcomes)
        failed = [o for o in outcomes if o.status == "fail"]
        skipped = sum(o.status == "skip" for o in outcomes)
        head = (
            self._paint("PASSED", "1;32")
            if not failed
            else self._paint("FAILED", "1;31")
        )
        self._write(f"\n{head}: {passed} ok, {len(failed)} failed, {skipped} skipped\n")
        for outcome in failed:
            self._write(f"  - {outcome.name}: {outcome.detail}\n")
        if log_path:
            self._write(f"Full log: {os.path.relpath(log_path)}\n")
            if failed:
                self._write("If you need help, send that log file.\n")
