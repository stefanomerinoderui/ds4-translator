#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Exit criteria for the module's virtual controller (the "kernel" emulation
# backend), run as root inside the VM (run-vm.sh). A uhid fake Bluetooth pad
# streams a known input state; the module translates it and hid-playstation, the
# real driver, binds the virtual pad -- so what it exposes on evdev, and the
# rumble it sends back, are the oracle.
#
#   test-virtual.sh <hid-ds4-translator.ko> <fake-pad> <virtual-check>

set -u
KO=${1:?module path}
FAKE=${2:?fake-pad path}
VCHECK=${3:?virtual-check path}
MAC_PAD=AA:BB:CC:00:00:11
MAC_VIRT=74:e7:d6:3a:47:e8	# the daemon's default emulated MAC

. "$(dirname "$0")/lib.sh"

# Runs virtual-check in the background; its output goes to /tmp/virtual.log.
start_virtual() { # <args>
    rm -f /tmp/virtual.log
    "$VCHECK" "$@" >/tmp/virtual.log 2>&1 &
    attach_pid=$!
}

# Waits up to ~10 s for $1 to appear in the virtual-check output.
wait_virtual() {
    local _
    for _ in $(seq 100); do
        grep -q "$1" /tmp/virtual.log && return 0
        sleep 0.1
    done
    return 1
}

stop_virtual() {
    kill "$attach_pid" 2>/dev/null
    wait "$attach_pid" 2>/dev/null
    attach_pid=
}

# The state the fake pad sends with --fixed (see fake-pad.c), as the virtual
# pad's gamepad must show it.
FIXED_STATE='x=16 y=32 rx=48 ry=64 z=80 rz=96 hat_x=1 hat_y=0 south=1'
NEUTRAL_STATE='x=128 y=128 rx=128 ry=128 z=0 rz=0 hat_x=0 hat_y=0 south=0'

dmesg_mark=$(dmesg | wc -l)

mount | grep -q debugfs || mount -t debugfs none /sys/kernel/debug
modprobe uhid || { echo "uhid unavailable"; exit 1; }
modprobe hid_playstation || { echo "hid_playstation unavailable"; exit 1; }
rmmod hid_ds4_translator 2>/dev/null
insmod "$KO" || { echo "insmod failed"; exit 1; }

# 1 is a DualShock 4, 2 a DualSense (DS4T_VIRTUAL_*).
for type in 1 2; do
    name=$([ "$type" = 1 ] && echo DS4 || echo DualSense)
    echo "# $name virtual controller follows a Bluetooth pad"
    start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
    check "pad starts on the regular driver" wait_driver "$MAC_PAD" "$FALLBACK"

    start_virtual --type "$type" --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --rumble --hold 4
    check "virtual controller created" wait_virtual '^created following=0005'
    check "hid-playstation binds it" wait_driver "$MAC_VIRT" "$FALLBACK"
    check "its gamepad shows up on evdev" wait_virtual '^evdev found'
    check "the pad is claimed by the module" wait_driver "$MAC_PAD" "$DRIVER"
    check "translated input reaches the gamepad unchanged" \
        wait_virtual "^state $FIXED_STATE\$"
    check "rumble requested through evdev" wait_virtual '^rumble sent'
    check "rumble reaches the pad as a Bluetooth report with a valid CRC" bash -c \
        "for _ in \$(seq 50); do grep -Eq 'output rnum=11 len=78 crc=ok (motor_right=255 motor_left=0|motor_right=0 motor_left=255)' /tmp/fake-pad.$MAC_PAD.log && exit 0; sleep 0.1; done; exit 1"
    check "no output report with a bad CRC" bash -c "! grep -q 'crc=bad' /tmp/fake-pad.$MAC_PAD.log"

    stop_virtual
    check "closing control removes the virtual controller" \
        bash -c "for _ in \$(seq 50); do ! grep -qi '^HID_UNIQ=$MAC_VIRT\$' /sys/bus/hid/devices/*/uevent 2>/dev/null && exit 0; sleep 0.1; done; exit 1"
    check "and the pad goes back to the regular driver" wait_driver "$MAC_PAD" "$FALLBACK"
    stop_pads
done

echo "# a virtual controller with no pad moves on injected input"
start_virtual --type 1 --mac "$MAC_VIRT" --inject --hold 2
check "created without a pad" wait_virtual '^created following=-'
check "injected report reaches the gamepad" \
    wait_virtual '^state x=119 y=136 rx=153 ry=170 z=187 rz=204 hat_x=0 hat_y=0 south=1$'
stop_virtual

echo "# unplugging the followed pad leaves a neutral gamepad"
start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
start_virtual --type 1 --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --hold 4
check "follows the pad" wait_virtual "^state $FIXED_STATE\$"
stop_pads
check "everything is released when the pad goes away" \
    wait_virtual "^final $NEUTRAL_STATE\$"
stop_virtual

echo "# a second virtual controller is refused, and the module still unloads"
start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
start_virtual --type 1 --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --hold 3
wait_virtual '^evdev found'
stop_virtual
stop_pads
check "rmmod works once nothing is attached" rmmod hid_ds4_translator

echo "# no kernel splats across the whole run"
check "no oops, KASAN/KFENCE report, slab corruption or lockdep splat in dmesg" \
    bash -c "! dmesg | tail -n +$((dmesg_mark + 1)) | grep -Eq 'BUG:|Oops|KASAN|KFENCE|Poison overwritten|Redzone|Object already free|WARNING: CPU|WARNING: .*lock|possible circular'"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
