#!/bin/sh
# Disconnect the remembered controller over Bluetooth, if it is connected.
# Used by tools/hwtest scenarios to turn the pad off without touching it.
# Needs no root: bluetoothctl talks to bluetoothd on the system bus.
set -eu

mac=$(ds4-ctl status | sed -n 's/^Remembered Controller MAC: //p' | head -n1)
if [ -z "$mac" ]; then
    echo "bt-disconnect: the daemon reports no remembered controller MAC" >&2
    exit 1
fi
if ! bluetoothctl devices Connected | grep -qi "$mac"; then
    echo "bt-disconnect: $mac is not connected over Bluetooth"
    exit 0
fi
bluetoothctl disconnect "$mac"
