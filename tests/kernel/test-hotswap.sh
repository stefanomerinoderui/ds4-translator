#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Bluetooth/USB hot-swap with the real daemon and the driver hide method, run
# as root inside the VM (run-vm.sh). Two uhid fake pads share one MAC, the way
# one controller shows up twice when its cable is plugged in while it is paired:
# the Bluetooth one sends X=16, the USB one X=17, so what the emulated pad shows
# says which connection the daemon is reading.
#
#   test-hotswap.sh <hid-ds4-translator.ko> <fake-pad> <virtual-check> <ds4-translator> <ds4-ctl>
#
# The daemon normally treats a USB-bus uhid device as its own emulated pad;
# DS4_TEST_UHID_USB_IS_PHYSICAL lets the fake USB pad count as real hardware,
# and the kernel backend keeps the emulated pad out of uhid.

set -u
KO=${1:?module path}
FAKE=${2:?fake-pad path}
VCHECK=${3:?virtual-check path}
DAEMON=${4:?ds4-translator path}
DS4CTL=${5:?ds4-ctl path}
MAC_PAD=AA:BB:CC:00:00:21

# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

DS4_NAME="Sony Computer Entertainment Wireless Controller"
DS5_NAME="Sony Interactive Entertainment DualSense Wireless Controller"
BT_STATE='x=16 y=32 rx=48 ry=64 z=80 rz=96 hat_x=1 hat_y=0 south=1'
USB_STATE='x=17 y=32 rx=48 ry=64 z=80 rz=96 hat_x=1 hat_y=0 south=1'
usb_pid=
daemon_pid=

# Polls the emulated pad called $1 for up to $3 seconds until it shows $2.
pad_shows() { # <name> <state> [seconds]
    "$VCHECK" --observe --name "$1" --hold "${3:-6}" 2>&1 | grep -q "^state $2\$"
}

start_usb() {
    rm -f /tmp/fake-usb.log
    "$FAKE" --bus usb --mac "$MAC_PAD" --count 20 --fixed 2 --stream 1 >/tmp/fake-usb.log 2>&1 &
    usb_pid=$!
}

unplug_usb() {
    kill "$usb_pid" 2>/dev/null
    wait "$usb_pid" 2>/dev/null
    usb_pid=
}

stop_daemon() {
    kill "$daemon_pid" 2>/dev/null
    wait "$daemon_pid" 2>/dev/null
    daemon_pid=
}

# Waits up to ~15 s for the daemon log to contain $1 at least $2 times.
wait_log() { # <text> [count]
    local _
    for _ in $(seq 150); do
        [ "$(grep -c -- "$1" /tmp/daemon.log)" -ge "${2:-1}" ] && return 0
        sleep 0.1
    done
    return 1
}

cleanup_hotswap() {
    unplug_usb
    stop_daemon
    stop_pads
}
trap 'cleanup_hotswap; cleanup' EXIT

dmesg_mark=$(dmesg | wc -l)
mount | grep -q debugfs || mount -t debugfs none /sys/kernel/debug
modprobe uhid || { echo "uhid unavailable"; exit 1; }
modprobe hid_playstation || { echo "hid_playstation unavailable"; exit 1; }
rmmod hid_ds4_translator 2>/dev/null
insmod "$KO" || { echo "insmod failed"; exit 1; }

export DS4_RUNTIME_DIR=/tmp/ds4d DS4_STATE_DIR=/tmp/ds4d/state DS4_CONFIG_PATH=/tmp/ds4d/ds4-translator.conf
export DS4_TEST_UHID_USB_IS_PHYSICAL=1
mkdir -p "$DS4_RUNTIME_DIR"

# One scenario per run of the daemon, with both types on <backend>; the rest of
# the arguments are a ds4-ctl command to run while on USB, before the cable is
# pulled.
scenario() { # <title> <backend> [ds4-ctl command...]
    local title=$1 backend=$2
    shift 2
    echo "# $title ($backend backend)"
    printf 'hide_method=driver\nbackend_ds4=%s\nbackend_dualsense=%s\n' "$backend" "$backend" >"$DS4_CONFIG_PATH"
    rm -f /tmp/daemon.log
    start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "Bluetooth pad did not become ready"
    "$DAEMON" -t ds4 >/tmp/daemon.log 2>&1 &
    daemon_pid=$!
    check "the emulated pad follows Bluetooth" pad_shows "$DS4_NAME" "$BT_STATE"

    # Every X the game sees across the switch: 16 (Bluetooth), then 17 (USB),
    # never 128, the centred stick of a neutral report.
    "$VCHECK" --observe --name "$DS4_NAME" --watch --hold 8 >/tmp/watch.log 2>&1 &
    local watcher=$!
    start_usb
    check "plugging the cable switches to USB" wait_log 'switching from Bluetooth'
    check "the emulated pad follows USB" pad_shows "$DS4_NAME" "$USB_STATE"
    wait "$watcher"
    check "the switch reaches the game" grep -qx 'abs_x=17' /tmp/watch.log
    check "with no neutral input in between" bash -c "! grep -qx 'abs_x=128' /tmp/watch.log"

    local name=$DS4_NAME
    if [ $# -gt 0 ]; then
        "$DS4CTL" "$@" >/tmp/ctl.log 2>&1
        [ "$1" = set-type ] && [ "$2" = dualsense ] && name=$DS5_NAME
        check "after '$*' the emulated pad still follows USB" pad_shows "$name" "$USB_STATE" 10
    fi

    unplug_usb
    check "pulling the cable goes back to Bluetooth" pad_shows "$name" "$BT_STATE" 10

    stop_daemon
    stop_pads
}

# uhid is left out: with the test hook its emulated pad would pass for a
# physical one. functionfs needs dummy_hcd and libcomposite in the guest.
backends=(kernel)
modprobe dummy_hcd 2>/dev/null && modprobe libcomposite 2>/dev/null && backends+=(functionfs)
for backend in "${backends[@]}"; do
    scenario "Bluetooth, then USB, then the cable is pulled" "$backend"
    scenario "the same with an emulation type change while on USB" "$backend" set-type dualsense
done

# Turning emulation on from "none" while a game is running: the module taking
# the pad over is the only remove the game may see. The daemon used to unbind
# and rebind the pad as well, so it vanished, came back and vanished again
# within a second, which Wine's HID bus did not survive.
echo "# none, then ds4, with the pad on hid-playstation in between (kernel backend)"
printf 'hide_method=driver\nbackend_ds4=kernel\nbackend_dualsense=kernel\n' >"$DS4_CONFIG_PATH"
rm -f /tmp/daemon.log
start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "Bluetooth pad did not become ready"
"$DAEMON" -t none >/tmp/daemon.log 2>&1 &
daemon_pid=$!
check "the daemon starts with emulation off" wait_log 'Physical controller left untouched'
phys_id=$(basename "$(hid_dev_by_mac "$MAC_PAD")")
# How often hid-playstation has bound the physical pad so far.
phys_binds() { dmesg | grep -c "playstation $phys_id: Registered"; }
binds_before=$(phys_binds)
"$DS4CTL" set-type ds4 >/tmp/ctl.log 2>&1
check "the emulated pad shows up" pad_shows "$DS4_NAME" "$BT_STATE" 10
check "the pad is not unbound and rebound on top of the takeover" \
    bash -c "! grep -q 'Forcing physical controller re-enumeration' /tmp/daemon.log"
binds_after=$(phys_binds)
echo "# hid-playstation bound $phys_id $((binds_after - binds_before)) more time(s)"
check "the physical pad does not come back on hid-playstation in between" \
    test "$binds_after" -eq "$binds_before"
stop_daemon
stop_pads

unset DS4_RUNTIME_DIR DS4_STATE_DIR DS4_CONFIG_PATH DS4_TEST_UHID_USB_IS_PHYSICAL

echo "# no kernel splats across the whole run"
check "no oops, KASAN/KFENCE report, slab corruption or lockdep splat in dmesg" \
    bash -c "! dmesg | tail -n +$((dmesg_mark + 1)) | grep -Eq 'BUG:|Oops|KASAN|KFENCE|Poison overwritten|Redzone|Object already free|WARNING: CPU|WARNING: .*lock|possible circular'"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
