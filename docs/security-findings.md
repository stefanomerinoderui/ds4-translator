# Security findings register

Weaknesses found by a security review of the daemon, the helper libraries and
the packaging, with their status and the tests that guard each fix. Tests named
`Regression_<ID>_*` (gtest) or carrying the id in their check name (shell
scripts under `tests/`) fail if a fix is reverted.

Access policy for the control socket (finding H2): the socket stays
connectable by every user, so read-only use (`status`, `test`, the UI's live
view) works for everyone. Every other command needs root, the daemon's own
user, or membership of the `ds4-translator` group (created by the package /
`make install`; add yourself with `sudo usermod -aG ds4-translator "$USER"`).
The peer is identified with `SO_PEERCRED` on every connection.

| ID | Sev. | Weakness | Status | Guarded by |
|----|------|----------|--------|------------|
| H1 | High | One idle client blocked the single-threaded main loop (blocking `read()` on the accepted fd, no timeout) | **Fixed**: accepted sockets are non-blocking, polled in the main loop, capped at 32 and dropped after 2 s without a command | `test_daemon_security.sh` (H1) |
| H2 | High | World-writable socket with no peer check: any local user could take the pad, inject input, rewrite config, run `udevadm trigger` | **Fixed**: per-command `SO_PEERCRED` check, `ds4-translator` group (`ds4-translator.sysusers`) | `test_daemon_security.sh` (H2, needs passwordless sudo), `ReadOnlyCommands` unit tests |
| H3 | High | `identify` closed a dup'd fd the Bluetooth output pacer could still target (write to a reused fd); unbounded detached threads | **Fixed**: one `identify` at a time; the pacer's pending report for that fd is flushed before `close()` | Not automatable (needs a Bluetooth pad); manual: loop `ds4-ctl identify` while a game drives rumble over BT |
| H4 | High | uevent netlink sender never verified; `HID_ID` matched by substring; devpath tail unvalidated | **Fixed**: sender must be the kernel (`nl_pid == 0`), exact `HID_ID` and sysfs-id shape | `test_uevent.cpp` `Regression_H4_*`, `UeventSender`, `fuzz_uevent.cpp` |
| M1 | Med. | Unbounded `test` subscribers; `accept()` EMFILE spun the loop at 100% CPU | **Fixed**: 16-monitor cap, spare descriptor to shed connections when the table is full | `test_daemon_security.sh` (M1) |
| M2 | Med. | Daemon killed by SIGPIPE when a client reset mid-reply (outside systemd) | **Fixed**: SIGPIPE ignored | `test_daemon_security.sh` (M2) |
| M3 | Med. | Use-after-free: the USB watchdog `libusb_close()`d the handle while the pump thread was in a transfer on it | **Fixed in code, pending a hardware check**: transfers are now asynchronous and cancellable; the watchdog only cancels the in-flight transfer and the handle is closed once, by `usb_hid_transport_close()`, after both threads joined. Still to do: unplug/replug the real pad (see tests/hardware/verify.sh notes) to confirm the multi-minute hang the old close worked around does not return | `tests/component/test_usb_transport.cpp` against a fake libusb (`test-component`, `test-sanitize`, `test-tsan`) |
| L1 | Low | `set-name` accepted control characters and invalid UTF-8 (echoed to other users' terminals) | **Fixed**: shared `validate_name` in the daemon, also applied to names read from the config file | `test_ipc_protocol.cpp` `ValidateName`, `test_daemon_security.sh` (L1) |
| L2 | Low | Config/state/sentinel files written through symlinks, config not atomic | **Fixed**: `ds4fs::write_file_atomic` (temp file with `O_EXCL\|O_NOFOLLOW`, `fsync`, `rename`) | `test_fs_util.cpp`, `test_daemon_security.sh` (L2) |
| L3 | Low | `udev-spoof.so`: NULL `sysattr` crash, shared static uevent buffer, racy lazy init | **Fixed**: NULL guard, per-thread buffer ring, `pthread_once` | `test_udev_spoof.cpp` `Regression_L3_*` |
| L4 | Low | Virtual hidraw nodes `MODE="0666"` | **Fixed**: `0660` (the `uaccess` ACL still grants the logged-in seat user) | `static_checks.sh` (L4) |
| L5 | Low | `ds4-translator.service` had no sandboxing | **Fixed**: `NoNewPrivileges`, `ProtectSystem=yes`, `ProtectHome`, `PrivateTmp`, `RestrictAddressFamilies`, `LockPersonality`, `SystemCallFilter=@system-service @mount bpf` (from a strace over BT and USB), `CapabilityBoundingSet=CAP_SYS_ADMIN` (found with `tests/hardware/bisect-capabilities.sh`) | `static_checks.sh` (L5) and `tests/hardware/verify.sh` |
| L6 | Low | No hardening flags | **Fixed**: stack protector, stack-clash protection, CET, `_FORTIFY_SOURCE=2`, PIE, full RELRO | `static_checks.sh` (L6) |
| L7 | Low | CI supply chain | **Fixed**: actions pinned to commit SHAs, bpftool SHA-256 verified, `permissions:` everywhere, the signing key is no longer present while the project's build scripts run | `static_checks.sh` (L7) |
| L8 | Low | UI single-instance socket fell back to an unchecked `/tmp` directory | **Fixed**: directory created 0700 and verified (directory, owned by us, no group/other access) before use | Manual (needs Qt and a second user): pre-create `/tmp/ds4-translator-ui-<uid>` as another user with `XDG_RUNTIME_DIR` unset |

## Keeping them fixed
- A "known issue" check (`known_issue` in the shell helpers) passes while a
  weakness exists and fails with `FIXED?` once it is gone. The two remaining
  `L5` directives use it, so adding them forces this table to be updated.
- New findings: add a row, write the failing test first, then fix.
