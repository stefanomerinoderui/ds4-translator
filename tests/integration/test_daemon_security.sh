#!/usr/bin/env bash
# Security regression tests for the daemon's attack surface: its Unix socket,
# config and sentinel files. Same scratch-directory setup as
# test_daemon_ctl.sh -- nothing here can reach a production daemon.
#
# Each block guards the fix of a finding in docs/security-findings.md (the id
# is in the check name).
#
# This script floods the daemon with connections and, when passwordless sudo
# is available, talks to it as another uid. Run it on CI or a disposable
# machine; it refuses to start next to a production daemon unless
# DS4_TEST_FORCE=1 is set.
set -u
# shellcheck disable=SC2034 # BUILD_DIR is read by tests/lib/daemon.sh
BUILD_DIR="${1:-build}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/assert.sh
. "$HERE/../lib/assert.sh"
# shellcheck source=../lib/daemon.sh
. "$HERE/../lib/daemon.sh"

guard_no_production_daemon
setup_scratch

client() { python3 -I "$SOCKCLIENT" "$SOCK" "$@"; }
status_ok() { timeout 3 "$CTL" status 2>&1 | grep -qF "Virtual Emulation"; }

# ------------------------------------------------ L2: files the daemon writes
# The "none" sentinel used to be written with a plain ofstream, which follows
# symlinks: a root daemon pointed at a planted link wrote wherever it went.
ln -s "$WORK/victim" "$WORK/ds4-translator.none"
start_daemon -t none
require "L2 daemon does not write through a symlinked sentinel" bash -c "! test -e '$WORK/victim'"
require "L2 the symlink was replaced by a regular sentinel file" test -f "$WORK/ds4-translator.none" -a ! -L "$WORK/ds4-translator.none"
stop_daemon
rm -f "$WORK/ds4-translator.none" "$WORK/victim" "$SOCK"

# Malformed or hostile config files must not stop the daemon from starting,
# and an oversized/invalid name in one must not be used.
{
    printf 'type=%s\n' "$(head -c 5000 /dev/zero | tr '\0' 'x')"
    printf 'garbage line with no equals\n==\n=value\nkey=\n'
    printf 'name_ds4=%s\n' "$(head -c 200 /dev/zero | tr '\0' 'N')"
    printf 'name_dualsense=bad\033]0;pwn\007name\n'
    printf 'name_ds4=second\nhide_method=bogus\nbackend_ds4=bogus\nmac_squat=maybe\n'
    head -c 1048576 /dev/zero | tr '\0' 'L'
    printf '\n'
} >"$DS4_CONFIG_PATH"
start_daemon -t none
require "daemon starts with a malformed/oversized config" status_ok
require "L1 an invalid name from the config file is ignored" \
    bash -c "! '$CTL' status | grep -q 'DualSense Name: bad'"
stop_daemon
rm -f "$SOCK" "$DS4_CONFIG_PATH"

# ---------------------------------------------------------- start for the rest
start_daemon -t none

# ------------------------------------------------ H1: one idle client stalls it
client idle 6 &
IDLE_PID=$!
sleep 0.5
require "H1 status answers while another client sits idle" status_ok
kill "$IDLE_PID" 2>/dev/null
wait "$IDLE_PID" 2>/dev/null

# Many idle clients (more than the daemon's pending-client cap) must neither
# stall nor kill it, and are dropped after their deadline.
IDLERS=()
for _ in $(seq 1 60); do client idle 5 & IDLERS+=($!); done
sleep 0.5
require "H1 status answers with 60 idle clients connected" status_ok
sleep 3
require "H1 daemon is still up after the idle clients time out" daemon_alive
for pid in "${IDLERS[@]}"; do
    kill "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null
done

# ------------------------------------------- H2: access control on the socket
# The socket stays connectable by everyone (status/test are open); what a peer
# may *do* depends on its credentials.
require "H2 socket is connectable by every user" test "$(stat -c %a "$SOCK")" = 666

if sudo -n true 2>/dev/null && command -v setpriv >/dev/null; then
    chmod 755 "$WORK"
    # Runs the socket client as an unprivileged uid; payload on stdin.
    nobody_raw() {
        printf '%s' "$1" | sudo -n setpriv --reuid=nobody --regid=nogroup --clear-groups \
            python3 -I "$SOCKCLIENT" "$SOCK" raw
    }
    check "H2 unprivileged user can read status" "Virtual Emulation" nobody_raw status
    check "H2 unprivileged user cannot change settings" "Permission denied" nobody_raw "set-hide-method legacy"
    check "H2 unprivileged user cannot take the pad" "Permission denied" nobody_raw "release-physical"
    check "H2 unprivileged user cannot inject input" "Permission denied" nobody_raw "input 00"
    check "H2 unprivileged user cannot set the LED" "Permission denied" nobody_raw "led 1 2 3 4 5"
    check "H2 unprivileged user cannot rename the controller" "Permission denied" nobody_raw "set-name ds4 x"
    require "H2 the denied commands changed nothing" \
        bash -c "! '$CTL' status | grep -q 'Hide Method: legacy'"
    check "H2 the daemon's own user is still allowed" "Hide method" "$CTL" set-hide-method unlink
else
    skip "H2 as another uid (needs passwordless sudo + setpriv)"
fi

# ----------------------------------------- L1: set-name rejects control characters
esc_name_rejected() { raw $'set-name ds4 \x1b]0;pwn\x07' | grep -q 'Error'; }
bad_utf8_rejected() { raw $'set-name ds4 \xff\xfe' | grep -q 'Error'; }
require "L1 set-name rejects an ESC sequence" esc_name_rejected
require "L1 set-name rejects invalid UTF-8" bad_utf8_rejected
check "L1 set-name still accepts a normal UTF-8 name" "OK" "$CTL" set-name ds4 "Mando de Stefán"
"$CTL" set-name ds4 --reset >/dev/null 2>&1

# ---------------------------------- M2: daemon survives clients that reset mid-reply
client reset 150
sleep 0.5
require "M2 daemon survives 150 connection resets" daemon_alive
require "M2 daemon still answers after the resets" status_ok

# ------------------------------------------- M1: bounded `test` subscribers
client subscribe 150 4 >"$WORK/subscribed" &
SUB_PID=$!
sleep 2
FDS=$(find "/proc/$DAEMON_PID/fd" -mindepth 1 2>/dev/null | wc -l)
require "M1 live-monitor subscribers are capped (daemon holds $FDS fds, limit 60)" test "$FDS" -lt 60
require "M1 status answers during the subscriber flood" status_ok
wait "$SUB_PID" 2>/dev/null
sleep 1
require "M1 daemon alive after subscriber flood" daemon_alive

# ---- M1: descriptor exhaustion must not spin the loop at 100% CPU
DAEMON_NOFILE=64 restart_daemon -t none
unset DAEMON_NOFILE
client subscribe 120 6 >/dev/null &
SUB_PID=$!
sleep 2
T0=$(daemon_cpu_ticks)
sleep 2
T1=$(daemon_cpu_ticks)
TICKS_PER_SEC=$(getconf CLK_TCK)
require "M1 daemon is not spinning with a full fd table ($((T1 - T0)) ticks in 2s)" \
    test "$((T1 - T0))" -lt "$((TICKS_PER_SEC))"
wait "$SUB_PID" 2>/dev/null
require "M1 daemon alive after fd exhaustion" daemon_alive
require "M1 status answers after fd exhaustion" status_ok

# ------------------------------------- black-box command fuzz
restart_daemon -t none
REFUSED=$(client fuzz 1500 42)
require "daemon survived 1500 random commands" test "${REFUSED:-1}" -eq 0
require "daemon alive after fuzzing" daemon_alive
require "daemon answers after fuzzing" status_ok

finish security
