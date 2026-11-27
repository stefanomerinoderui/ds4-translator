#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# The "kernel-usb" backend: the virtual controller as a USB gadget on
# dummy_hcd (ds4t-gadget.ko), run as root inside the VM (run-vm.sh). A uhid
# fake pad streams a known state; the host side -- usbhid and hid-playstation,
# the real drivers -- binds the gadget as it would a pad plugged in, so what it
# exposes, and the rumble it sends back, are the oracle. Then the ways it can
# go away while in use, each of which must leave no oops, leak or lockdep
# splat behind.
#
#   test-gadget.sh <hid-ds4-translator.ko> <fake-pad> <virtual-check> <ds4-translator> <ds4-ctl>

set -u
KO=${1:?module path}
FAKE=${2:?fake-pad path}
VCHECK=${3:?virtual-check path}
GADGET_KO="$(dirname "$KO")/ds4t-gadget.ko"
MAC_PAD=AA:BB:CC:00:00:31
MAC_VIRT=74:e7:d6:3a:47:e8	# the daemon's default emulated MAC
PRODUCT="ds4t virtual test pad"	# virtual-check's name, the USB product string

# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

FIXED_STATE='x=16 y=32 rx=48 ry=64 z=80 rz=96 hat_x=1 hat_y=0 south=1'
NEUTRAL_STATE='x=128 y=128 rx=128 ry=128 z=0 rz=0 hat_x=0 hat_y=0 south=0'
GADGET_DIR=/sys/kernel/config/usb_gadget/ds4t-busy

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

# The host-side USB device of the gadget, or nothing.
sony_usb_dev() {
    local d
    for d in /sys/bus/usb/devices/*; do
        [ "$(cat "$d/idVendor" 2>/dev/null)" = 054c ] && { echo "$d"; return 0; }
    done
    return 1
}

# Whether the gadget's USB device has attribute $1 equal to $2.
usb_attr_is() { # <attribute> <value>
    local d
    d=$(sony_usb_dev) || return 1
    [ "$(cat "$d/$1")" = "$2" ]
}

wait_usb_gone() {
    local _
    for _ in $(seq 100); do
        sony_usb_dev >/dev/null || return 0
        sleep 0.1
    done
    return 1
}

# Takes the only UDC with a configfs gadget of its own (an ACM serial port).
occupy_udc() {
    modprobe usb_f_acm 2>/dev/null || return 1
    mkdir -p "$GADGET_DIR/functions/acm.usb0" "$GADGET_DIR/configs/c.1" || return 1
    echo 0x1d6b >"$GADGET_DIR/idVendor"
    ln -s "$GADGET_DIR/functions/acm.usb0" "$GADGET_DIR/configs/c.1/" 2>/dev/null
    local u
    for u in /sys/class/udc/*; do
        basename "$u" >"$GADGET_DIR/UDC"
        return
    done
    return 1
}

release_udc() {
    [ -d "$GADGET_DIR" ] || return 0
    echo "" >"$GADGET_DIR/UDC" 2>/dev/null
    rm -f "$GADGET_DIR/configs/c.1/acm.usb0"
    rmdir "$GADGET_DIR/configs/c.1" "$GADGET_DIR/functions/acm.usb0" "$GADGET_DIR" 2>/dev/null
}

cleanup_gadget() {
    stop_virtual
    stop_pads
    release_udc
}
trap 'cleanup_gadget; cleanup' EXIT

dmesg_mark=$(dmesg | wc -l)
mount | grep -q debugfs || mount -t debugfs none /sys/kernel/debug
mount | grep -q configfs || mount -t configfs none /sys/kernel/config
modprobe uhid || { echo "uhid unavailable"; exit 1; }
modprobe hid_playstation || { echo "hid_playstation unavailable"; exit 1; }
if ! modprobe dummy_hcd || ! modprobe libcomposite; then
    echo "# dummy_hcd or libcomposite unavailable: gadget checks skipped"
    exit 0
fi
[ -f "$GADGET_KO" ] || { echo "no $GADGET_KO (kernel without gadget support?)"; exit 1; }
unload_modules
insmod "$KO" || { echo "insmod failed"; exit 1; }
insmod "$GADGET_KO" || { echo "insmod ds4t-gadget failed"; exit 1; }

# 1 is a DualShock 4, 2 a DualSense (DS4T_VIRTUAL_*).
for type in 1 2; do
    name=$([ "$type" = 1 ] && echo DS4 || echo DualSense)
    pid=$([ "$type" = 1 ] && echo 05c4 || echo 0ce6)
    maker=$([ "$type" = 1 ] && echo "Sony Computer Entertainment" || echo "Sony Interactive Entertainment")
    echo "# $name gadget follows a Bluetooth pad"
    start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
    start_virtual --usb --type "$type" --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --rumble --hold 4
    check "virtual controller created" wait_virtual '^created following=0005'
    check "its gamepad shows up on evdev" wait_virtual '^evdev found'
    check "the host sees a USB device $pid" usb_attr_is idProduct "$pid"
    check "with a real pad's version" usb_attr_is bcdDevice 0100
    check "and Sony's strings" usb_attr_is manufacturer "$maker"
    check "the product name it was given" usb_attr_is product "$PRODUCT"
    check "and its MAC as serial" usb_attr_is serial "$MAC_VIRT"
    check "hid-playstation binds it through usbhid" wait_driver "$MAC_VIRT" "$FALLBACK"
    check "translated input reaches the gamepad unchanged" \
        wait_virtual "^state $FIXED_STATE\$"
    check "rumble requested through evdev" wait_virtual '^rumble sent'
    check "rumble reaches the pad as a Bluetooth report with a valid CRC" bash -c \
        "for _ in \$(seq 50); do grep -Eq 'output rnum=11 len=78 crc=ok (motor_right=255 motor_left=0|motor_right=0 motor_left=255)' /tmp/fake-pad.$MAC_PAD.log && exit 0; sleep 0.1; done; exit 1"
    stop_virtual
    check "closing control unplugs the gadget" wait_usb_gone
    check "and the pad goes back to the regular driver" wait_driver "$MAC_PAD" "$FALLBACK"
    stop_pads
done

for layout in usb bt; do
    echo "# DS4 gadget: rumble written to hidraw in the $layout layout"
    start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
    start_virtual --usb --type 1 --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --hidraw-rumble "$layout" --hold 3
    check "the report was written" wait_virtual "^hidraw rumble ($layout) sent"
    check "it reaches the pad" bash -c \
        "for _ in \$(seq 50); do grep -q 'output rnum=11 len=78 crc=ok motor_right=32 motor_left=64' /tmp/fake-pad.$MAC_PAD.log && exit 0; sleep 0.1; done; exit 1"
    stop_virtual
    stop_pads
done

echo "# DS4 gadget: feature reports read from a USB pad, through ep0"
mac_le=$(echo "$MAC_VIRT" | tr -d : | sed 's/../& /g' | awk '{for (i = NF; i > 0; i--) printf "%s", $i}')
start_pad usb "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
start_virtual --usb --type 1 --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --features 12,81,a9,a3,02 --hold 1
check "the pairing report carries the emulated MAC" wait_virtual "^feature 12 len=16 data=12$mac_le"
check "a forwarded one is the pad's" wait_virtual '^feature a9 len=9 data=a9a5a5a5a5a5a5a5a5$'
check "the forwarded MAC one carries the emulated MAC" wait_virtual "^feature 81 len=7 data=81$mac_le\$"
check "firmware info" wait_virtual '^feature a3 len=49 '
check "calibration" wait_virtual '^feature 02 len=37 '
stop_virtual
stop_pads

SDL_RUMBLE="$(dirname "$VCHECK")/sdl-rumble"
if [ -x "$SDL_RUMBLE" ]; then
    for mode in hidapi evdev; do
        echo "# DS4 gadget through SDL's $mode driver"
        start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
        start_virtual --usb --type 1 --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --hold 12
        wait_virtual '^evdev found'
        before=$(grep -c '^output' "/tmp/fake-pad.$MAC_PAD.log")
        "$SDL_RUMBLE" "$mode" 054c >/tmp/sdl.log 2>&1
        check "SDL finds and opens it as a Sony pad" grep -q 'vendor=054c product=05c4' /tmp/sdl.log
        check "SDL accepts the rumble" grep -q '^sdl rumble: accepted' /tmp/sdl.log
        check "it reaches the pad" bash -c \
            "[ \$(grep -c '^output' /tmp/fake-pad.$MAC_PAD.log) -gt $before ]"
        stop_virtual
        stop_pads
    done
fi

echo "# the followed pad unplugged mid-stream"
start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
start_virtual --usb --type 1 --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --hold 4
check "follows the pad" wait_virtual "^state $FIXED_STATE\$"
stop_pads
check "everything is released when the pad goes away" wait_virtual "^final $NEUTRAL_STATE\$"
stop_virtual
check "the gadget is unplugged after" wait_usb_gone

echo "# the owner killed while games read and rumble"
start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
start_virtual --usb --type 1 --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --hold 30
wait_virtual '^evdev found'
raw=
for node in "$(hid_dev_by_mac "$MAC_VIRT")"/hidraw/hidraw*; do
    raw=$(basename "$node")
done
readers=()
for _ in 1 2 3 4; do
    cat "/dev/$raw" >/dev/null 2>&1 &
    readers+=($!)
done
# A USB rumble report (motors 0x40/0x20), written in one go each time.
{ printf '\x05\x01\x00\x00\x40\x20'; head -c 26 /dev/zero; } >/tmp/rumble.bin
( for _ in $(seq 200); do
      cat /tmp/rumble.bin >"/dev/$raw" 2>/dev/null || exit 0
  done ) &
writer=$!
sleep 1
kill -KILL "$attach_pid"
wait "$attach_pid" 2>/dev/null
attach_pid=
check "a killed owner unplugs the gadget" wait_usb_gone
kill "${readers[@]}" "$writer" 2>/dev/null
wait "${readers[@]}" "$writer" 2>/dev/null
check "and returns the pad" wait_driver "$MAC_PAD" "$FALLBACK"
stop_pads

echo "# the gadget module is pinned while in use"
start_virtual --usb --type 1 --mac "$MAC_VIRT" --inject --hold 5
check "created without a pad" wait_virtual '^created following=-'
check "rmmod ds4t_gadget is refused while live" bash -c '! rmmod ds4t_gadget 2>/dev/null'
stop_virtual
check "and works after" rmmod ds4t_gadget
# The kernel would load it on demand, and the guest shares the host's modules,
# which may hold an installed copy: point the module loader nowhere meanwhile.
modprobe_path=$(cat /proc/sys/kernel/modprobe)
echo /bin/false >/proc/sys/kernel/modprobe
check "without it, a USB virtual controller is refused" \
    bash -c "'$VCHECK' --usb --type 1 --mac '$MAC_VIRT' --hold 0 | grep -q '^create failed: Operation not supported'"
echo "$modprobe_path" >/proc/sys/kernel/modprobe
insmod "$GADGET_KO" || nok "insmod ds4t-gadget failed"

echo "# dummy_hcd removed under a live gadget"
start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
start_virtual --usb --type 1 --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --hold 6
wait_virtual '^evdev found'
check "rmmod dummy_hcd under it" rmmod dummy_hcd
check "the host side goes away" wait_usb_gone
stop_virtual
stop_pads
modprobe dummy_hcd || nok "dummy_hcd did not come back"

echo "# no free UDC"
if occupy_udc; then
    check "a USB virtual controller is refused while the UDC is taken" \
        bash -c "'$VCHECK' --usb --type 1 --mac '$MAC_VIRT' --hold 0 | grep -q '^create failed: Device or resource busy'"
    release_udc
else
    echo "# skipped: usb_f_acm unavailable"
fi

# Every other one is killed a moment after creation, mostly while the host is
# still enumerating it; the rest run until their gamepad shows up.
echo "# create and destroy, 100 times, half of them mid-enumeration"
start_pad bt "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"
failed=0
for i in $(seq 100); do
    if [ $((i % 2)) = 0 ]; then
        # Braces: bash's own "Killed" notice goes to their stderr, not the log.
        { timeout -s KILL "0.$((RANDOM % 5 + 1))" "$VCHECK" --usb --type 1 --mac "$MAC_VIRT" \
            --pad-mac "$MAC_PAD" --hold 0 >/dev/null 2>&1; } 2>/dev/null
        continue
    fi
    "$VCHECK" --usb --type 2 --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --hold 0 >/tmp/loop.log 2>&1
    grep -q '^evdev found' /tmp/loop.log || failed=$((failed + 1))
done
check "every full run got its gamepad ($failed did not)" test "$failed" -eq 0
check "and none is left behind" wait_usb_gone
stop_pads

check "rmmod of both modules works at the end" bash -c 'rmmod ds4t_gadget && rmmod hid_ds4_translator'

echo "# no kernel splats across the whole run"
check_no_splat "$dmesg_mark"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
