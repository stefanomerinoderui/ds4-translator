#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Exit criteria for hid-ds4-translator (binding, takeover, attach/detach, the
# per-pad char device and the daemon's transport), run as root against uhid fake
# pads (see fake-pad.c). Meant to run inside a throwaway VM (run-vm.sh): it
# loads and unloads kernel modules.
#
#   test-binder.sh <hid-ds4-translator.ko> <fake-pad> <pad-client> <transport-check> <ds4-translator> <ds4-ctl>

set -u
KO=${1:?module path}
FAKE=${2:?fake-pad path}
CLIENT=${3:?pad-client path}
TRANSPORT=${4:?transport-check path}
DAEMON=${5:?ds4-translator path}
DS4CTL=${6:?ds4-ctl path}
EMULATED_MAC=74:e7:d6:3a:47:e8	# the daemon's default emulated MAC
DRIVER=hid-ds4-translator
FALLBACK=playstation   # driver name; the module is hid_playstation
MAC_A=AA:BB:CC:00:00:01
MAC_B=AA:BB:CC:00:00:02
DEBUGFS=/sys/kernel/debug/ds4t

pass=0
fail=0
pad_pids=()
attach_pid=
CONTROL=/dev/ds4-translator/control

ok()   { echo "ok   - $1"; pass=$((pass + 1)); }
nok()  { echo "FAIL - $1"; fail=$((fail + 1)); }
check() { local desc=$1; shift; if "$@"; then ok "$desc"; else nok "$desc"; fi; }

cleanup() {
    kill "${pad_pids[@]}" "$attach_pid" 2>/dev/null
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

# Stands in for the daemon: opens the control node (optionally naming the
# emulated pad's MAC) and stays attached until detach/kill.
attach() { # [ignore mac]
    rm -f /tmp/attach.log
    "$CLIENT" "$CONTROL" control "$@" >/tmp/attach.log 2>&1 &
    attach_pid=$!
    local _
    for _ in $(seq 50); do
        grep -q '^attached' /tmp/attach.log && return 0
        sleep 0.1
    done
    return 1
}

detach() { # [signal]
    kill "-${1:-TERM}" "$attach_pid" 2>/dev/null
    wait "$attach_pid" 2>/dev/null
    attach_pid=
}

has_node() { # <hid device path> <name under it>
    [ -e "$1/$2" ]
}

mount | grep -q debugfs || mount -t debugfs none /sys/kernel/debug
modprobe uhid || { echo "uhid unavailable"; exit 1; }
modprobe hid_playstation || { echo "hid_playstation unavailable"; exit 1; }

echo "# 0. module loaded, nobody attached: pads stay with the regular driver"
insmod "$KO" || { echo "insmod failed"; exit 1; }
check "control node is 0600 root" \
    bash -c "[ \"\$(stat -c '%a %U' $CONTROL)\" = '600 root' ]"
start_pad usb "$MAC_A" --stream 1
check "pad stays on $FALLBACK" wait_driver "$MAC_A" "$FALLBACK"

echo "# 1. attach: the pad is taken over and its nodes disappear"
attach
check "control open succeeded with ABI 2" grep -q 'attached abi=2' /tmp/attach.log
check "pad taken over by $DRIVER" wait_driver "$MAC_A" "$DRIVER"
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

echo "# 2. a second attach is refused, and the module cannot be unloaded while attached"
check "second control open is refused with EBUSY" \
    bash -c "'$CLIENT' $CONTROL control 2>&1 | grep -q 'Device or resource busy'"
check "rmmod is refused while attached" bash -c "! rmmod hid_ds4_translator 2>/dev/null"

echo "# 3. detach hands the pad back"
detach
check "pad rebound to $FALLBACK" wait_driver "$MAC_A" "$FALLBACK"
check "playstation registered its input again" \
    bash -c "for _ in \$(seq 30); do ls '$dev'/input/* >/dev/null 2>&1 && exit 0; sleep 0.1; done; exit 1"

echo "# 4. a killed daemon releases the pad too"
attach
check "pad taken over again" wait_driver "$MAC_A" "$DRIVER"
detach KILL
check "pad rebound to $FALLBACK after SIGKILL" wait_driver "$MAC_A" "$FALLBACK"
stop_pads
check "rmmod works once detached" rmmod hid_ds4_translator
insmod "$KO"

echo "# 5. pad plugged in while attached, with $FALLBACK loaded first"
attach
start_pad bt "$MAC_B"
check "Bluetooth pad ends up on $DRIVER" wait_driver "$MAC_B" "$DRIVER"
stop_pads
detach

echo "# 6. the emulated MAC is never claimed (ioctl)"
attach "$MAC_A"
start_pad usb "$MAC_A"
check "ignored MAC stays on $FALLBACK" wait_driver "$MAC_A" "$FALLBACK"
start_pad usb "$MAC_B"
check "other MAC still claimed" wait_driver "$MAC_B" "$DRIVER"
stop_pads
detach
start_pad usb "$MAC_A"
attach
check "the ignore MAC is forgotten on detach" wait_driver "$MAC_A" "$DRIVER"
stop_pads
detach

echo "# 6b. the emulated MAC is never claimed (module parameter)"
rmmod hid_ds4_translator
insmod "$KO" ignore_mac="$MAC_A"
attach
start_pad usb "$MAC_A"
check "parameter MAC stays on $FALLBACK" wait_driver "$MAC_A" "$FALLBACK"
stop_pads
detach
rmmod hid_ds4_translator
insmod "$KO"

echo "# 7. char device"
attach
start_pad usb "$MAC_B" --stream 1
wait_driver "$MAC_B" "$DRIVER"
hid_id=$(basename "$(hid_dev_by_mac "$MAC_B")")
node=/dev/ds4-translator/$hid_id
check "node exists" bash -c "for _ in \$(seq 50); do [ -c '$node' ] && exit 0; sleep 0.1; done; exit 1"
check "node is 0600 root" bash -c "[ \"\$(stat -c '%a %U' '$node')\" = '600 root' ]"
info=$("$CLIENT" "$node" info)
check "GET_INFO reports abi, ids and MAC" \
    bash -c "grep -qi 'abi=2 bus=3 vendor=054c product=05c4 uniq=$MAC_B' <<<'$info' && grep -q 'hid_id=$hid_id' <<<'$info'"
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

echo "# 8. the daemon's transport (src/driver-transport.cpp) against the module"
detach
start_pad usb "$MAC_B" --stream 1
start_pad usb "$MAC_A"	# stands in for the daemon's own emulated pad: same IDs
wait_driver "$MAC_B" "$FALLBACK"
wait_driver "$MAC_A" "$FALLBACK"
hid_id=$(basename "$(hid_dev_by_mac "$MAC_B")")
"$TRANSPORT" "$hid_id" "$MAC_A" >/tmp/transport-check.log 2>&1 &
checker=$!
check "daemon transport opens, reads and writes" \
    bash -c "for _ in \$(seq 100); do grep -q 'waiting for disconnect' /tmp/transport-check.log && exit 0; sleep 0.1; done; exit 1"
check "the emulated pad was not claimed by the attach" wait_driver "$MAC_A" "$FALLBACK"
check "output report from the daemon reaches the pad" \
    bash -c "sleep 0.3; grep -q '^output rnum=05 len=32' /tmp/fake-pad.$MAC_B.log"
stop_pads
check "daemon transport sees the disconnect" \
    bash -c "for _ in \$(seq 50); do grep -q 'disconnect detected' /tmp/transport-check.log && exit 0; sleep 0.1; done; exit 1"
check "transport-check exited after the disconnect" bash -c "! kill -0 $checker 2>/dev/null"

echo "# 9. the real daemon with hide_method=driver"
detach
export DS4_RUNTIME_DIR=/tmp/ds4d DS4_STATE_DIR=/tmp/ds4d/state DS4_CONFIG_PATH=/tmp/ds4d/ds4-translator.conf
mkdir -p "$DS4_RUNTIME_DIR"
printf 'hide_method=driver\nbackend_ds4=uhid\n' >"$DS4_CONFIG_PATH"
# Bluetooth: the daemon skips USB-bus uhid devices as its own emulated pad. The
# fake's CRC is invalid, so hid-playstation never binds it; "not ours" is the test.
start_pad bt "$MAC_A" --stream 1
check "pad is not ours before the daemon starts" wait_not_driver "$MAC_A" "$DRIVER"
"$DAEMON" -t ds4 >/tmp/daemon.log 2>&1 &
daemon_pid=$!
check "daemon attached to the module" \
    bash -c "for _ in \$(seq 100); do grep -q 'driver: attached to hid_ds4_translator' /tmp/daemon.log && exit 0; sleep 0.1; done; exit 1"
check "pad taken over by $DRIVER" wait_driver "$MAC_A" "$DRIVER"
check "daemon read reports from the pad through the node" \
    bash -c "for _ in \$(seq 100); do grep -q 'First report from physical controller' /tmp/daemon.log && exit 0; sleep 0.1; done; exit 1"
check "daemon never fell back to another method" bash -c "! grep -q 'falling back\|using the unbind method' /tmp/daemon.log"
check "the emulated pad exists and was not claimed" wait_driver "$EMULATED_MAC" "$FALLBACK"

"$DS4CTL" release-physical >/tmp/ctl.log 2>&1
check "release-physical hands the pad back" wait_not_driver "$MAC_A" "$DRIVER"
check "the daemon detached" grep -q 'driver: detached' /tmp/daemon.log
"$DS4CTL" resume-physical >/tmp/ctl.log 2>&1
check "resume-physical claims it again" wait_driver "$MAC_A" "$DRIVER"

kill -KILL "$daemon_pid"
wait "$daemon_pid" 2>/dev/null
check "a killed daemon releases the pad" wait_not_driver "$MAC_A" "$DRIVER"
stop_pads
unset DS4_RUNTIME_DIR DS4_STATE_DIR DS4_CONFIG_PATH

echo "# 10. no kernel splats across the whole run"
check "no BUG/KASAN/lockdep in dmesg" \
    bash -c "! dmesg | grep -Eq 'BUG:|KASAN|WARNING: .*lock|possible circular'"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
