# hid-ds4-translator (kernel module)

HID driver that claims a physical DualShock 4 before `hid-playstation`, so
no other driver creates a hidraw, input, battery or LED device for it. See
[docs/kernel-driver.md](../docs/kernel-driver.md) for the design.

**Status: prototype (M3).** The module is inert until a daemon attaches
through `/dev/ds4-translator/control`; closing that node, however it happens,
hands every pad back to `hid-playstation`. While attached it claims the pad,
dumps incoming reports to debugfs, and exposes each pad as
`/dev/ds4-translator/<hid_id>` for the daemon's `driver` hide method
(`ds4-ctl set-hide-method driver`). Not yet: feature-report ioctls and
calibration passthrough.

## Build

Needs the running kernel's headers. A Clang-built kernel (CachyOS) is
detected from its `.config` and built with `LLVM=1` automatically.

```
make kernel            # from the repository root, or `make` in this directory
make kernel-check      # checkpatch.pl --strict
```

## Try it

Loading a module needs root, so these are shown, not run for you:

```
sudo insmod kernel/hid-ds4-translator.ko   # inert: pads still use hid-playstation
sudo ds4-translator &                      # with hide_method=driver: attaches
sudo cat /sys/kernel/debug/ds4t/*          # last 32 reports of each claimed pad
ls -l /dev/ds4-translator/                 # control + one 0600 node per claimed pad
sudo rmmod hid_ds4_translator              # only works once the daemon has detached
```

While attached, pads already bound to another driver, or that connect later
and lose the load-order race, are taken over. The daemon names its own
emulated pad through the control node so it is never claimed;
`ignore_mac=AA:BB:CC:DD:EE:FF` (module parameter, writable at runtime under
`/sys/module/hid_ds4_translator/parameters/`) does the same by hand.

## Test without hardware

`make kernel-test` boots a virtme-ng VM and runs
`tests/kernel/test-binder.sh` against `tests/kernel/fake-pad.c`, a DS4 built
on `/dev/uhid`, with `pad-client.c` and `transport-check.cpp` (the daemon's
own `src/driver-transport.cpp`) as the node's users. Install virtme-ng with `uv tool install virtme-ng`.
