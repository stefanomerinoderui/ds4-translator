import unittest

from helpers import scenario_from
from hwtest.scenario import ScenarioError, count_steps, substitute


class ParsingTest(unittest.TestCase):
    def test_minimal_scenario(self):
        s = scenario_from(
            """
            name = "x"
            [[step]]
            name = "a"
            run = ["true"]
            """
        )
        self.assertEqual(s.steps[0].argv, ["true"])
        self.assertFalse(s.steps[0].root)

    def test_shell_string_is_rejected(self):
        with self.assertRaisesRegex(ScenarioError, "argv"):
            scenario_from('[[step]]\nname="a"\nrun = "echo hi"\n')

    def test_step_needs_exactly_one_kind(self):
        with self.assertRaisesRegex(ScenarioError, "exactly one"):
            scenario_from('[[step]]\nname="a"\nrun=["true"]\nwait_for=["true"]\n')
        with self.assertRaisesRegex(ScenarioError, "exactly one"):
            scenario_from('[[step]]\nname="a"\n')

    def test_invalid_regex_is_reported(self):
        with self.assertRaisesRegex(ScenarioError, "invalid regex"):
            scenario_from('[[step]]\nname="a"\nrun=["true"]\nexpect="("\n')

    def test_bad_on_fail_and_timeout(self):
        with self.assertRaisesRegex(ScenarioError, "on_fail"):
            scenario_from('[[step]]\nname="a"\nrun=["true"]\non_fail="explode"\n')
        with self.assertRaisesRegex(ScenarioError, "timeout"):
            scenario_from('[[step]]\nname="a"\nrun=["true"]\ntimeout=-1\n')

    def test_root_only_on_commands(self):
        with self.assertRaisesRegex(ScenarioError, "root"):
            scenario_from('[[step]]\nname="a"\nprompt="hi"\nroot=true\n')

    def test_repeat_needs_a_body_and_positive_count(self):
        with self.assertRaisesRegex(ScenarioError, "nested"):
            scenario_from('[[step]]\nname="a"\nrepeat=2\n')
        with self.assertRaisesRegex(ScenarioError, "positive"):
            scenario_from(
                '[[step]]\nname="a"\nrepeat=0\n[[step.step]]\nname="b"\nrun=["true"]\n'
            )

    def test_no_steps(self):
        with self.assertRaisesRegex(ScenarioError, "at least one"):
            scenario_from('name = "x"\n')

    def test_invalid_toml(self):
        with self.assertRaisesRegex(ScenarioError, "TOML"):
            scenario_from("[[step\n")

    def test_restore_steps_must_be_commands(self):
        with self.assertRaisesRegex(ScenarioError, "restore"):
            scenario_from(
                '[[step]]\nname="a"\nrun=["true"]\n[[restore]]\nname="r"\nprompt="x"\n'
            )

    def test_vars_cannot_shadow_builtins(self):
        with self.assertRaisesRegex(ScenarioError, "built-in"):
            scenario_from('[vars]\nrepo="x"\n[[step]]\nname="a"\nrun=["true"]\n')


class VariablesTest(unittest.TestCase):
    def test_undefined_variable_is_caught_before_running(self):
        with self.assertRaisesRegex(ScenarioError, "not defined"):
            scenario_from('[[step]]\nname="a"\nrun=["echo", "${nope}"]\n')

    def test_save_as_makes_a_variable_available_to_later_steps_only(self):
        scenario_from(
            """
            [[step]]
            name = "a"
            run = ["true"]
            save_as = "got"
            [[step]]
            name = "b"
            run = ["echo", "${got}"]
            """
        )
        with self.assertRaisesRegex(ScenarioError, "not defined"):
            scenario_from(
                """
                [[step]]
                name = "b"
                run = ["echo", "${got}"]
                [[step]]
                name = "a"
                run = ["true"]
                save_as = "got"
                """
            )

    def test_loop_index_is_only_defined_inside_a_repeat(self):
        scenario_from(
            """
            [[step]]
            name = "r"
            repeat = 2
            [[step.step]]
            name = "b"
            run = ["echo", "${i}"]
            """
        )
        with self.assertRaisesRegex(ScenarioError, "not defined"):
            scenario_from('[[step]]\nname="b"\nrun=["echo", "${i}"]\n')

    def test_substitute_leaves_regex_anchors_alone(self):
        self.assertEqual(substitute("^a${x}b$", {"x": "-"}), "^a-b$")

    def test_substitute_rejects_unknown_names(self):
        with self.assertRaises(ScenarioError):
            substitute("${x}", {})

    def test_count_steps_expands_repeats(self):
        s = scenario_from(
            """
            [[step]]
            name = "one"
            run = ["true"]
            [[step]]
            name = "r"
            repeat = 3
            [[step.step]]
            name = "a"
            run = ["true"]
            [[step.step]]
            name = "b"
            run = ["true"]
            """
        )
        self.assertEqual(count_steps(s.steps), 7)


if __name__ == "__main__":
    unittest.main()
