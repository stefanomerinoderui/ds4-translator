"""Fakes shared by the hwtest unit tests."""

import sys
import tempfile
import textwrap
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "hwtest"))

from hwtest.process import Result
from hwtest.scenario import load


def scenario_from(text: str, builtin: dict[str, str] | None = None):
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "s.toml"
        path.write_text(textwrap.dedent(text))
        return load(path, builtin or {"repo": "/repo", "user": "u", "uid": "1"})


class FakeRunner:
    """Answers commands from a table of (argv prefix) -> Result and records every call."""

    def __init__(self, answers=None, default=None):
        self.answers = answers or {}
        self.default = default or Result(0, "")
        self.calls: list[tuple[list[str], bool]] = []

    def run(self, argv, root, cwd, timeout, on_chunk):
        self.calls.append((argv, root))
        for prefix, result in self.answers.items():
            if " ".join(argv).startswith(prefix):
                value = (
                    result.pop(0)
                    if isinstance(result, list) and len(result) > 1
                    else result
                )
                value = value[0] if isinstance(value, list) else value
                on_chunk(value.output)
                return value
        return self.default


class FakeUI:
    def __init__(self, prompt_answers=None, choices=None):
        self.events: list[tuple] = []
        self.prompt_answers = list(prompt_answers or [])
        self.choices = list(choices or [])
        self.probes_true: list[bool] = []

    def step_start(self, index, total, name, root):
        self.events.append(("start", index, total, name, root))

    def step_end(self, status, detail):
        self.events.append(("end", status, detail))

    def chunk(self, text):
        self.events.append(("chunk", text))

    def info(self, text):
        self.events.append(("info", text))

    def output(self, text):
        self.events.append(("output", text))

    def prompt(self, text, probe, timeout):
        self.events.append(("prompt", text))
        if probe is not None:
            self.probes_true.append(probe())
        return self.prompt_answers.pop(0) if self.prompt_answers else "done"

    def choose(self, question, options):
        self.events.append(("choose", question))
        return self.choices.pop(0) if self.choices else options[-1]

    def statuses(self):
        return [e[1] for e in self.events if e[0] == "end"]
