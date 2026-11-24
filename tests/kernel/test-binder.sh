#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# M1 exit criteria for hid-ds4-translator, run as root against a uhid fake pad
# (see fake-pad.c). Meant to run inside a throwaway VM (run-vm.sh): it loads
# and unloads kernel modules.
#
#   test-binder.sh <path/to/hid-ds4-translator.ko> <path/to/fake-pad>

set -u
KO=${1:?module path}
FAKE=${2:?fake-pad path}
DRIVER=hid-ds4-translator
FALLBACK=playstation   # driver name; the module is hid_playstation
MAC_A=AA:BB:CC:00:00:01
MAC_B=AA:BB:CC:00:00:02
DEBUGFS=/sys/kernel/debug/ds4t

pass=0
fail=0
pad_pids=()

ok()   { echo "ok   - $1"; pass=$((pass + 1)); }
nok()  { echo "FAIL - $1"; fail=$((fail + 1)); }
check() { local desc=$1; shift; if "$@"; then ok "$desc"; else nok "$desc"; fi; }

cleanup() {
    kill "${pad_pids[@]}" 2>/dev/null
    wait 2>/dev/null
    rmmod hid_ds4_translator 2>/dev/null
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

start_pad() { # <bus> <mac>
    local log="/tmp/fake-pad.$2.log"
    rm -f "$log"
    "$FAKE" --bus "$1" --mac "$2" --count 20 >"$log" 2>&1 &
    pad_pids+=($!)
    local _
    for _ in $(seq 50); do
        grep -q ready "$log" && return 0
        sleep 0.1
    done
    return 1
}

stop_pads() {
    kill "${pad_pids[@]}" 2>/dev/null
    wait 2>/dev/null
    pad_pids=()
    sleep 0.3
}

has_node() { # <hid device path> <name under it>
    [ -e "$1/$2" ]
}

mount | grep -q debugfs || mount -t debugfs none /sys/kernel/debug
modprobe uhid || { echo "uhid unavailable"; exit 1; }
modprobe hid_playstation || { echo "hid_playstation unavailable"; exit 1; }

echo "# 1. module loaded first: pad binds to $DRIVER and exposes no nodes"
insmod "$KO" || { echo "insmod failed"; exit 1; }
start_pad usb "$MAC_A"
check "USB pad bound to $DRIVER" wait_driver "$MAC_A" "$DRIVER"
dev=$(hid_dev_by_mac "$MAC_A")
check "no hidraw node" bash -c "! ls '$dev'/hidraw/* >/dev/null 2>&1"
check "no input node"  bash -c "! ls '$dev'/input/* >/dev/null 2>&1"
check "no power_supply" bash -c "! ls /sys/class/power_supply/ps-controller-battery-* >/dev/null 2>&1"
dump=$DEBUGFS/$(basename "$dev")
check "debugfs ring has the reports" grep -q '^total [1-9]' "$dump"
# The driver symlink appears before probe finishes, so give the log a moment.
check "probe logged the claim" \
    bash -c "sleep 0.5; dmesg | grep -qi 'claimed USB pad $MAC_A'"
check "no calibration warning" bash -c "! dmesg | grep -q 'calibration report .* failed'"

echo "# 2. unload hands the pad back"
rmmod hid_ds4_translator
check "pad rebound to $FALLBACK" wait_driver "$MAC_A" "$FALLBACK"
stop_pads

echo "# 3. module loaded after the pad: pad is taken over"
start_pad usb "$MAC_A"
check "pad starts on $FALLBACK" wait_driver "$MAC_A" "$FALLBACK"
insmod "$KO"
check "pad taken over by $DRIVER" wait_driver "$MAC_A" "$DRIVER"
stop_pads

echo "# 4. pad plugged in after the module, with $FALLBACK loaded first"
start_pad bt "$MAC_B"
check "Bluetooth pad ends up on $DRIVER" wait_driver "$MAC_B" "$DRIVER"
stop_pads
rmmod hid_ds4_translator

echo "# 5. ignore_mac keeps the emulated pad on $FALLBACK"
insmod "$KO" ignore_mac="$MAC_A"
start_pad usb "$MAC_A"
check "ignored MAC stays on $FALLBACK" wait_driver "$MAC_A" "$FALLBACK"
start_pad usb "$MAC_B"
check "other MAC still claimed" wait_driver "$MAC_B" "$DRIVER"
stop_pads

echo "# 6. no kernel splats across the whole run"
check "no BUG/KASAN/lockdep in dmesg" \
    bash -c "! dmesg | grep -Eq 'BUG:|KASAN|WARNING: .*lock|possible circular'"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
