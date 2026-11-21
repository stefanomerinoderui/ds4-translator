"""Executes a scenario: the step types and their control flow.

Everything the executor touches from the outside (running commands, talking to
the user, the clock) is injected, so the control flow is unit-tested without
hardware, root or a terminal.
"""

from __future__ import annotations

import re
import time
from collections.abc import Callable
from dataclasses import dataclass
from typing import Protocol

from .broker import RootBroker
from .process import Result, stream_process
from .scenario import Scenario, Step, count_steps, substitute

PROBE_TIMEOUT = 15.0


class Runner(Protocol):
    def run(
        self,
        argv: list[str],
        root: bool,
        cwd: str,
        timeout: float,
        on_chunk: Callable[[str], None],
    ) -> Result: ...


class UI(Protocol):
    def step_start(self, index: int, total: int, name: str, root: bool) -> None: ...
    def step_end(self, status: str, detail: str) -> None: ...
    def chunk(self, text: str) -> None: ...
    def info(self, text: str) -> None: ...
    def output(self, text: str) -> None: ...
    def prompt(
        self, text: str, probe: Callable[[], bool] | None, timeout: float
    ) -> str: ...
    def choose(self, question: str, options: str) -> str: ...


class CommandRunner:
    """Runs user steps directly and root steps through the polkit broker."""

    def __init__(self, broker: RootBroker | None = None):
        self._broker = broker or RootBroker()

    def run(self, argv, root, cwd, timeout, on_chunk):
        if root:
            return self._broker.run(argv, cwd, timeout, on_chunk)
        return stream_process(argv, cwd, timeout, on_chunk)

    def close(self) -> None:
        self._broker.close()


@dataclass
class Outcome:
    name: str
    status: str  # ok | fail | skip
    detail: str = ""


class StepFailed(Exception):
    """A step failed and its on_fail policy is `stop`."""


class Executor:
    def __init__(
        self,
        scenario: Scenario,
        variables: dict[str, str],
        runner: Runner,
        ui: UI,
        cwd: str,
        start_at: int = 1,
        sleep: Callable[[float], None] = time.sleep,
        clock: Callable[[], float] = time.monotonic,
    ):
        self.scenario = scenario
        self.vars = dict(variables)
        self.runner = runner
        self.ui = ui
        self.cwd = cwd
        self.start_at = start_at
        self.sleep = sleep
        self.clock = clock
        self.outcomes: list[Outcome] = []
        self._counter = 0
        self._total = count_steps(scenario.steps) + count_steps(scenario.restore)

    # ------------------------------------------------------------- public
    def run(self) -> bool:
        """Run the scenario, then the restore steps no matter what. True if nothing failed."""
        ok = True
        try:
            for position, step in enumerate(self.scenario.steps, start=1):
                if position < self.start_at:
                    self._skipped(
                        step,
                        f"before --from {self.start_at}",
                        count=count_steps([step]),
                    )
                    continue
                self._execute(step, self.vars)
        except StepFailed:
            ok = False
        except KeyboardInterrupt:
            self.ui.info("interrupted: restoring")
            ok = False
        finally:
            ok = self._restore() and ok
        return ok and not any(o.status == "fail" for o in self.outcomes)

    # ------------------------------------------------------------ plumbing
    def _restore(self) -> bool:
        ok = True
        for step in self.scenario.restore:
            try:
                self._execute(step, self.vars)
            except (StepFailed, KeyboardInterrupt):
                ok = False
        return ok

    def _skipped(self, step: Step, why: str, count: int = 1) -> None:
        for _ in range(count):
            self._counter += 1
            self.outcomes.append(Outcome(step.name, "skip", why))
        self.ui.info(f"skipped '{step.name}': {why}")

    def _sub(self, texts: list[str], variables: dict[str, str]) -> list[str]:
        return [substitute(t, variables) for t in texts]

    def _command(
        self, step: Step, argv: list[str], timeout: float, stream: bool
    ) -> Result:
        return self.runner.run(
            argv,
            step.root,
            self.cwd,
            timeout,
            self.ui.chunk if stream else lambda _: None,
        )

    def _judge(
        self, step: Step, result: Result, variables: dict[str, str]
    ) -> str | None:
        """None if the result is acceptable, otherwise why not."""
        if result.timed_out:
            return f"timed out after {step.timeout:g}s"
        if result.rc not in step.ok_rc:
            return f"exit code {result.rc}"
        if step.expect and not re.search(
            substitute(step.expect, variables), result.output, re.MULTILINE
        ):
            return f"output does not match /{step.expect}/"
        if step.expect_not:
            hit = re.search(
                substitute(step.expect_not, variables), result.output, re.MULTILINE
            )
            if hit:
                return f"output matches forbidden /{step.expect_not}/: {hit.group(0)!r}"
        return None

    def _fail(self, step: Step, why: str, output: str = "") -> None:
        self.ui.step_end("fail", why)
        if output:
            self.ui.output(output)
        self.outcomes.append(Outcome(step.name, "fail", why))
        if step.on_fail == "stop":
            raise StepFailed(step.name)

    # --------------------------------------------------------------- steps
    def _execute(self, step: Step, variables: dict[str, str]) -> None:
        if step.kind == "repeat":
            self._repeat(step, variables)
            return
        self._counter += 1
        name = substitute(step.name, variables)
        self.ui.step_start(self._counter, self._total, name, step.root)
        if (
            step.when
            and self.runner.run(
                self._sub(step.when, variables),
                False,
                self.cwd,
                PROBE_TIMEOUT,
                lambda _: None,
            ).rc
            != 0
        ):
            self.ui.step_end("skip", "condition not met")
            self.outcomes.append(Outcome(name, "skip", "condition not met"))
            return
        handler = {
            "run": self._run,
            "wait_for": self._wait_for,
            "prompt": self._prompt,
        }[step.kind]
        handler(step, name, variables)

    def _repeat(self, step: Step, variables: dict[str, str]) -> None:
        for i in range(1, step.count + 1):
            round_vars = {**variables, "i": str(i)}
            self.ui.info(f"{substitute(step.name, round_vars)}: round {i}/{step.count}")
            for child in step.steps:
                self._execute(child, round_vars)

    def _run(self, step: Step, name: str, variables: dict[str, str]) -> None:
        argv = self._sub(step.argv, variables)
        result = self._command(step, argv, step.timeout, step.stream)
        why = self._judge(step, result, variables)
        if why:
            self._fail(step, why, result.output)
            return
        if step.save_as:
            variables[step.save_as] = result.output.strip()
        self.ui.step_end("ok", "")
        self.outcomes.append(Outcome(name, "ok"))

    def _wait_for(self, step: Step, name: str, variables: dict[str, str]) -> None:
        argv = self._sub(step.argv, variables)
        deadline = self.clock() + step.timeout
        last = Result(-1, "")
        while True:
            last = self._command(step, argv, PROBE_TIMEOUT, False)
            if self._judge(step, last, variables) is None:
                if step.save_as:
                    variables[step.save_as] = last.output.strip()
                self.ui.step_end("ok", "")
                self.outcomes.append(Outcome(name, "ok"))
                return
            if self.clock() >= deadline:
                self._fail(step, f"not satisfied after {step.timeout:g}s", last.output)
                return
            self.sleep(step.interval)

    def _prompt(self, step: Step, name: str, variables: dict[str, str]) -> None:
        text = substitute(step.text, variables)
        probe = None
        if step.argv:
            argv = self._sub(step.argv, variables)

            def probe() -> bool:
                result = self._command(step, argv, PROBE_TIMEOUT, False)
                return self._judge(step, result, variables) is None

        while True:
            answer = self.ui.prompt(text, probe, step.timeout)
            if answer == "done":
                self.ui.step_end("ok", "")
                self.outcomes.append(Outcome(name, "ok"))
                return
            if answer == "timeout":
                answer = self.ui.choose(
                    "Not detected in time", "rsa"
                )  # retry / skip / abort
            if answer == "r":
                continue
            if answer == "s":
                self.ui.step_end("skip", "skipped by the user")
                self.outcomes.append(Outcome(name, "skip", "skipped by the user"))
                return
            self._fail(
                step,
                "aborted by the user"
                if answer in ("a", "abort")
                else "needs a person at a terminal",
            )
            return


def plan(scenario: Scenario, variables: dict[str, str]) -> list[str]:
    """Human-readable list of what a run would do (for --dry-run)."""
    lines: list[str] = []

    def walk(steps: list[Step], indent: str, local: dict[str, str]) -> None:
        for step in steps:
            if step.kind == "repeat":
                lines.append(f"{indent}repeat x{step.count}: {step.name}")
                walk(step.steps, indent + "  ", _Lenient({**local, "i": "<i>"}))
            elif step.kind == "prompt":
                lines.append(f"{indent}ask:    {substitute(step.text, local)}")
            else:
                shown = " ".join(substitute(a, local) for a in step.argv)
                tag = "[root] " if step.root else ""
                lines.append(
                    f"{indent}{step.kind}: {tag}{shown}   ({substitute(step.name, local)})"
                )

    lenient = _Lenient(variables)
    walk(scenario.steps, "", lenient)
    if scenario.restore:
        lines.append("then, always:")
        walk(scenario.restore, "  ", lenient)
    return lines


class _Lenient(dict):
    """Unknown variables (captured at run time) print as ${name} in a dry run."""

    def __missing__(self, key: str) -> str:
        return "${" + key + "}"

    def __contains__(self, key: object) -> bool:
        return True
