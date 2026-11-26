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
MAC_A=AA:BB:CC:00:00:01
MAC_B=AA:BB:CC:00:00:02
DEBUGFS=/sys/kernel/debug/ds4t

# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

# The ABI the module and the tools here were built with.
ABI=$(awk '/^#define DS4T_ABI_VERSION/ { print $3 }' "$(dirname "$0")/../../include/uapi/ds4t.h")

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

# Only what the kernel logs from here on counts as ours.
dmesg_mark=$(dmesg | wc -l)

mount | grep -q debugfs || mount -t debugfs none /sys/kernel/debug
modprobe uhid || { echo "uhid unavailable"; exit 1; }
modprobe hid_playstation || { echo "hid_playstation unavailable"; exit 1; }

# A copy installed on the host (DKMS) must not stand in for the build under test.
unload_modules

echo "# 0a. oracle: hid-playstation accepts the fake Bluetooth pad's calibration"
# A valid CRC proves the seed (0xA3) used by src/translate.cpp; the payload's
# gyro words only validate in the Bluetooth order, which proves that layout.
start_pad bt "$MAC_B"
check "Bluetooth pad binds to $FALLBACK" wait_driver "$MAC_B" "$FALLBACK"
bt_name=$(basename "$(hid_dev_by_mac "$MAC_B")")
check "no calibration rejected for it" bash -c "! dmesg | grep -q 'playstation $bt_name: Invalid'"
stop_pads

echo "# 0. module loaded, nobody attached: pads stay with the regular driver"
insmod "$KO" || { echo "insmod failed"; exit 1; }
check "control node is 0600 root" \
    bash -c "[ \"\$(stat -c '%a %U' $CONTROL)\" = '600 root' ]"
start_pad usb "$MAC_A" --stream 1
check "pad stays on $FALLBACK" wait_driver "$MAC_A" "$FALLBACK"

echo "# 1. attach: the pad is taken over and its nodes disappear"
attach
check "control open succeeded with the current ABI" grep -q "attached abi=$ABI" /tmp/attach.log
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
    bash -c "grep -qi 'abi=$ABI bus=3 vendor=054c product=05c4 uniq=$MAC_B' <<<'$info' && grep -q 'hid_id=$hid_id' <<<'$info'"
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

# Feature reports: the fake answers 0x02 with 0x02 followed by 0xA5 bytes.
cal=$("$CLIENT" "$node" calibration)
check "GET_CALIBRATION returns the report read at probe" \
    bash -c "grep -q '^len=37 02a5a5a5' <<<'$cal'"
feat=$("$CLIENT" "$node" feature-get 02 37)
check "GET_FEATURE returns the pad's report" bash -c "grep -q '^len=37 02a5a5a5' <<<'$feat'"
check "GET_FEATURE of a report the pad refuses fails" \
    bash -c "'$CLIENT' '$node' feature-get 77 8 | grep -q 'Input/output error'"
check "GET_FEATURE rejects a length beyond the buffer" \
    bash -c "'$CLIENT' '$node' feature-get 02 200 | grep -q 'Invalid argument'"
"$CLIENT" "$node" feature-set 04010203 >/dev/null
check "SET_FEATURE reaches the pad" \
    bash -c "sleep 0.5; grep -q '^set_report rnum=04' /tmp/fake-pad.$MAC_B.log"

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
# The fake BT pad's calibration is the oracle payload of fake-pad.c; the
# emulated DS4 must serve it in the USB layout, gyro words reordered.
emulated=$(hid_dev_by_mac "$EMULATED_MAC")
hidraw=$(basename "$(find "$emulated/hidraw" -mindepth 1 -maxdepth 1 | head -1)")
want=$(python3 -I -c '
import struct
bt = [0, 0, 0, 100, 200, 300, 100, 200, 300, 540, 540, 8192, -8192, 8192, -8192, 8192, -8192, 0]
# the fake sends exactly these words little-endian; reorder the gyro block
w = bt[3:9]
usb = bt[:3] + [w[0], w[2], w[4], w[1], w[3], w[5]] + bt[9:]
print("len=37 02" + struct.pack("<18h", *usb).hex())')
got=$("$CLIENT" "/dev/$hidraw" hidraw-feature 02 37)
check "emulated DS4 serves the physical pad's calibration" test "$got" = "$want"
check "hid-playstation accepts the calibration served for the emulated pad" \
    bash -c "! dmesg | grep -q 'playstation $(basename "$emulated"): Invalid'"

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

echo "# 10. hardening: churn, unplugging under load, unloading"
start_pad usb "$MAC_A" --stream 1
cycles_ok=1
for _ in $(seq 8); do
    attach
    wait_driver "$MAC_A" "$DRIVER" || cycles_ok=0
    detach
    wait_driver "$MAC_A" "$FALLBACK" || cycles_ok=0
done
check "8 attach/detach cycles each end on $FALLBACK" test $cycles_ok = 1
stop_pads

# A client using every operation of the node while the pad is unplugged.
unplug_ok=1
attach
for _ in $(seq 8); do
    start_pad usb "$MAC_B" --stream 1
    wait_driver "$MAC_B" "$DRIVER" || unplug_ok=0
    node=/dev/ds4-translator/$(basename "$(hid_dev_by_mac "$MAC_B")")
    "$CLIENT" "$node" stress >/tmp/stress.log 2>&1 &
    stressor=$!
    sleep 0.4
    stop_pads
    for _ in $(seq 50); do kill -0 "$stressor" 2>/dev/null || break; sleep 0.1; done
    if kill -0 "$stressor" 2>/dev/null; then unplug_ok=0; kill -KILL "$stressor"; fi
    wait "$stressor" 2>/dev/null
    grep -q 'stress: .* ops, ended:' /tmp/stress.log || unplug_ok=0
done
check "8 unplugs under load: the client always ended" test $unplug_ok = 1
check "no pad node left behind" \
    bash -c "[ -z \"\$(ls /dev/ds4-translator | grep -v '^control\$')\" ]"

# Unplugging while the kernel waits on the pad: the fake sits on its answer to
# a feature request for four seconds and is killed meanwhile (its /dev/uhid
# closing is what destroys the device, as a USB unplug would).
start_pad usb "$MAC_B" --stream 1 --slow-feature 4000
wait_driver "$MAC_B" "$DRIVER"
node=/dev/ds4-translator/$(basename "$(hid_dev_by_mac "$MAC_B")")
"$CLIENT" "$node" feature-get 02 37 >/tmp/slow.log 2>&1 &
blocked=$!
sleep 0.7
kill -KILL "${pad_pids[@]}"
wait "${pad_pids[@]}" 2>/dev/null
pad_pids=()
check "a feature request blocked on the pad returns when it is unplugged" \
    bash -c "for _ in \$(seq 25); do kill -0 $blocked 2>/dev/null || exit 0; sleep 0.1; done; exit 1"
check "it reports an error and not data" grep -Eq 'Input/output error|No such device' /tmp/slow.log
check "the node is gone" bash -c "for _ in \$(seq 30); do [ ! -e '$node' ] && exit 0; sleep 0.1; done; exit 1"
detach

start_pad usb "$MAC_A" --stream 1
wait_driver "$MAC_A" "$FALLBACK"
reload_ok=1
for _ in $(seq 5); do
    rmmod hid_ds4_translator || reload_ok=0
    insmod "$KO" || reload_ok=0
done
check "5 unload/load cycles with a pad present" test $reload_ok = 1
check "the pad is untouched by them" wait_driver "$MAC_A" "$FALLBACK"
attach
check "and the reloaded module still takes it over" wait_driver "$MAC_A" "$DRIVER"
detach
check "and gives it back" wait_driver "$MAC_A" "$FALLBACK"
stop_pads

echo "# 11. no kernel splats across the whole run"
check "no oops, KASAN/KFENCE report, slab corruption or lockdep splat in dmesg" \
    bash -c "! dmesg | tail -n +$((dmesg_mark + 1)) | grep -Eq 'BUG:|Oops|KASAN|KFENCE|Poison overwritten|Redzone|Object already free|WARNING: CPU|WARNING: .*lock|possible circular'"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
