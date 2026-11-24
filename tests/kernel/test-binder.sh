#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# M1 exit criteria for hid-ds4-translator, run as root against a uhid fake pad
# (see fake-pad.c). Meant to run inside a throwaway VM (run-vm.sh): it loads
# and unloads kernel modules.
#
#   test-binder.sh <hid-ds4-translator.ko> <fake-pad> <pad-client>

set -u
KO=${1:?module path}
FAKE=${2:?fake-pad path}
CLIENT=${3:?pad-client path}
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

echo "# 6. char device (module still loaded with ignore_mac=$MAC_A)"
stop_pads
start_pad usb "$MAC_B" --stream 1
wait_driver "$MAC_B" "$DRIVER"
hid_id=$(basename "$(hid_dev_by_mac "$MAC_B")")
node=/dev/ds4-translator/$hid_id
check "node exists" bash -c "for _ in \$(seq 50); do [ -c '$node' ] && exit 0; sleep 0.1; done; exit 1"
check "node is 0600 root" bash -c "[ \"\$(stat -c '%a %U' '$node')\" = '600 root' ]"
info=$("$CLIENT" "$node" info)
check "GET_INFO reports abi, ids and MAC" \
    bash -c "grep -qi 'abi=1 bus=3 vendor=054c product=05c4 uniq=$MAC_B' <<<'$info' && grep -q 'hid_id=$hid_id' <<<'$info'"
reports=$("$CLIENT" "$node" read 5)
check "read returns 5 USB input reports" \
    bash -c "[ \$(grep -c '^report id=01 len=64 ' <<<'$reports') = 5 ]"
check "second open is refused with EBUSY" bash -c "
    '$CLIENT' '$node' hold >/dev/null & holder=\$!
    sleep 0.5
    '$CLIENT' '$node' info 2>&1 | grep -q 'Device or resource busy'; rc=\$?
    kill \$holder; exit \$rc"
"$CLIENT" "$node" write "05ff000000000000000000ff0000" >/dev/null
check "output report reaches the pad" \
    bash -c "sleep 0.5; grep -q '^output rnum=05' /tmp/fake-pad.$MAC_B.log"

# The daemon is blocked in poll()/read() when the pad disappears.
"$CLIENT" "$node" hold >/tmp/pad-client.hold.log 2>&1 &
holder=$!
sleep 0.5
stop_pads
check "blocked reader gets ENODEV/HUP when the pad is removed" \
    bash -c "for _ in \$(seq 30); do kill -0 $holder 2>/dev/null || exit 0; sleep 0.1; done; exit 1"
check "reader saw the disconnect" grep -Eq 'read error: No such device|poll hangup' /tmp/pad-client.hold.log
check "node removed with the pad" bash -c "[ ! -e '$node' ]"

echo "# 7. no kernel splats across the whole run"
check "no BUG/KASAN/lockdep in dmesg" \
    bash -c "! dmesg | grep -Eq 'BUG:|KASAN|WARNING: .*lock|possible circular'"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
