#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Helpers shared by the VM test scripts (test-binder.sh, test-virtual.sh,
# test-hotswap.sh): result counting, fake pads and finding what the kernel
# bound them to. Sourced, not run. The including script sets FAKE and may set
# attach_pid.

# For the scripts that source this file; checked on its own, shellcheck cannot
# see them used.
# shellcheck disable=SC2034
{
    DRIVER=hid-ds4-translator
    FALLBACK=playstation   # driver name; the module is hid_playstation
    CONTROL=/dev/ds4-translator/control
}
pass=0
fail=0
pad_pids=()
attach_pid=

ok()   { echo "ok   - $1"; pass=$((pass + 1)); }

# What a kernel bug leaves in dmesg: an oops, a KASAN/KFENCE/UBSAN report, slab
# corruption, a lockdep or debug-objects splat, sleeping in atomic context.
SPLAT_RE='BUG:|Oops|KASAN|KFENCE|UBSAN|Poison overwritten|Redzone|Object already free|WARNING: CPU|WARNING: .*lock|possible circular|sleeping function called|ODEBUG'

# The final check of each suite: no splat since dmesg line $1; on failure the
# splat itself is printed, the only trace of it the run keeps.
check_no_splat() { # <dmesg line mark>
    local new
    new=$(dmesg | tail -n +$(($1 + 1)))
    # Known, upstream, not this module's: hid-playstation's DualSense output
    # worker reports the headset jack while holding its own lock, the reverse
    # of its rumble path's order (lockdep, kernel 6.18). Only when it is the
    # one report and nothing of ours is in it.
    local report
    report=$(grep -E -A80 "$SPLAT_RE" <<<"$new")
    if [ "$(grep -Ec "$SPLAT_RE" <<<"$new")" = 1 ] &&
       grep -q 'dualsense_output_worker' <<<"$report" && grep -q 'dualsense_play_effect' <<<"$report" &&
       ! grep -Eq 'ds4t_|hid_ds4_translator' <<<"$report"; then
        echo "KNOWN - upstream hid-playstation lock order (DualSense jack switch under ps_dev->lock); lockdep is off from here"
        ok "no oops, sanitizer report, slab corruption or lockdep splat in dmesg (but the known one)"
        return
    fi
    if grep -Eq "$SPLAT_RE" <<<"$new"; then
        nok "no oops, sanitizer report, slab corruption or lockdep splat in dmesg"
        grep -E -B2 -A40 "$SPLAT_RE" <<<"$new" | head -n 120
    else
        ok "no oops, sanitizer report, slab corruption or lockdep splat in dmesg"
    fi
}
nok()  { echo "FAIL - $1"; fail=$((fail + 1)); }
check() { local desc=$1; shift; if "$@"; then ok "$desc"; else nok "$desc"; fi; }

# Unloads the gadget module, then the main one. A killed daemon or tool may
# still be releasing the control node (that closes asynchronously, after the
# process is reaped) and so still hold them: retry for a few seconds.
unload_modules() {
    local _
    for _ in $(seq 50); do
        rmmod ds4t_gadget 2>/dev/null
        rmmod hid_ds4_translator 2>/dev/null
        lsmod | grep -q -e '^ds4t_gadget ' -e '^hid_ds4_translator ' || return 0
        sleep 0.1
    done
    return 1
}

cleanup() {
    kill "${pad_pids[@]}" "$attach_pid" 2>/dev/null
    wait 2>/dev/null
    unload_modules
}
trap cleanup EXIT

# Prints the sysfs path of the HID device whose uniq is $1.
hid_dev_by_mac() {
    local dev
    for dev in /sys/bus/hid/devices/*; do
        grep -qi "^HID_UNIQ=$1\$" "$dev/uevent" 2>/dev/null && { echo "$dev"; return 0; }
    done
    return 1
}

bound_driver() {
    local dev
    dev=$(hid_dev_by_mac "$1") || return 1
    [ -L "$dev/driver" ] && basename "$(readlink "$dev/driver")"
}

# Waits up to ~10 s for $1's bound driver to become $2 (a rebind after
# rmmod takes a couple of seconds in the VM).
wait_driver() {
    local _
    for _ in $(seq 100); do
        [ "$(bound_driver "$1")" = "$2" ] && return 0
        sleep 0.1
    done
    return 1
}

# Waits up to ~10 s for $1 to be bound to anything but $2 (or to nothing).
wait_not_driver() {
    local _
    for _ in $(seq 100); do
        [ "$(bound_driver "$1")" != "$2" ] && return 0
        sleep 0.1
    done
    return 1
}

start_pad() { # <bus> <mac> [extra fake-pad args]
    local bus=$1 mac=$2 log="/tmp/fake-pad.$2.log"
    shift 2
    rm -f "$log"
    "$FAKE" --bus "$bus" --mac "$mac" --count 20 "$@" >"$log" 2>&1 &
    pad_pids+=($!)
    local _
    for _ in $(seq 50); do
        grep -q ready "$log" && return 0
        sleep 0.1
    done
    return 1
}

# Only the fake pads: a daemon stand-in may be running on purpose.
stop_pads() {
    kill "${pad_pids[@]}" 2>/dev/null
    wait "${pad_pids[@]}" 2>/dev/null
    pad_pids=()
    sleep 0.3
}

