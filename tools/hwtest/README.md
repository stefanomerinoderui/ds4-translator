# hwtest: guided hardware tests

A small runner for tests that need a real controller, root and a person: it runs
every command, asks for root **once** through polkit, and tells you what to do
with the pad and when. It starts where packaging ends: install the package
yourself, then run a scenario.

```bash
tools/hwtest/run list                    # the scenarios
tools/hwtest/run run reload --dry-run    # show the steps; runs and asks nothing
tools/hwtest/run run reload              # daemon-reload, restart, hardware checks
tools/hwtest/run run usb-replug          # plug/unplug the cable 5 times
tools/hwtest/run run bluetooth-cycle     # turn the pad off/on 3 times
tools/hwtest/run run full                # everything: reload, Bluetooth + USB rounds, sweeps, rumble
tools/hwtest/run run capabilities        # find the minimal CapabilityBoundingSet
tools/hwtest/run check <scenario>        # validate a scenario file
```

Options of `run`: `--dry-run`, `--from N` (skip top-level steps before N, to
resume), `--no-color`. A full log is written to `build/hwtest/`; send it with
a bug report. The exit code is 0 only if every step passed.

## Root, polkit

Steps marked `root = true` run through **one** helper started with
`pkexec` the first time one is needed (a single password prompt per run).
The helper reads commands from a pipe owned by the runner, accepts only argv
lists (no shell strings), checks it was started for your user, and exits when
the run ends. If you cancel the prompt the run stops; there is no `sudo`
fallback. Everything else runs as you, so group membership and ACL checks
see what you would see.

Because a root helper executes whatever the scenario says, only run scenarios
you have read; `--dry-run` shows exactly what would run as root.

## Scenario format

A scenario is a TOML file in `tools/hwtest/scenarios/` (or any path). The
runner knows nothing about this project: everything specific is in the files.

```toml
name = "example"
description = "shown by `list`"

[vars]                    # ${name} in any string below; built-ins: repo, user, uid
unit = "ds4-translator.service"

[[step]]
name = "Restart the service"
run = ["systemctl", "restart", "${unit}"]   # an argv list, never a shell string
root = true                                 # via the polkit helper
timeout = 60                                # seconds (default 60)
ok_rc = [0, 1]                              # accepted exit codes (default [0])
expect = "regex"                            # output must match
expect_not = "regex"                        # output must not match
save_as = "inv"                             # keep the stripped output as ${inv}
stream = true                               # show output live
on_fail = "continue"                        # default "stop"
when = ["probe", "arg"]                     # skip the step unless this exits 0

[[step]]
name = "Wait for it"
wait_for = ["systemctl", "is-active", "--quiet", "${unit}"]  # poll until ok
interval = 1                                                # seconds between polls
timeout = 30

[[step]]
name = "Tell the user what to do"
prompt = "Plug the USB cable in."          # shown in a box; Enter continues
until = ["ds4-ctl", "status"]               # optional: continue by itself when this
expect = "Connection Type: USB"             #   probe matches (Enter still works)
timeout = 60                                # then: retry / skip / abort
guard = ["ds4-ctl", "status"]               # optional trio: while waiting, if `guard` output
guard_expect = "Connection Type: Bluetooth" #   matches, `guard_run` undoes it and, inside a
guard_run = ["tests/hardware/bt-disconnect.sh"]  # repeat, that round is replayed uncounted

[[step]]
name = "Rounds"
repeat = 5                                  # ${i} is the round, 1-based
max_redo = 3                                # rounds a guard may throw away before failing

[[step.step]]
name = "Round ${i}"
run = ["true"]

[[step]]
include = "bluetooth-cycle"                 # splice in another scenario's steps, variables
                                            #   (yours win on a clash) and restore steps

[[restore]]                                 # always runs at the end: success,
name = "Put things back"                    #   failure or Ctrl-C (run/wait_for only)
run = ["true"]
```

Variables are checked when the scenario loads: using one before it is defined
(or a typo) is an error, not a command with a hole in it. To pass a captured
value to a shell snippet, use positional arguments
(`["sh", "-c", "... \"$1\"", "sh", "${inv}"]`) rather than splicing it into
the script text.

## Development

```bash
make test-hwtest                              # unit tests: no hardware, no root
ruff check tools/hwtest tests/hwtest && ruff format --check tools/hwtest tests/hwtest
```
