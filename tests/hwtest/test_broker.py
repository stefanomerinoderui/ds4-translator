import io
import json
import os
import sys
import unittest
from pathlib import Path

from helpers import FakeRunner  # noqa: F401  (also puts tools/hwtest on sys.path)
from hwtest.broker import BrokerError, RootBroker, serve
from hwtest.process import stream_process

TOOLS = str(Path(__file__).resolve().parents[2] / "tools" / "hwtest")
HELPER = [
    sys.executable,
    "-c",
    f"import sys; sys.path.insert(0, {TOOLS!r}); from hwtest.broker import serve; sys.exit(serve(require_root=False))",
]


def serve_lines(requests):
    out = io.StringIO()
    serve(
        stdin=io.StringIO("".join(json.dumps(r) + "\n" for r in requests)),
        stdout=out,
        require_root=False,
    )
    return [json.loads(line) for line in out.getvalue().splitlines()]


class ServeTest(unittest.TestCase):
    def test_runs_an_argv_and_streams_its_output(self):
        messages = serve_lines([{"id": 1, "argv": ["echo", "hi"], "cwd": "/"}])
        self.assertIn("hello", messages[0])
        self.assertEqual(
            "".join(m.get("chunk", "") for m in messages if m.get("id") == 1), "hi\n"
        )
        self.assertEqual(messages[-1], {"id": 1, "rc": 0, "timed_out": False})

    def test_a_shell_string_is_refused_and_nothing_runs(self):
        messages = serve_lines([{"id": 2, "argv": "echo pwned", "cwd": "/"}])
        self.assertEqual(messages[-1]["rc"], 126)
        self.assertIn("refused", messages[-2]["chunk"])

    def test_garbage_and_non_objects_are_refused_and_the_loop_survives(self):
        out = io.StringIO()
        serve(
            stdin=io.StringIO('not json\n[1,2]\n{"id": 3, "argv": ["true"]}\n'),
            stdout=out,
            require_root=False,
        )
        messages = [json.loads(line) for line in out.getvalue().splitlines()]
        self.assertEqual([m["rc"] for m in messages if "rc" in m], [126, 126, 0])

    def test_an_unstartable_command_reports_127(self):
        messages = serve_lines([{"id": 4, "argv": ["/nonexistent/binary"], "cwd": "/"}])
        self.assertEqual(messages[-1]["rc"], 127)

    def test_timeout_kills_the_command(self):
        messages = serve_lines(
            [{"id": 5, "argv": ["sleep", "30"], "cwd": "/", "timeout": 1}]
        )
        self.assertTrue(messages[-1]["timed_out"])

    def test_refuses_to_run_without_root(self):
        if os.geteuid() == 0:
            self.skipTest("running as root")
        self.assertEqual(
            serve(stdin=io.StringIO(""), stdout=io.StringIO(), require_root=True), 2
        )


class RootBrokerTest(unittest.TestCase):
    def setUp(self):
        self.broker = RootBroker(
            command=HELPER, expect_uid=-1
        )  # no PKEXEC_UID in the test env -> hello is -1
        self.addCleanup(self.broker.close)

    def test_round_trip_with_streaming(self):
        seen = []
        result = self.broker.run(
            ["sh", "-c", "echo out; echo err >&2; exit 3"], "/", 10, seen.append
        )
        self.assertEqual(result.rc, 3)
        self.assertIn("out", result.output)
        self.assertIn("err", result.output)
        self.assertEqual("".join(seen), result.output)

    def test_several_commands_share_one_helper(self):
        self.broker.run(["true"], "/", 10)
        first = self.broker._proc.pid
        self.broker.run(["true"], "/", 10)
        self.assertEqual(self.broker._proc.pid, first)

    def test_a_different_uid_is_refused(self):
        broker = RootBroker(command=HELPER, expect_uid=12345)
        with self.assertRaisesRegex(BrokerError, "different user"):
            broker.start()

    def test_a_cancelled_authorisation_is_a_clear_error(self):
        broker = RootBroker(command=[sys.executable, "-c", "import sys; sys.exit(126)"])
        with self.assertRaisesRegex(BrokerError, "refused or cancelled"):
            broker.start()


class StreamProcessTest(unittest.TestCase):
    def test_merges_stderr_and_reports_exit_code(self):
        chunks = []
        result = stream_process(
            ["sh", "-c", "echo a; echo b >&2; exit 2"], "/", 10, chunks.append
        )
        self.assertEqual(result.rc, 2)
        self.assertEqual(sorted(result.output.split()), ["a", "b"])

    def test_timeout_kills_the_whole_process_group(self):
        result = stream_process(["sh", "-c", "sleep 30 & wait"], "/", 1, lambda _: None)
        self.assertTrue(result.timed_out)

    def test_missing_binary(self):
        self.assertEqual(
            stream_process(["/nonexistent"], "/", 5, lambda _: None).rc, 127
        )


if __name__ == "__main__":
    unittest.main()
