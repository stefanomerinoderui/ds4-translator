import unittest

from helpers import FakeRunner, FakeUI, scenario_from
from hwtest.process import Result
from hwtest.steps import Executor, plan

VARS = {"repo": "/repo", "user": "u", "uid": "1"}


def run_scenario(text, runner=None, ui=None, **kwargs):
    scenario = scenario_from(text)
    runner = runner or FakeRunner()
    ui = ui or FakeUI()
    ticks = iter(range(10_000))
    executor = Executor(
        scenario,
        VARS,
        runner,
        ui,
        cwd="/repo",
        sleep=lambda _: None,
        clock=lambda: float(next(ticks)),
        **kwargs,
    )
    return executor.run(), executor, runner, ui


class RunStepTest(unittest.TestCase):
    def test_success_and_root_routing(self):
        ok, _, runner, _ = run_scenario(
            """
            [[step]]
            name = "a"
            run = ["echo", "hi"]
            [[step]]
            name = "b"
            run = ["systemctl", "daemon-reload"]
            root = true
            """
        )
        self.assertTrue(ok)
        self.assertEqual(
            runner.calls,
            [(["echo", "hi"], False), (["systemctl", "daemon-reload"], True)],
        )

    def test_nonzero_exit_fails_and_stops(self):
        runner = FakeRunner({"false": Result(1, "boom")})
        ok, executor, runner, ui = run_scenario(
            '[[step]]\nname="a"\nrun=["false"]\n[[step]]\nname="b"\nrun=["echo"]\n',
            runner=runner,
        )
        self.assertFalse(ok)
        self.assertEqual([o.status for o in executor.outcomes], ["fail"])
        self.assertEqual(len(runner.calls), 1)  # "b" never ran
        self.assertIn(("output", "boom"), ui.events)

    def test_on_fail_continue_keeps_going_but_the_run_still_fails(self):
        runner = FakeRunner({"false": Result(1, "")})
        ok, executor, _, _ = run_scenario(
            '[[step]]\nname="a"\nrun=["false"]\non_fail="continue"\n[[step]]\nname="b"\nrun=["echo"]\n',
            runner=runner,
        )
        self.assertFalse(ok)
        self.assertEqual([o.status for o in executor.outcomes], ["fail", "ok"])

    def test_ok_rc_accepts_listed_codes(self):
        runner = FakeRunner({"systemctl": Result(1, "")})
        ok, *_ = run_scenario(
            '[[step]]\nname="a"\nrun=["systemctl","x"]\nok_rc=[0,1]\n', runner=runner
        )
        self.assertTrue(ok)

    def test_expect_and_expect_not(self):
        runner = FakeRunner({"cmd": Result(0, "Physical Controller: None\n")})
        ok, *_ = run_scenario(
            '[[step]]\nname="a"\nrun=["cmd"]\nexpect="Controller: None$"\n',
            runner=runner,
        )
        self.assertTrue(ok)
        ok, executor, *_ = run_scenario(
            '[[step]]\nname="a"\nrun=["cmd"]\nexpect_not="None"\n', runner=runner
        )
        self.assertFalse(ok)
        self.assertIn("forbidden", executor.outcomes[0].detail)
        ok, executor, *_ = run_scenario(
            '[[step]]\nname="a"\nrun=["cmd"]\nexpect="Bluetooth"\n', runner=runner
        )
        self.assertFalse(ok)

    def test_timeout_is_reported(self):
        runner = FakeRunner({"slow": Result(-9, "", timed_out=True)})
        ok, executor, *_ = run_scenario(
            '[[step]]\nname="a"\nrun=["slow"]\ntimeout=5\n', runner=runner
        )
        self.assertFalse(ok)
        self.assertIn("timed out", executor.outcomes[0].detail)

    def test_save_as_feeds_a_later_step(self):
        runner = FakeRunner({"get": Result(0, "abc123\n")})
        _, _, runner, _ = run_scenario(
            """
            [[step]]
            name = "a"
            run = ["get"]
            save_as = "inv"
            [[step]]
            name = "b"
            run = ["use", "${inv}"]
            """,
            runner=runner,
        )
        self.assertEqual(runner.calls[-1][0], ["use", "abc123"])

    def test_when_probe_skips_the_step(self):
        runner = FakeRunner({"probe": Result(1, "")})
        ok, executor, runner, _ = run_scenario(
            '[[step]]\nname="a"\nrun=["echo"]\nwhen=["probe"]\n', runner=runner
        )
        self.assertTrue(ok)
        self.assertEqual([o.status for o in executor.outcomes], ["skip"])
        self.assertNotIn((["echo"], False), runner.calls)

    def test_from_skips_earlier_top_level_steps(self):
        ok, executor, runner, _ = run_scenario(
            '[[step]]\nname="a"\nrun=["one"]\n[[step]]\nname="b"\nrun=["two"]\n',
            start_at=2,
        )
        self.assertTrue(ok)
        self.assertEqual([c[0] for c in runner.calls], [["two"]])
        self.assertEqual([o.status for o in executor.outcomes], ["skip", "ok"])


class WaitForTest(unittest.TestCase):
    def test_polls_until_it_matches(self):
        runner = FakeRunner(
            {"ds4-ctl": [Result(0, "None"), Result(0, "None"), Result(0, "0005:054C")]}
        )
        ok, _, runner, _ = run_scenario(
            '[[step]]\nname="w"\nwait_for=["ds4-ctl"]\nexpect="054C"\ntimeout=100\n',
            runner=runner,
        )
        self.assertTrue(ok)
        self.assertEqual(len(runner.calls), 3)

    def test_gives_up_after_the_timeout(self):
        runner = FakeRunner({"ds4-ctl": Result(0, "None")})
        ok, executor, *_ = run_scenario(
            '[[step]]\nname="w"\nwait_for=["ds4-ctl"]\nexpect="054C"\ntimeout=3\n',
            runner=runner,
        )
        self.assertFalse(ok)
        self.assertIn("not satisfied", executor.outcomes[0].detail)


class RepeatTest(unittest.TestCase):
    SCENARIO = """
        [[step]]
        name = "loop"
        repeat = 3
        [[step.step]]
        name = "a ${i}"
        run = ["echo", "${i}"]
        """

    def test_runs_the_body_each_round_with_the_index(self):
        ok, _, runner, ui = run_scenario(self.SCENARIO)
        self.assertTrue(ok)
        self.assertEqual(
            [c[0] for c in runner.calls], [["echo", "1"], ["echo", "2"], ["echo", "3"]]
        )
        self.assertEqual(
            [e[1:3] for e in ui.events if e[0] == "start"], [(1, 3), (2, 3), (3, 3)]
        )

    def test_a_failure_in_round_two_stops_the_loop(self):
        runner = FakeRunner({"echo 2": Result(1, "")})
        ok, _executor, runner, _ = run_scenario(self.SCENARIO, runner=runner)
        self.assertFalse(ok)
        self.assertEqual(len(runner.calls), 2)


class PromptTest(unittest.TestCase):
    PROMPT = (
        '[[step]]\nname="p"\nprompt="Plug it in"\nuntil=["ds4-ctl"]\nexpect="USB"\n'
    )

    def test_done_continues_and_the_probe_is_the_until_command(self):
        runner = FakeRunner({"ds4-ctl": Result(0, "Connection Type: USB")})
        ok, _, _, ui = run_scenario(self.PROMPT, runner=runner)
        self.assertTrue(ok)
        self.assertEqual(ui.probes_true, [True])

    def test_timeout_then_retry_then_done(self):
        ui = FakeUI(prompt_answers=["timeout", "done"], choices=["r"])
        ok, _, _, ui = run_scenario(self.PROMPT, ui=ui)
        self.assertTrue(ok)
        self.assertEqual(sum(1 for e in ui.events if e[0] == "prompt"), 2)

    def test_timeout_then_skip(self):
        ui = FakeUI(prompt_answers=["timeout"], choices=["s"])
        ok, executor, *_ = run_scenario(self.PROMPT, ui=ui)
        self.assertTrue(ok)
        self.assertEqual(executor.outcomes[0].status, "skip")

    def test_timeout_then_abort_fails_the_run(self):
        ui = FakeUI(prompt_answers=["timeout"], choices=["a"])
        ok, executor, *_ = run_scenario(self.PROMPT, ui=ui)
        self.assertFalse(ok)
        self.assertEqual(executor.outcomes[0].status, "fail")

    def test_no_terminal_means_abort_rather_than_hanging(self):
        ui = FakeUI(prompt_answers=["abort"])
        ok, _executor, *_ = run_scenario('[[step]]\nname="p"\nprompt="Press"\n', ui=ui)
        self.assertFalse(ok)


class RestoreTest(unittest.TestCase):
    SCENARIO = """
        [[step]]
        name = "change"
        run = ["change"]
        [[step]]
        name = "boom"
        run = ["boom"]
        [[restore]]
        name = "undo"
        run = ["undo"]
        """

    def test_restore_runs_after_a_failure(self):
        runner = FakeRunner({"boom": Result(1, "")})
        ok, _, runner, _ = run_scenario(self.SCENARIO, runner=runner)
        self.assertFalse(ok)
        self.assertEqual([c[0] for c in runner.calls], [["change"], ["boom"], ["undo"]])

    def test_restore_runs_after_success_too(self):
        ok, _, runner, _ = run_scenario(self.SCENARIO)
        self.assertTrue(ok)
        self.assertEqual(runner.calls[-1][0], ["undo"])

    def test_restore_runs_on_ctrl_c(self):
        class Interrupting(FakeRunner):
            def run(self, argv, *rest):
                if argv == ["boom"]:
                    raise KeyboardInterrupt
                return super().run(argv, *rest)

        ok, _, runner, ui = run_scenario(self.SCENARIO, runner=Interrupting())
        self.assertFalse(ok)
        self.assertEqual(runner.calls[-1][0], ["undo"])
        self.assertIn(("info", "interrupted: restoring"), ui.events)

    def test_a_failing_restore_step_fails_the_run(self):
        runner = FakeRunner({"undo": Result(1, "")})
        ok, *_ = run_scenario(self.SCENARIO, runner=runner)
        self.assertFalse(ok)


class PlanTest(unittest.TestCase):
    def test_dry_run_marks_root_steps_and_expands_nothing_it_cannot_know(self):
        scenario = scenario_from(
            """
            [[step]]
            name = "r"
            run = ["systemctl", "restart", "x"]
            root = true
            [[step]]
            name = "get"
            run = ["id"]
            save_as = "inv"
            [[step]]
            name = "use"
            run = ["echo", "${inv}"]
            [[step]]
            name = "loop"
            repeat = 2
            [[step.step]]
            name = "n"
            prompt = "Round ${i}"
            """
        )
        lines = plan(scenario, VARS)
        self.assertIn("[root]", lines[0])
        self.assertIn("echo ${inv}", lines[2])
        self.assertIn("repeat x2", lines[3])
        self.assertIn("Round <i>", lines[4])


if __name__ == "__main__":
    unittest.main()
