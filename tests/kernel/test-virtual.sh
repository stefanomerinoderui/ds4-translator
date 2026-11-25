#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Exit criteria for the module's virtual controller (the "kernel" emulation
# backend), run as root inside the VM (run-vm.sh). A uhid fake Bluetooth pad
# streams a known input state; the module translates it and hid-playstation, the
# real driver, binds the virtual pad -- so what it exposes on evdev, and the
# rumble it sends back, are the oracle.
#
#   test-virtual.sh <hid-ds4-translator.ko> <fake-pad> <virtual-check> <ds4-translator> <ds4-ctl>

set -u
KO=${1:?module path}
FAKE=${2:?fake-pad path}
VCHECK=${3:?virtual-check path}
DAEMON=${4:?ds4-translator path}
DS4CTL=${5:?ds4-ctl path}
MAC_PAD=AA:BB:CC:00:00:11
MAC_VIRT=74:e7:d6:3a:47:e8	# the daemon's default emulated MAC

# shellcheck source=lib.sh
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

# What a game writes to the emulated pad's hidraw node, USB or Bluetooth
# layout, has to reach the pad as rumble (strong 0x40 = 64, weak 0x20 = 32).
for type in 1 2; do
    for layout in usb bt; do
        name=$([ "$type" = 1 ] && echo DS4 || echo DualSense)
        echo "# $name: rumble written to hidraw in the $layout layout"
        start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
        start_virtual --type "$type" --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --hidraw-rumble "$layout" --hold 3
        check "the report was written" wait_virtual "^hidraw rumble ($layout) sent"
        check "it reaches the pad" bash -c \
            "for _ in \$(seq 50); do grep -q 'output rnum=11 len=78 crc=ok motor_right=32 motor_left=64' /tmp/fake-pad.$MAC_PAD.log && exit 0; sleep 0.1; done; exit 1"
        stop_virtual
        stop_pads
    done
done

# SDL, which most games sit on, through its HIDAPI driver (hidraw) and its evdev
# one: it must find the emulated pad as the Sony pad it is, and a rumble it
# plays must reach the pad.
SDL_RUMBLE="$(dirname "$VCHECK")/sdl-rumble"
if [ -x "$SDL_RUMBLE" ]; then
    for type in 1 2; do
        for mode in hidapi evdev; do
            name=$([ "$type" = 1 ] && echo DS4 || echo DualSense)
            echo "# $name through SDL's $mode driver"
            start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
            start_virtual --type "$type" --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --hold 12
            wait_virtual '^evdev found'
            before=$(grep -c '^output' "/tmp/fake-pad.$MAC_PAD.log")
            "$SDL_RUMBLE" "$mode" 054c >/tmp/sdl.log 2>&1
            check "SDL finds and opens it as a Sony pad" grep -q "vendor=054c product=$([ "$type" = 1 ] && echo 05c4 || echo 0ce6)" /tmp/sdl.log
            check "SDL accepts the rumble" grep -q '^sdl rumble: accepted' /tmp/sdl.log
            check "it reaches the pad" bash -c \
                "[ \$(grep -c '^output.*motor_right=[1-9]\|^output.*motor_left=[1-9]' /tmp/fake-pad.$MAC_PAD.log) -gt 0 ] && [ \$(grep -c '^output' /tmp/fake-pad.$MAC_PAD.log) -gt $before ]"
            stop_virtual
            stop_pads
        done
    done
else
    echo "# SDL checks skipped: no SDL2 development files when the tools were built"
fi

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

echo "# the real daemon with the kernel backend"
insmod "$KO" || { echo "insmod failed"; exit 1; }
export DS4_RUNTIME_DIR=/tmp/ds4d DS4_STATE_DIR=/tmp/ds4d/state DS4_CONFIG_PATH=/tmp/ds4d/ds4-translator.conf
mkdir -p "$DS4_RUNTIME_DIR"
printf 'hide_method=driver\nbackend_ds4=kernel\n' >"$DS4_CONFIG_PATH"
DS4_NAME="Sony Computer Entertainment Wireless Controller"
# Reads the daemon's pad (by name) until it shows the pad's state, for 3 s.
daemon_pad_shows() { # <state>
    "$VCHECK" --observe --name "$DS4_NAME" --hold 3 2>&1 | grep -q "^state $1\$"
}
start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
"$DAEMON" -t ds4 >/tmp/daemon.log 2>&1 &
daemon_pid=$!
check "the daemon asked the module for the virtual controller" \
    bash -c "for _ in \$(seq 100); do grep -q 'created by the kernel module' /tmp/daemon.log && exit 0; sleep 0.1; done; exit 1"
check "hid-playstation binds the module's virtual pad" wait_driver "$MAC_VIRT" "$FALLBACK"
check "the daemon's pad shows the physical pad's state" daemon_pad_shows "$FIXED_STATE"
check "the daemon did not fall back" bash -c "! grep -q 'Falling back' /tmp/daemon.log"

"$DS4CTL" set-backend ds4 uhid >/tmp/ctl.log 2>&1
check "switching to uhid takes the kernel pad down and brings one up" \
    bash -c "for _ in \$(seq 100); do grep -q 'created via UHID' /tmp/daemon.log && exit 0; sleep 0.1; done; exit 1"
check "the uhid pad shows the physical pad's state" daemon_pad_shows "$FIXED_STATE"
"$DS4CTL" set-backend ds4 kernel >/tmp/ctl.log 2>&1
check "and back to kernel" \
    bash -c "for _ in \$(seq 100); do [ \$(grep -c 'created by the kernel module' /tmp/daemon.log) -ge 2 ] && exit 0; sleep 0.1; done; exit 1"
check "the kernel pad shows the physical pad's state again" daemon_pad_shows "$FIXED_STATE"

kill -KILL "$daemon_pid"
wait "$daemon_pid" 2>/dev/null
check "a killed daemon takes its virtual pad with it" \
    bash -c "for _ in \$(seq 50); do ! grep -qi '^HID_UNIQ=$MAC_VIRT\$' /sys/bus/hid/devices/*/uevent 2>/dev/null && exit 0; sleep 0.1; done; exit 1"
check "and returns the physical pad" wait_driver "$MAC_PAD" "$FALLBACK"
stop_pads
unset DS4_RUNTIME_DIR DS4_STATE_DIR DS4_CONFIG_PATH
check "rmmod works once the daemon is gone" rmmod hid_ds4_translator

echo "# no kernel splats across the whole run"
check "no oops, KASAN/KFENCE report, slab corruption or lockdep splat in dmesg" \
    bash -c "! dmesg | tail -n +$((dmesg_mark + 1)) | grep -Eq 'BUG:|Oops|KASAN|KFENCE|Poison overwritten|Redzone|Object already free|WARNING: CPU|WARNING: .*lock|possible circular'"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
