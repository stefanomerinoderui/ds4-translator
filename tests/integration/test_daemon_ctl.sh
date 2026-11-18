#!/usr/bin/env bash
# Integration test: run the real daemon unprivileged against a scratch
# directory (DS4_RUNTIME_DIR / DS4_STATE_DIR / DS4_CONFIG_PATH) with emulation
# off, and drive it with the real ds4-ctl. No controller, no root.
set -u
BUILD_DIR="${1:-build}"
DAEMON="$BUILD_DIR/ds4-translator"
CTL="$BUILD_DIR/ds4-ctl"

# Safety: with a real daemon on this machine, a test daemon could race it for
# the physical controller. CI runners never have one.
if [ -S /run/ds4-translator.sock ] && [ "${DS4_TEST_FORCE:-}" != "1" ]; then
    echo "SKIP: a real ds4-translator is running here (set DS4_TEST_FORCE=1 to run anyway)"
    exit 0
fi

WORK="$(mktemp -d)"
export DS4_RUNTIME_DIR="$WORK" DS4_STATE_DIR="$WORK/state" DS4_CONFIG_PATH="$WORK/ds4-translator.conf"
SOCK="$WORK/ds4-translator.sock"
DAEMON_PID=""
fails=0

cleanup() {
    [ -n "$DAEMON_PID" ] && kill "$DAEMON_PID" 2>/dev/null && wait "$DAEMON_PID" 2>/dev/null
    rm -rf "$WORK"
}
trap cleanup EXIT

pass() { printf 'ok   %s\n' "$1"; }
fail() { printf 'FAIL %s\n' "$1"; fails=$((fails + 1)); }

# check <description> <expected-substring> <cmd...>: output must contain it.
check() {
    local desc="$1" want="$2"; shift 2
    local out
    out="$("$@" 2>&1)"
    if grep -qF -- "$want" <<<"$out"; then pass "$desc"; else fail "$desc (wanted '$want', got: $out)"; fi
}

"$DAEMON" -t none >"$WORK/daemon.log" 2>&1 &
DAEMON_PID=$!
for _ in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.1; done
if [ ! -S "$SOCK" ]; then
    fail "daemon created its control socket in the scratch dir"
    cat "$WORK/daemon.log"
    exit 1
fi
pass "daemon created its control socket in the scratch dir"

check "status reports emulation off" "Virtual Emulation: None" "$CTL" status
check "set-hide-method persists" "Hide method set to unlink" "$CTL" set-hide-method unlink
check "status reflects hide method" "Hide Method: unlink" "$CTL" status
grep -q '^hide_method=unlink$' "$DS4_CONFIG_PATH" && pass "config file written to override path" \
    || fail "config file written to override path"

check "set-name stores a custom name" "Test Pad" "$CTL" set-name ds4 "Test Pad"
check "status reflects the name" "DS4 Name: Test Pad" "$CTL" status
check "set-name --reset restores the default" "Sony Computer Entertainment Wireless Controller" "$CTL" set-name ds4 --reset
check "set-backend accepted" "OK" "$CTL" set-backend dualsense functionfs
check "set-mac-squat accepted" "MAC squat" "$CTL" set-mac-squat on

# Daemon-side validation must hold even if a client skips its own checks.
raw() { printf '%s\n' "$1" | python3 -c '
import socket, sys
s = socket.socket(socket.AF_UNIX); s.settimeout(5); s.connect(sys.argv[1])
s.sendall(sys.stdin.buffer.read()); s.shutdown(socket.SHUT_WR)
out = b""
while True:
    chunk = s.recv(4096)
    if not chunk: break
    out += chunk
sys.stdout.write(out.decode(errors="replace"))' "$SOCK"; }

check "daemon rejects an unknown command" "Unknown" raw "bogus-command"
check "daemon rejects an invalid type" "Unknown command" raw "set-type nope"

kill -0 "$DAEMON_PID" 2>/dev/null && pass "daemon alive after bad input" || fail "daemon alive after bad input"

# Garbage and oversized input must not take the daemon down.
raw "$(head -c 100000 /dev/zero | tr '\0' 'A')" >/dev/null 2>&1
raw "$(head -c 100000 /dev/urandom | base64 -w0)" >/dev/null 2>&1
kill -0 "$DAEMON_PID" 2>/dev/null && pass "daemon alive after oversized/garbage input" \
    || fail "daemon alive after oversized/garbage input"
check "daemon still answers after garbage" "Virtual Emulation" "$CTL" status

# Clean shutdown on SIGTERM removes the socket.
kill -TERM "$DAEMON_PID"; wait "$DAEMON_PID" 2>/dev/null; DAEMON_PID=""
[ ! -e "$SOCK" ] && pass "socket removed on shutdown" || fail "socket removed on shutdown"

[ "$fails" -eq 0 ] && echo "integration: all checks passed" || echo "integration: $fails check(s) failed"
exit "$((fails > 0))"
