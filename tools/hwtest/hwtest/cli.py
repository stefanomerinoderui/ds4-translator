"""Command line: run, list and check scenarios; `--broker` is the internal root helper."""

from __future__ import annotations

import argparse
import getpass
import os
import sys
import time
from pathlib import Path

from . import broker
from .report import Console
from .scenario import Scenario, ScenarioError, load, substitute
from .steps import CommandRunner, Executor, plan

PACKAGE_DIR = Path(__file__).resolve().parent.parent  # tools/hwtest
SCENARIO_DIR = PACKAGE_DIR / "scenarios"
REPO = PACKAGE_DIR.parent.parent


def builtin_variables() -> dict[str, str]:
    return {"repo": str(REPO), "user": getpass.getuser(), "uid": str(os.getuid())}


def resolve(name: str) -> Path:
    candidate = Path(name)
    if candidate.suffix == ".toml" and candidate.exists():
        return candidate
    return SCENARIO_DIR / f"{name}.toml"


def load_scenario(name: str) -> tuple[Scenario, dict[str, str]]:
    builtin = builtin_variables()
    scenario = load(resolve(name), builtin)
    variables = dict(builtin)
    for (
        key,
        value,
    ) in scenario.variables.items():  # a [vars] value may use the built-ins
        variables[key] = substitute(value, variables)
    return scenario, variables


def cmd_list(_args) -> int:
    for path in sorted(SCENARIO_DIR.glob("*.toml")):
        try:
            scenario = load(path, builtin_variables())
            print(f"{path.stem:18} {scenario.description}")
        except ScenarioError as exc:
            print(f"{path.stem:18} INVALID: {exc}")
    return 0


def cmd_check(args) -> int:
    try:
        scenario, _ = load_scenario(args.scenario)
    except ScenarioError as exc:
        print(f"invalid: {exc}", file=sys.stderr)
        return 1
    print(f"ok: {scenario.name} ({len(scenario.steps)} top-level steps)")
    return 0


def cmd_run(args) -> int:
    try:
        scenario, variables = load_scenario(args.scenario)
    except ScenarioError as exc:
        print(f"invalid scenario: {exc}", file=sys.stderr)
        return 2

    if args.dry_run:
        print(f"{scenario.name}: {scenario.description}\n")
        print("\n".join(plan(scenario, variables)))
        return 0

    log_dir = REPO / "build" / "hwtest"
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / f"{scenario.name}-{time.strftime('%Y%m%d-%H%M%S')}.log"
    with open(log_path, "w") as log:
        ui = Console(log=log, color=False if args.no_color else None)
        ui.info(f"{scenario.name}: {scenario.description}")
        runner = CommandRunner()
        executor = None
        ok = False
        try:
            executor = Executor(
                scenario, variables, runner, ui, cwd=str(REPO), start_at=args.start_from
            )
            ok = executor.run()
        except (ScenarioError, broker.BrokerError) as exc:
            ui.info(f"stopped: {exc}")
        finally:
            runner.close()
        ui.summary(executor.outcomes if executor else [], str(log_path))
    return 0 if ok else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="hwtest", description="Guided hardware-test runner."
    )
    parser.add_argument("--broker", action="store_true", help=argparse.SUPPRESS)
    sub = parser.add_subparsers(dest="command")
    sub.add_parser("list", help="list the scenarios")
    check = sub.add_parser("check", help="validate a scenario without running it")
    check.add_argument("scenario")
    run = sub.add_parser("run", help="run a scenario")
    run.add_argument(
        "scenario", help="scenario name (see `list`) or path to a .toml file"
    )
    run.add_argument(
        "--dry-run",
        action="store_true",
        help="print the steps, run nothing, ask for nothing",
    )
    run.add_argument(
        "--from",
        dest="start_from",
        type=int,
        default=1,
        metavar="N",
        help="skip top-level steps before N",
    )
    run.add_argument("--no-color", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.broker:
        return broker.serve()
    handlers = {"list": cmd_list, "check": cmd_check, "run": cmd_run}
    if args.command not in handlers:
        build_parser().print_help()
        return 2
    return handlers[args.command](args)
