#!/usr/bin/env bash
# Integration test: run the real daemon unprivileged against a scratch
# directory (DS4_RUNTIME_DIR / DS4_STATE_DIR / DS4_CONFIG_PATH) with emulation
# off, and drive it with the real ds4-ctl. No controller, no root.
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
start_daemon -t none
pass "daemon created its control socket in the scratch dir"

check "status reports emulation off" "Virtual Emulation: None" "$CTL" status
check "set-hide-method persists" "Hide method set to unlink" "$CTL" set-hide-method unlink
check "status reflects hide method" "Hide Method: unlink" "$CTL" status
if grep -q '^hide_method=unlink$' "$DS4_CONFIG_PATH"; then
    pass "config file written to override path"
else
    fail "config file written to override path"
fi

check "set-hide-method accepts driver" "Hide method set to driver" "$CTL" set-hide-method driver
check "status reflects the driver method" "Hide Method: driver" "$CTL" status
check "daemon rejects an unknown hide method" "Unknown command" raw "set-hide-method bogus"

check "set-name stores a custom name" "Test Pad" "$CTL" set-name ds4 "Test Pad"
check "status reflects the name" "DS4 Name: Test Pad" "$CTL" status
check "set-name --reset restores the default" "Sony Computer Entertainment Wireless Controller" "$CTL" set-name ds4 --reset
check "set-backend accepted" "OK" "$CTL" set-backend dualsense functionfs
check "set-mac-squat accepted" "MAC squat" "$CTL" set-mac-squat on

# Daemon-side validation must hold even if a client skips its own checks.
check "daemon rejects an unknown command" "Unknown" raw "bogus-command"
check "daemon rejects an invalid type" "Unknown command" raw "set-type nope"

if daemon_alive; then pass "daemon alive after bad input"; else fail "daemon alive after bad input"; fi

# Garbage and oversized input must not take the daemon down.
raw "$(head -c 100000 /dev/zero | tr '\0' 'A')" >/dev/null 2>&1
raw "$(head -c 100000 /dev/urandom | base64 -w0)" >/dev/null 2>&1
if daemon_alive; then
    pass "daemon alive after oversized/garbage input"
else
    fail "daemon alive after oversized/garbage input"
fi
check "daemon still answers after garbage" "Virtual Emulation" "$CTL" status

# Clean shutdown on SIGTERM removes the socket.
kill -TERM "$DAEMON_PID"; wait "$DAEMON_PID" 2>/dev/null; DAEMON_PID=""
if [ ! -e "$SOCK" ]; then pass "socket removed on shutdown"; else fail "socket removed on shutdown"; fi

finish integration
