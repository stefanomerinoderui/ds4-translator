# Security findings register

Weaknesses found by the security review of the test-suite work, with the test
that pins each one. **No production fixes were made alongside the tests.**

Each pinned test is a *known-issue check*: it passes while the weakness exists
and fails with `FIXED?` (shell) or a "turn this into a regression test" message
(gtest) once it is gone. Whoever fixes a finding therefore has to flip the test
into an ordinary regression check and update this table, so a finding can
neither disappear nor come back unnoticed.

Target access policy for the control socket (what the H2 fix should enforce):
socket `root:ds4-translator` mode 0660, with `SO_PEERCRED` checked per
connection. Read-only commands (`status`, `test`) stay open to everyone;
mutating commands need root or the `ds4-translator` group.

| ID | Severity | Weakness | Where | Pinned by |
|----|----------|----------|-------|-----------|
| H1 | High | One idle client blocks the single-threaded main loop (blocking `read()` on the accepted fd, no timeout), freezing input forwarding for everyone | `src/main.cpp` accept/read in the main loop | `test_daemon_security.sh` (H1) |
| H2 | High | Socket is mode 0666 with no peer-credential check: any local user can take the physical pad (`release-physical` + `create-virtual` + `input`), rewrite `/etc/ds4-translator.conf`, trigger `udevadm trigger`, read the pad MAC | `src/main.cpp` `setup_ipc_socket` and command chain | `test_daemon_security.sh` (H2; the other-uid part needs passwordless sudo) |
| H3 | High | `identify` closes a dup'd fd that `BtOutputPacer` may still hold, so a later flush can write to a reused fd; each call also spawns an unbounded detached thread | `src/main.cpp` identify handler, `BtOutputPacer` | **Not automatable** (needs a Bluetooth pad). Manual: loop `ds4-ctl identify` while a game drives rumble over BT, under ASan/TSan |
| H4 | High | uevent netlink sender is never verified (`nl_pid`/`SCM_CREDENTIALS`), `HID_ID` is matched by substring, and the hid id handed to sysfs writes is just the last `DEVPATH` component | `src/hid-unbind-detect.cpp` `hid_uevent_monitor_read` | `test_uevent.cpp` `KnownIssue_H4_*`; `fuzz_uevent.cpp` |
| M1 | Medium | No cap on `test` subscribers; when `accept()` hits EMFILE the listen fd stays readable and `poll()` spins at 100% CPU | `src/main.cpp` subscriber list | `test_daemon_security.sh` (M1) |
| M2 | Medium | Daemon replies with `write()` and does not ignore SIGPIPE; a client that resets mid-reply kills it outside systemd (systemd ignores SIGPIPE by default) | `src/main.cpp` reply writes | `test_daemon_security.sh` (M2) |
| M3 | Medium | Possible use-after-free: the USB watchdog `libusb_close()`s the handle while the pump thread may be inside a transfer on it | `src/usb-hid-transport.cpp` | **Not automatable** (needs hardware). Manual: unplug the pad mid-transfer under an ASan build |
| L1 | Low | Server-side `set-name` accepts ESC/control characters and invalid UTF-8; they are echoed to other users' terminals by `status`/`set-name` | `src/main.cpp` set-name handler | `test_daemon_security.sh` (L1) |
| L2 | Low | Sentinel/state files are written through symlinks with a plain `ofstream` (arbitrary root file write if the directory is writable by others); `write_config` truncates in place (not atomic) | `src/main.cpp` | `test_daemon_security.sh` (L2); malformed-config robustness checks |
| L3 | Low | `udev-spoof.so`: NULL `sysattr` crashes the host process; the spoofed uevent text is one static buffer shared by all callers and threads | `src/udev-spoof.c` | `test_udev_spoof.cpp` `KnownIssue_L3_*` |
| L4 | Low | Virtual controller hidraw nodes get `MODE="0666"` (`uaccess` alone would do) | `72-ds4-translator-hide.rules` | `static_checks.sh` (L4) |
| L5 | Low | `ds4-translator.service` has no sandboxing (`NoNewPrivileges`, `ProtectSystem`, `ProtectHome`, `PrivateTmp`, `CapabilityBoundingSet`, `RestrictAddressFamilies`, `SystemCallFilter`) | `ds4-translator.service` | `static_checks.sh` (L5, plus the `systemd-analyze security` score) |
| L6 | Low | The Makefile requests no hardening flags (`_FORTIFY_SOURCE`, stack protector, PIE, full RELRO) and drops makepkg's CFLAGS for the daemon | `Makefile` | `static_checks.sh` (L6, plus a per-binary hardening report) |
| L7 | Low | CI: actions pinned by tag not SHA, bpftool downloaded without a checksum, `lint.yml` has no `permissions:` block, GPG signing key imported in the same step that builds | `.github/` | `static_checks.sh` (L7) |
| L8 | Low | UI single-instance socket falls back to a predictable `/tmp/ds4-translator-ui-<uid>` with no owner check | `ui/src/singleinstance.cpp` | **Not automated** (needs Qt and a second user). Manual: pre-create the directory as another user with `XDG_RUNTIME_DIR` unset |

## Burning a finding down
1. Fix the code.
2. The matching known-issue check now fails (`FIXED?` / "turn this into a regression test").
3. Invert it into a normal assertion of the secure behaviour (`known_issue` -> `check`/`EXPECT_*`), moving gtest cases out of the `KnownIssue_` naming.
4. Delete or update the row above.
