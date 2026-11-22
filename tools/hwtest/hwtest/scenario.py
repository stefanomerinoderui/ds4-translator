"""Scenario files: parsing and validation.

A scenario is a TOML file with optional `[vars]`, a list of `[[step]]` tables
and an optional list of `[[restore]]` tables that always run at the end. See
tools/hwtest/README.md for the format.
"""

from __future__ import annotations

import re
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

KINDS = ("run", "wait_for", "prompt", "repeat")
ON_FAIL = ("stop", "continue")
_VAR = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}")
_INCLUDE_NAME = re.compile(r"[A-Za-z0-9_-]+")


class ScenarioError(Exception):
    """The scenario file is malformed."""


@dataclass
class Step:
    name: str
    kind: str
    argv: list[str] = field(default_factory=list)  # run / wait_for / prompt probe
    text: str = ""  # prompt text
    root: bool = False
    timeout: float = 60.0
    interval: float = 1.0
    expect: str | None = None
    expect_not: str | None = None
    ok_rc: tuple[int, ...] = (0,)
    on_fail: str = "stop"
    when: list[str] = field(default_factory=list)
    save_as: str | None = None
    stream: bool = False
    count: int = 0  # repeat
    steps: list[Step] = field(default_factory=list)  # repeat body
    max_redo: int = 3  # repeat: rounds that may be thrown away by a guard
    # prompt only: a condition that must not appear while waiting. When
    # `guard` output matches `guard_expect`, `guard_run` undoes it and the
    # enclosing repeat round starts over without counting.
    guard: list[str] = field(default_factory=list)
    guard_expect: str | None = None
    guard_run: list[str] = field(default_factory=list)


@dataclass
class Scenario:
    name: str
    description: str
    variables: dict[str, str]
    steps: list[Step]
    restore: list[Step]
    path: Path | None = None


def _argv(value: object, where: str) -> list[str]:
    if (
        not isinstance(value, list)
        or not value
        or not all(isinstance(a, str) for a in value)
    ):
        raise ScenarioError(
            f"{where}: must be a non-empty list of strings (an argv, never a shell string)"
        )
    return list(value)


def _regex(value: object, where: str) -> str | None:
    if value is None:
        return None
    if not isinstance(value, str):
        raise ScenarioError(f"{where}: must be a string")
    try:
        re.compile(value)
    except re.error as exc:
        raise ScenarioError(f"{where}: invalid regex ({exc})") from exc
    return value


def _number(table: dict, key: str, default: float, where: str) -> float:
    value = table.get(key, default)
    if isinstance(value, bool) or not isinstance(value, (int, float)) or value <= 0:
        raise ScenarioError(f"{where}.{key}: must be a positive number")
    return float(value)


def _parse_guard(step: Step, table: dict, where: str) -> None:
    present = [k for k in ("guard", "guard_expect", "guard_run") if k in table]
    if not present:
        return
    if len(present) != 3:
        raise ScenarioError(f"{where}: guard, guard_expect and guard_run go together")
    step.guard = _argv(table["guard"], f"{where}.guard")
    step.guard_expect = _regex(table["guard_expect"], f"{where}.guard_expect")
    step.guard_run = _argv(table["guard_run"], f"{where}.guard_run")


def parse_step(table: dict, where: str) -> Step:
    name = table.get("name")
    if not isinstance(name, str) or not name:
        raise ScenarioError(f"{where}: every step needs a name")
    where = f"{where} '{name}'"
    kinds = [k for k in KINDS if k in table]
    if len(kinds) != 1:
        raise ScenarioError(
            f"{where}: needs exactly one of {', '.join(KINDS)} (found {kinds or 'none'})"
        )
    kind = kinds[0]

    on_fail = table.get("on_fail", "stop")
    if on_fail not in ON_FAIL:
        raise ScenarioError(f"{where}.on_fail: must be one of {', '.join(ON_FAIL)}")
    ok_rc = table.get("ok_rc", [0])
    if not isinstance(ok_rc, list) or not all(isinstance(r, int) for r in ok_rc):
        raise ScenarioError(f"{where}.ok_rc: must be a list of integers")

    step = Step(
        name=name,
        kind=kind,
        root=bool(table.get("root", False)),
        timeout=_number(table, "timeout", 60.0, where),
        interval=_number(table, "interval", 1.0, where),
        expect=_regex(table.get("expect"), f"{where}.expect"),
        expect_not=_regex(table.get("expect_not"), f"{where}.expect_not"),
        ok_rc=tuple(ok_rc),
        on_fail=on_fail,
        when=_argv(table["when"], f"{where}.when") if "when" in table else [],
        save_as=table.get("save_as"),
        stream=bool(table.get("stream", False)),
    )
    if kind in ("run", "wait_for"):
        step.argv = _argv(table[kind], f"{where}.{kind}")
    elif kind == "prompt":
        if not isinstance(table["prompt"], str) or not table["prompt"].strip():
            raise ScenarioError(f"{where}.prompt: must be the text to show the user")
        step.text = table["prompt"]
        if "until" in table:
            step.argv = _argv(table["until"], f"{where}.until")
        _parse_guard(step, table, where)
    else:
        count = table["repeat"]
        if isinstance(count, bool) or not isinstance(count, int) or count < 1:
            raise ScenarioError(f"{where}.repeat: must be a positive integer")
        step.count = count
        max_redo = table.get("max_redo", 3)
        if isinstance(max_redo, bool) or not isinstance(max_redo, int) or max_redo < 0:
            raise ScenarioError(f"{where}.max_redo: must be an integer >= 0")
        step.max_redo = max_redo
        body = table.get("step")
        if not isinstance(body, list) or not body:
            raise ScenarioError(
                f"{where}: a repeat needs at least one nested [[step.step]]"
            )
        step.steps = [parse_step(t, where) for t in body]
    if step.root and kind in ("prompt", "repeat"):
        raise ScenarioError(f"{where}: 'root' only applies to run and wait_for steps")
    return step


def _walk(steps: list[Step]):
    for step in steps:
        yield step
        yield from _walk(step.steps)


def check_variables(scenario: Scenario, builtin: set[str]) -> None:
    """Every ${var} must be defined before the step that uses it."""
    known = set(builtin) | set(scenario.variables)

    def texts(step: Step) -> list[str]:
        return [
            *step.argv,
            *step.when,
            *step.guard,
            *step.guard_run,
            step.text,
            step.name,
            step.expect or "",
            step.expect_not or "",
        ]

    def visit(steps: list[Step], known: set[str]) -> None:
        for step in steps:
            visible = (
                known | {"i"} if step.kind == "repeat" else known
            )  # a repeat's title may name the round
            for text in texts(step):
                for var in _VAR.findall(text):
                    if var not in visible:
                        raise ScenarioError(
                            f"step '{step.name}': ${{{var}}} is not defined at this point"
                        )
            if step.kind == "repeat":
                visit(step.steps, known | {"i"})
            if step.save_as:
                known.add(step.save_as)

    visit(scenario.steps, known)
    visit(scenario.restore, known)


def _expand_steps(
    raw: list, path: Path, builtin_vars: dict[str, str], stack: tuple[Path, ...]
) -> tuple[list[Step], dict[str, str], list[Step]]:
    """Parse [[step]] tables, replacing `include = "other"` with that scenario's steps.

    Returns the steps plus the variables and restore steps the included
    scenarios bring along.
    """
    steps: list[Step] = []
    variables: dict[str, str] = {}
    restore: list[Step] = []
    for table in raw:
        if not isinstance(table, dict) or "include" not in table:
            steps.append(parse_step(table, "step"))
            continue
        name = table["include"]
        if not isinstance(name, str) or not _INCLUDE_NAME.fullmatch(name):
            raise ScenarioError(
                f"include {name!r}: must be a scenario name (letters, digits, - and _)"
            )
        child_path = path.parent / f"{name}.toml"
        if child_path.resolve() in stack:
            raise ScenarioError(
                f"include {name!r}: scenarios include each other in a loop"
            )
        child = load(child_path, builtin_vars, stack)
        steps.extend(child.steps)
        restore.extend(child.restore)
        for key, value in child.variables.items():
            variables.setdefault(key, value)
    return steps, variables, restore


def load(
    path: Path, builtin_vars: dict[str, str], _stack: tuple[Path, ...] = ()
) -> Scenario:
    stack = (*_stack, path.resolve())
    try:
        with open(path, "rb") as handle:
            data = tomllib.load(handle)
    except tomllib.TOMLDecodeError as exc:
        raise ScenarioError(f"{path}: not valid TOML ({exc})") from exc
    except OSError as exc:
        raise ScenarioError(f"{path}: {exc.strerror}") from exc

    variables = data.get("vars", {})
    if not isinstance(variables, dict) or not all(
        isinstance(v, (str, int)) for v in variables.values()
    ):
        raise ScenarioError("[vars] must map names to strings or integers")
    clash = set(variables) & (set(builtin_vars) | {"i"})
    if clash:
        raise ScenarioError(
            f"[vars] redefines built-in variable(s): {', '.join(sorted(clash))}"
        )

    steps_raw = data.get("step", [])
    if not isinstance(steps_raw, list) or not steps_raw:
        raise ScenarioError("a scenario needs at least one [[step]]")
    steps, inherited_vars, inherited_restore = _expand_steps(
        steps_raw, path, builtin_vars, stack
    )
    own_vars = {k: str(v) for k, v in variables.items()}
    scenario = Scenario(
        name=str(data.get("name", path.stem)),
        description=str(data.get("description", "")),
        # The including scenario wins over what an included one defines.
        variables={**inherited_vars, **own_vars},
        steps=steps,
        restore=[parse_step(t, "restore") for t in data.get("restore", [])]
        + inherited_restore,
        path=path,
    )
    for step in _walk(scenario.restore):
        if step.kind in ("prompt", "repeat"):
            raise ScenarioError(
                f"restore '{step.name}': restore steps must be run or wait_for"
            )
    check_variables(scenario, set(builtin_vars))
    return scenario


def substitute(text: str, variables: dict[str, str]) -> str:
    """Replace ${var}. Only that exact form is touched, so regexes may use `$`.

    Unknown names are an error: a typo must not run a command with a hole in it.
    """

    def replace(match: re.Match) -> str:
        name = match.group(1)
        if name not in variables:
            raise ScenarioError(f"undefined variable ${{{name}}}")
        return variables[name]

    return _VAR.sub(replace, text)


def count_steps(steps: list[Step]) -> int:
    """Total leaf steps, with repeats expanded (for the [n/total] progress)."""
    return sum(
        s.count * count_steps(s.steps) if s.kind == "repeat" else 1 for s in steps
    )
