#!/usr/bin/env bash
# Security tests for the daemon's attack surface: its Unix socket, config and
# sentinel files. Same scratch-directory setup as test_daemon_ctl.sh -- nothing
# here can reach a production daemon.
#
# Checks marked "known_issue" pin a documented weakness (see
# docs/security-findings.md): they pass while it exists and fail with
# "FIXED?" once it is gone, so the fix must turn them into regression checks.
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

# ------------------------------------------------ L2: files the daemon writes
# The "none" sentinel is written with a plain ofstream, which follows symlinks:
# a root daemon pointed at a planted link writes wherever the link goes.
ln -s "$WORK/victim" "$WORK/ds4-translator.none"
start_daemon -t none
known_issue L2 "daemon follows a symlinked sentinel file" test -e "$WORK/victim"
stop_daemon
rm -f "$WORK/ds4-translator.none" "$WORK/victim" "$SOCK"

# Malformed or hostile config files must not stop the daemon from starting.
{
    printf 'type=%s\n' "$(head -c 5000 /dev/zero | tr '\0' 'x')"
    printf 'garbage line with no equals\n==\n=value\nkey=\n'
    printf 'name_ds4=%s\n' "$(head -c 200 /dev/zero | tr '\0' 'N')"
    printf 'name_ds4=second\nhide_method=bogus\nbackend_ds4=bogus\nmac_squat=maybe\n'
    head -c 1048576 /dev/zero | tr '\0' 'L'
    printf '\n'
} >"$DS4_CONFIG_PATH"
start_daemon -t none
check "daemon starts with a malformed/oversized config" "Virtual Emulation" "$CTL" status
stop_daemon
rm -f "$SOCK" "$DS4_CONFIG_PATH"

# ---------------------------------------------------------- start for the rest
start_daemon -t none

# ------------------------------------------------ H1: one idle client stalls it
client idle 6 &
IDLE_PID=$!
sleep 0.5
known_issue H1 "an idle client blocks every other client" \
    bash -c "! timeout 3 '$CTL' status >/dev/null 2>&1"
kill "$IDLE_PID" 2>/dev/null
wait "$IDLE_PID" 2>/dev/null
check "daemon recovers once the idle client leaves" "Virtual Emulation" "$CTL" status

# ------------------------------------------- H2: no access control on the socket
known_issue H2 "socket is world-writable (mode 666)" test "$(stat -c %a "$SOCK")" = 666

if sudo -n true 2>/dev/null && command -v setpriv >/dev/null; then
    chmod 755 "$WORK"
    # Runs the socket client as an unprivileged uid; payload on stdin.
    nobody_client() { sudo -n setpriv --reuid=nobody --regid=nogroup --clear-groups \
        python3 -I "$SOCKCLIENT" "$SOCK" raw; }
    nobody_raw() { printf '%s' "$1" | nobody_client; }
    check "unprivileged user can read status" "Virtual Emulation" nobody_raw status
    known_issue H2 "unprivileged user can change settings (set-hide-method)" \
        bash -c "$(declare -f nobody_client nobody_raw); SOCK='$SOCK'; SOCKCLIENT='$SOCKCLIENT'; \
                 nobody_raw 'set-hide-method legacy' | grep -q 'Hide method'"
    known_issue H2 "unprivileged user can take the physical pad (release-physical)" \
        bash -c "$(declare -f nobody_client nobody_raw); SOCK='$SOCK'; SOCKCLIENT='$SOCKCLIENT'; \
                 nobody_raw 'release-physical' | grep -qi 'release'"
    # Put the scratch daemon back the way later checks expect it.
    "$CTL" set-hide-method unbind >/dev/null 2>&1
    "$CTL" resume-physical >/dev/null 2>&1
else
    skip "H2 as another uid (needs passwordless sudo + setpriv)"
fi

# ----------------------------------------- L1: set-name accepts control characters
esc_name() { raw $'set-name ds4 \x1b]0;pwn\x07' | grep -q 'OK'; }
bad_utf8_name() { raw $'set-name ds4 \xff\xfe' | grep -q 'OK'; }
known_issue L1 "set-name accepts an ESC sequence" esc_name
known_issue L1 "set-name accepts invalid UTF-8" bad_utf8_name
"$CTL" set-name ds4 --reset >/dev/null 2>&1

# ---------------------------------- M2: daemon dies if a client resets mid-reply
client reset 150
sleep 0.5
daemon_dead() { ! daemon_alive; }
known_issue M2 "daemon is killed by SIGPIPE when clients reset the connection" daemon_dead
if ! daemon_alive; then
    DAEMON_PID=""
    restart_daemon -t none
fi

# ------------------------------------------- M1: unbounded `test` subscribers
client subscribe 150 4 >"$WORK/subscribed" &
SUB_PID=$!
sleep 2
FDS=$(find "/proc/$DAEMON_PID/fd" -mindepth 1 2>/dev/null | wc -l)
known_issue M1 "no cap on live-input subscribers (daemon holds >=100 fds)" test "$FDS" -ge 100
wait "$SUB_PID" 2>/dev/null
sleep 1
daemon_alive && pass "daemon alive after subscriber flood" || fail "daemon alive after subscriber flood"
check "daemon answers after subscriber flood" "Virtual Emulation" "$CTL" status

# ------------------------------------- black-box command fuzz (positive check)
REFUSED=$(client fuzz 1500 42)
[ "${REFUSED:-1}" -eq 0 ] && pass "daemon survived 1500 random commands" \
    || fail "daemon refused connections during fuzzing ($REFUSED)"
daemon_alive && pass "daemon alive after fuzzing" || fail "daemon alive after fuzzing"
check "daemon answers after fuzzing" "Virtual Emulation" "$CTL" status

finish security
