#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Allocation failures in the virtual controller's creation, one at a time, on a
# debug kernel (tests/kernel/debug.config: failslab, kmemleak, KASAN, lockdep).
# Each run of the create ioctl gets its Nth allocation failed, N = 1, 2, ...
# until one runs through untouched: every failure must unwind cleanly (an
# error, no oops, nothing leaked), and the controller must still come up
# after. Skipped on a kernel without failslab, such as the host's.
#
#   test-faults.sh <hid-ds4-translator.ko> <fake-pad> <virtual-check> <ds4-translator> <ds4-ctl>

set -u
KO=${1:?module path}
FAKE=${2:?fake-pad path}
VCHECK=${3:?virtual-check path}
GADGET_KO="$(dirname "$KO")/ds4t-gadget.ko"
MAC_PAD=AA:BB:CC:00:00:41
MAC_VIRT=74:e7:d6:3a:47:e8
FAILSLAB=/sys/kernel/debug/failslab
KMEMLEAK=/sys/kernel/debug/kmemleak

# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

mount | grep -q debugfs || mount -t debugfs none /sys/kernel/debug
if [ ! -d "$FAILSLAB" ]; then
    echo "# no failslab (not a debug kernel): fault injection skipped"
    exit 0
fi

trap 'stop_pads; cleanup' EXIT

dmesg_mark=$(dmesg | wc -l)
modprobe uhid || { echo "uhid unavailable"; exit 1; }
modprobe hid_playstation || { echo "hid_playstation unavailable"; exit 1; }
backends=(hid)
if modprobe dummy_hcd 2>/dev/null && modprobe libcomposite 2>/dev/null && [ -f "$GADGET_KO" ]; then
    backends+=(usb)
fi
unload_modules
insmod "$KO" || { echo "insmod failed"; exit 1; }
[ "${backends[*]}" = "hid usb" ] && { insmod "$GADGET_KO" || { echo "insmod ds4t-gadget failed"; exit 1; }; }

# Sleeping allocations too (all of create's are GFP_KERNEL), every time asked,
# no log flood.
echo N >"$FAILSLAB/ignore-gfp-wait"
echo 0 >"$FAILSLAB/verbose"

# Spared: allocations under dev_set_name(). hid_add_device() ignores its
# failure and hands debugfs a NULL name (kernel 6.18, any HID device creator:
# uhid, usbhid); that oops is upstream's, and would only hide this module's own
# error paths behind it.
if [ -e "$FAILSLAB/reject-start" ]; then
    read -r start end < <(sort /proc/kallsyms | awk '
        found { print start, "0x" $1; exit }
        $3 == "dev_set_name" { start = "0x" $1; found = 1 }')
    if [ -n "${start:-}" ] && [ -n "${end:-}" ]; then
        echo "$start" >"$FAILSLAB/reject-start"
        echo "$end" >"$FAILSLAB/reject-end"
    else
        nok "dev_set_name not found in /proc/kallsyms"
    fi
else
    echo "# no failslab stack filter: hid_add_device()'s unchecked dev_set_name() may oops"
fi

start_pad usb "$MAC_PAD" --fixed 1 --stream 1 || nok "fake pad did not become ready"

for backend in "${backends[@]}"; do
    echo "# every allocation of the create ioctl failing in turn ($backend frontend)"
    usb=()
    [ "$backend" = usb ] && usb=(--usb)
    injected=0 bad=0 done_at=
    for n in $(seq "${FAULT_STEPS:-300}"); do
        "$VCHECK" "${usb[@]}" --type 1 --mac "$MAC_VIRT" --pad-mac "$MAC_PAD" --fail-nth "$n" --hold 0 \
            >/tmp/fault.log 2>&1
        left=$(sed -n 's/^fail-nth left=//p' /tmp/fault.log)
        echo "#   fail-nth $n: left=${left:-?} $(grep -m1 -e '^create failed' -e '^evdev found' -e '^no event node' /tmp/fault.log)"
        if [ "$left" = 0 ]; then
            injected=$((injected + 1))
            # An injected failure fails the ioctl, or is absorbed (a
            # best-effort allocation), or fails hid-playstation's probe of
            # the device created (no gamepad); the tool always exits, and
            # anything else is a crash.
            grep -q -e '^create failed' -e '^evdev found' -e '^no event node' /tmp/fault.log ||
                bad=$((bad + 1))
        else
            done_at=$n
            break
        fi
    done
    # Creating a HID device allocates more than the steps allowed under KASAN
    # in reasonable time (FAULT_STEPS, 300 by default): the clean run, once
    # reached, must bring the controller up; not reaching it is no failure.
    if [ -n "$done_at" ]; then
        check "the clean run ($injected failures injected before it) brings the controller up" \
            grep -q '^evdev found' /tmp/fault.log
    else
        echo "# $injected failures injected, no clean run within ${FAULT_STEPS:-300} steps"
    fi
    check "each injected failure ended cleanly ($bad did not)" test "$bad" -eq 0
done
stop_pads

if [ -e "$KMEMLEAK" ]; then
    echo "# nothing leaked"
    echo scan >"$KMEMLEAK"
    sleep 5
    echo scan >"$KMEMLEAK"
    check "kmemleak finds no object of the module's" bash -c "! grep -q -e ds4t -e hid_ds4_translator $KMEMLEAK"
fi

check "rmmod works at the end" bash -c 'rmmod ds4t_gadget 2>/dev/null; rmmod hid_ds4_translator'

echo "# no kernel splats across the whole run"
check_no_splat "$dmesg_mark"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
