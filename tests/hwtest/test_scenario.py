import tempfile
import textwrap
import unittest
from pathlib import Path
from typing import ClassVar

from helpers import scenario_from
from hwtest.scenario import ScenarioError, count_steps, load, substitute


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


class IncludeTest(unittest.TestCase):
    BUILTIN: ClassVar[dict[str, str]] = {"repo": "/repo", "user": "u", "uid": "1"}

    def load_tree(self, files, entry="main"):
        with tempfile.TemporaryDirectory() as tmp:
            for name, text in files.items():
                (Path(tmp) / f"{name}.toml").write_text(textwrap.dedent(text))
            return load(Path(tmp) / f"{entry}.toml", self.BUILTIN)

    def test_included_steps_are_inlined_in_place(self):
        s = self.load_tree(
            {
                "main": """
                    [[step]]
                    name = "before"
                    run = ["one"]
                    [[step]]
                    include = "child"
                    [[step]]
                    name = "after"
                    run = ["three"]
                """,
                "child": '[[step]]\nname = "inside"\nrun = ["two"]\n',
            }
        )
        self.assertEqual([st.argv[0] for st in s.steps], ["one", "two", "three"])

    def test_variables_merge_and_the_includer_wins(self):
        s = self.load_tree(
            {
                "main": """
                    [vars]
                    shared = "parent"
                    [[step]]
                    include = "child"
                """,
                "child": """
                    [vars]
                    shared = "child"
                    only_child = "x"
                    [[step]]
                    name = "a"
                    run = ["echo", "${shared}", "${only_child}"]
                """,
            }
        )
        self.assertEqual(s.variables, {"shared": "parent", "only_child": "x"})

    def test_restore_steps_come_along(self):
        s = self.load_tree(
            {
                "main": '[[step]]\ninclude = "child"\n[[restore]]\nname = "r1"\nrun = ["parent-undo"]\n',
                "child": '[[step]]\nname = "a"\nrun = ["x"]\n[[restore]]\nname = "r2"\nrun = ["child-undo"]\n',
            }
        )
        self.assertEqual([r.argv[0] for r in s.restore], ["parent-undo", "child-undo"])

    def test_two_scenarios_that_include_each_other_are_rejected(self):
        with self.assertRaisesRegex(ScenarioError, "loop"):
            self.load_tree(
                {
                    "main": '[[step]]\ninclude = "other"\n',
                    "other": '[[step]]\ninclude = "main"\n',
                }
            )

    def test_an_include_cannot_escape_the_scenario_directory(self):
        for bad in ("../secret", "/etc/passwd", "a/b", ""):
            with (
                self.subTest(name=bad),
                self.assertRaisesRegex(ScenarioError, "include"),
            ):
                self.load_tree({"main": f'[[step]]\ninclude = "{bad}"\n'})

    def test_a_missing_include_is_a_clear_error(self):
        with self.assertRaisesRegex(ScenarioError, "nope.toml"):
            self.load_tree({"main": '[[step]]\ninclude = "nope"\n'})

    def test_the_shipped_full_scenario_contains_both_connect_disconnect_cycles(self):
        from hwtest import cli

        scenario, _ = cli.load_scenario("full")
        names = [st.name for st in scenario.steps]
        self.assertIn("Bluetooth connect/disconnect rounds", names)
        self.assertIn("USB plug/unplug rounds", names)


if __name__ == "__main__":
    unittest.main()
