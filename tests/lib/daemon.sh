# shellcheck shell=bash
# Scratch-daemon helpers for the integration scripts. Source, don't execute.
# Requires tests/lib/assert.sh to be sourced first, and BUILD_DIR to be set.
#
# Everything runs against a private directory (DS4_RUNTIME_DIR etc., see
# src/paths.h). Two guards keep a test from ever reaching a production daemon:
# the script refuses to start if one is running, and ds4-ctl/raw are only
# usable after start_daemon has verified the socket lives in the scratch dir.

# shellcheck disable=SC2034 # CTL is used by the scripts that source this file
DAEMON="$BUILD_DIR/ds4-translator"
CTL="$BUILD_DIR/ds4-ctl"
PRODUCTION_SOCKET=/run/ds4-translator.sock

guard_no_production_daemon() {
    if [ -S "$PRODUCTION_SOCKET" ] && [ "${DS4_TEST_FORCE:-}" != "1" ]; then
        echo "SKIP: a real ds4-translator is running here (set DS4_TEST_FORCE=1 to run anyway)"
        exit 0
    fi
}

WORK=""
DAEMON_PID=""

setup_scratch() {
    WORK="$(mktemp -d)"
    export DS4_RUNTIME_DIR="$WORK" DS4_STATE_DIR="$WORK/state" DS4_CONFIG_PATH="$WORK/ds4-translator.conf"
    SOCK="$WORK/ds4-translator.sock"
    trap cleanup_scratch EXIT
}

cleanup_scratch() {
    stop_daemon
    [ -n "$WORK" ] && rm -rf "$WORK"
}

# start_daemon [daemon args...]; fails the script if the socket is not in the
# scratch dir within 5s.
start_daemon() {
    [ -n "${DS4_RUNTIME_DIR:-}" ] && [ "$DS4_RUNTIME_DIR" != "/run" ] || { echo "refusing to run without a scratch DS4_RUNTIME_DIR"; exit 2; }
    "$DAEMON" "$@" >"$WORK/daemon.log" 2>&1 &
    DAEMON_PID=$!
    for _ in $(seq 1 50); do [ -S "$SOCK" ] && return 0; sleep 0.1; done
    fail "daemon created its control socket in the scratch dir"
    cat "$WORK/daemon.log"
    exit 1
}

stop_daemon() {
    if [ -n "$DAEMON_PID" ]; then
        kill "$DAEMON_PID" 2>/dev/null && wait "$DAEMON_PID" 2>/dev/null
        DAEMON_PID=""
    fi
}

daemon_alive() { [ -n "$DAEMON_PID" ] && kill -0 "$DAEMON_PID" 2>/dev/null; }

SOCKCLIENT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/sockclient.py"

# raw <payload>: send one unterminated command exactly as given, bypassing
# ds4-ctl's client-side validation, and print the reply.
raw() { printf '%s' "$1" | python3 -I "$SOCKCLIENT" "$SOCK" raw; }

# restart_daemon [daemon args...]: stop whatever is running and start fresh.
restart_daemon() { stop_daemon; rm -f "$SOCK"; start_daemon "$@"; }

# Daemon CPU time in clock ticks (utime + stime) from /proc.
daemon_cpu_ticks() { awk '{print $14 + $15}' "/proc/$DAEMON_PID/stat"; }
