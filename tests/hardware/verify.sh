#!/usr/bin/env bash
# Manual verification on a machine with a real controller and the *installed*
# service (not part of CI: it needs hardware). Run as your normal user after
# `sudo make install` (or installing the package) and connecting a pad:
#
#   tests/hardware/verify.sh                 # read-only checks
#   tests/hardware/verify.sh --exercise      # also switches the emulated type
#                                            # and writes the config, then
#                                            # restores both
#
# Checks that the systemd sandbox is really in force, that the daemon still
# works inside it (config writes, uevents, hiding the pad), and that the
# virtual controller's hidraw node is 0660 yet openable by the logged-in user
# through the uaccess ACL.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/assert.sh
. "$HERE/../lib/assert.sh"

CTL="${CTL:-ds4-ctl}"
EXERCISE=0
[ "${1:-}" = "--exercise" ] && EXERCISE=1
UNIT=ds4-translator.service

status_field() { "$CTL" status 2>/dev/null | sed -n "s/^$1: //p"; }
unit_prop() { systemctl show "$UNIT" -p "$1" --value 2>/dev/null; }

# ---------------------------------------------------------------- the service
require "service is active" systemctl is-active --quiet "$UNIT"
PID="$(unit_prop MainPID)"
for prop in NoNewPrivileges ProtectHome PrivateTmp LockPersonality; do
    check "sandbox: $prop=yes" "yes" unit_prop "$prop"
done
check "sandbox: ProtectSystem=yes" "yes" unit_prop ProtectSystem
check "sandbox: RestrictAddressFamilies" "AF_UNIX" unit_prop RestrictAddressFamilies
check "sandbox: SystemCallFilter is set" "@system-service" unit_prop SystemCallFilter
require "sandbox is in force on the running process (NoNewPrivs=1)" \
    grep -q '^NoNewPrivs:[[:space:]]*1' "/proc/$PID/status"
# Namespace links of a root process are unreadable without root: skip then.
if DAEMON_NS="$(readlink "/proc/$PID/ns/mnt" 2>/dev/null)" && [ -n "$DAEMON_NS" ]; then
    require "daemon runs with its own /tmp (PrivateTmp)" \
        test "$DAEMON_NS" != "$(readlink /proc/1/ns/mnt)"
else
    skip "daemon mount namespace (needs root to read; PrivateTmp checked via systemctl)"
fi

# ------------------------------------------------------ daemon works sandboxed
require "ds4-ctl status answers" "$CTL" status
PHYS="$(status_field 'Physical Controller')"
if [ -n "$PHYS" ] && [ "$PHYS" != "None" ]; then
    pass "physical controller detected: $PHYS ($(status_field 'Connection Type'))"
else
    fail "no physical controller detected -- connect the pad first"
fi
require "hid-bpf object is where the service expects it" test -f /usr/lib/ds4-translator/hid-bpf-transport.bpf.o -o -f /usr/local/lib/ds4-translator/hid-bpf-transport.bpf.o
# Only the current run of the service: earlier invocations (an older unit, a
# crashed start) would report errors that are already gone.
sandbox_errors() {
    journalctl "_SYSTEMD_INVOCATION_ID=$(unit_prop InvocationID)" --no-pager 2>/dev/null |
        grep -iE 'read-only file system|operation not permitted|permission denied|EPERM|EROFS' |
        grep -vE 'UDC: Permission denied' | head -5
}
if [ -n "$(sandbox_errors)" ]; then
    fail "journal shows sandbox-related errors:"
    sandbox_errors
else
    pass "no sandbox-related errors in the current service run"
fi

# ----------------------------------------------------------- access control
if getent group ds4-translator | cut -d: -f4 | tr ',' '\n' | grep -qx "$(id -un)" || [ "$(id -u)" -eq 0 ]; then
    # The daemon checks the group database, not the shell, so no re-login is needed here.
    pass "you may change settings (root or ds4-translator group)"
    CAN_CHANGE=1
else
    CAN_CHANGE=0
    check "without the group, changes are refused" "Permission denied" "$CTL" set-mac-squat on
    echo "note: you are not in the ds4-translator group ('sudo usermod -aG ds4-translator \$USER');"
    echo "      --exercise and the node checks are skipped until then."
fi
check "status works for everyone" "Virtual Emulation" "$CTL" status

# ----------------------------------------------------- virtual node permissions
# find_virtual_hidraw: prints the hidraw node of the daemon's uhid device.
find_virtual_hidraw() {
    local uevent node
    for uevent in /sys/class/hidraw/hidraw*/device/uevent; do
        grep -qE 'HID_ID=0003:0000054C:(000005C4|00000CE6)' "$uevent" 2>/dev/null || continue
        readlink -f "$(dirname "$uevent")" | grep -q '/devices/virtual/misc/uhid/' || continue
        node="/dev/$(basename "$(dirname "$(dirname "$uevent")")")"
        echo "$node"
        return 0
    done
    return 1
}

# physical_pad_hidraw: hidraw nodes of a real (non-uhid) DualShock 4.
physical_pad_hidraw() {
    local uevent
    for uevent in /sys/class/hidraw/hidraw*/device/uevent; do
        grep -qE 'HID_ID=[0-9A-F]{4}:0000054C:(000005C4|000009CC)' "$uevent" 2>/dev/null || continue
        readlink -f "$(dirname "$uevent")" | grep -q '/devices/virtual/misc/uhid/' && continue
        echo "/dev/$(basename "$(dirname "$(dirname "$uevent")")")"
    done
}
export -f physical_pad_hidraw

check_virtual_node() {
    local node mode
    node="$(find_virtual_hidraw)" || { fail "no virtual controller hidraw node (is a uhid-backed type active?)"; return; }
    mode="$(stat -c %a "$node")"
    check "virtual node $node is mode 660 (not 666)" "660" echo "$mode"
    require "uaccess ACL grants $USER access on $node" bash -c "getfacl -p '$node' 2>/dev/null | grep -q '^user:$USER:rw'"
    require "$USER can open $node (read)" bash -c "exec 3<'$node'"
    require "other users cannot open $node" \
        bash -c "! sudo -n -u nobody test -r '$node' 2>/dev/null || ! sudo -n true 2>/dev/null"
}

if [ "$EXERCISE" -eq 1 ] && [ "$CAN_CHANGE" -eq 1 ]; then
    PREVIOUS_DISPLAY="$(status_field 'Virtual Emulation')"
    case "$PREVIOUS_DISPLAY" in
        "DualShock 4") PREVIOUS=ds4 ;; "DualSense") PREVIOUS=dualsense ;;
        "Xbox 360") PREVIOUS=xbox360 ;; "Xbox One S") PREVIOUS=xboxone ;;
        "Xbox Series X|S") PREVIOUS=xboxseries ;; "Hidden") PREVIOUS=hidden ;; *) PREVIOUS=none ;;
    esac
    echo "exercising: switching $PREVIOUS_DISPLAY -> DualSense (uhid), will restore $PREVIOUS"

    # Config writes must work under ProtectSystem=yes.
    OLD_SQUAT="$(status_field 'MAC Squat' | awk '{print $1}')"
    NEW_SQUAT=$([ "$OLD_SQUAT" = on ] && echo off || echo on)
    check "config write works inside the sandbox" "MAC squat" "$CTL" set-mac-squat "$NEW_SQUAT"
    require "/etc/ds4-translator.conf reflects it" grep -q "mac_squat=$([ "$NEW_SQUAT" = on ] && echo true || echo false)" /etc/ds4-translator.conf
    "$CTL" set-mac-squat "$OLD_SQUAT" >/dev/null 2>&1

    "$CTL" set-type dualsense >/dev/null 2>&1
    sleep 4
    check_virtual_node
    require "physical pad has no hidraw node left (hidden from users)" \
        bash -c "! physical_pad_hidraw | grep -q ."
    "$CTL" set-type "$PREVIOUS" >/dev/null 2>&1
    sleep 2
    check "emulation restored to $PREVIOUS_DISPLAY" "$PREVIOUS_DISPLAY" "$CTL" status
else
    find_virtual_hidraw >/dev/null 2>&1 && check_virtual_node || skip "virtual hidraw node (use --exercise, or switch to a uhid-backed type)"
fi

finish hardware
