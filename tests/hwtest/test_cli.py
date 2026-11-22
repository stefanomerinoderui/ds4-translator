import contextlib
import io
import tempfile
import textwrap
import unittest
from pathlib import Path

import helpers  # noqa: F401  (puts tools/hwtest on sys.path)
from hwtest import cli
from hwtest.report import Console


def run_cli(*argv):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        rc = cli.main(list(argv))
    return rc, out.getvalue(), err.getvalue()


def scenario_file(text):
    tmp = tempfile.TemporaryDirectory()
    path = Path(tmp.name) / "t.toml"
    path.write_text(textwrap.dedent(text))
    return tmp, str(path)


class CliTest(unittest.TestCase):
    def test_every_shipped_scenario_is_valid(self):
        for path in sorted(cli.SCENARIO_DIR.glob("*.toml")):
            with self.subTest(scenario=path.stem):
                rc, _, err = run_cli("check", path.stem)
                self.assertEqual(rc, 0, err)

    def test_list_shows_the_shipped_scenarios(self):
        rc, out, _ = run_cli("list")
        self.assertEqual(rc, 0)
        self.assertIn("reload", out)
        self.assertNotIn("INVALID", out)

    def test_check_reports_an_invalid_scenario(self):
        tmp, path = scenario_file('[[step]]\nname="a"\nrun="not a list"\n')
        with tmp:
            rc, _, err = run_cli("check", path)
        self.assertEqual(rc, 1)
        self.assertIn("argv", err)

    def test_dry_run_prints_the_plan_and_runs_nothing(self):
        tmp, path = scenario_file(
            """
            [[step]]
            name = "danger"
            run = ["sh", "-c", "touch /tmp/hwtest-should-not-exist"]
            root = true
            """
        )
        with tmp:
            rc, out, _ = run_cli("run", path, "--dry-run")
        self.assertEqual(rc, 0)
        self.assertIn("[root]", out)
        self.assertFalse(Path("/tmp/hwtest-should-not-exist").exists())

    def test_a_real_run_of_user_commands_passes_and_writes_a_log(self):
        tmp, path = scenario_file(
            'name = "cli-test"\n[[step]]\nname = "echo"\nrun = ["echo", "hi"]\n'
        )
        with tmp:
            rc, out, _ = run_cli("run", path, "--no-color")
        self.assertEqual(rc, 0)
        self.assertIn("PASSED: 1 ok", out)
        logs = sorted((cli.REPO / "build" / "hwtest").glob("cli-test-*.log"))
        self.assertTrue(logs)
        for log in logs:
            log.unlink()

    def test_a_failing_run_exits_nonzero_and_names_the_step(self):
        tmp, path = scenario_file(
            'name = "cli-fail"\n[[step]]\nname = "bad"\nrun = ["false"]\n'
        )
        with tmp:
            rc, out, _ = run_cli("run", path, "--no-color")
        self.assertEqual(rc, 1)
        self.assertIn("FAILED", out)
        self.assertIn("bad", out)
        for log in (cli.REPO / "build" / "hwtest").glob("cli-fail-*.log"):
            log.unlink()


class ConsoleTest(unittest.TestCase):
    def test_failed_output_shows_only_the_tail(self):
        out = io.StringIO()
        console = Console(out=out, color=False)
        console.output("\n".join(f"line {n}" for n in range(40)))
        text = out.getvalue()
        self.assertIn("line 39", text)
        self.assertNotIn("line 5\n", text)
        self.assertIn("earlier lines in the log", text)

    def test_a_prompt_without_a_terminal_aborts_instead_of_hanging(self):
        console = Console(out=io.StringIO(), color=False)
        console._interactive = False
        self.assertEqual(console.prompt("Press", None, 1), "abort")

    def test_a_probe_prompt_continues_by_itself_without_a_terminal(self):
        console = Console(out=io.StringIO(), color=False)
        console._interactive = False
        self.assertEqual(console.prompt("Plug", lambda: True, 5), "done")
        self.assertEqual(console.prompt("Plug", lambda: False, 0.01), "timeout")


if __name__ == "__main__":
    unittest.main()
