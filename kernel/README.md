# hid-ds4-translator (kernel module)

HID driver that claims a physical DualShock 4 before `hid-playstation`, so
no other driver creates a hidraw, input, battery or LED device for it. See
[docs/kernel-driver.md](../docs/kernel-driver.md) for the design.

**Status: prototype (M4).** The module is inert until a daemon attaches
through `/dev/ds4-translator/control`; closing that node, however it happens,
hands every pad back to `hid-playstation`. While attached it claims the pad,
dumps incoming reports to debugfs, and exposes each pad as
`/dev/ds4-translator/<hid_id>` for the daemon's `driver` hide method
(`ds4-ctl set-hide-method driver`), with raw feature-report ioctls and the
calibration cached at probe (the daemon serves it to the emulated DS4). Not
yet: DKMS packaging and CI.

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

## Install with DKMS

The Arch package `ds4-translator-dkms` (`make arch-pkg` builds it) installs the
sources to `/usr/src/ds4-translator-<version>/`, and DKMS builds them for every
installed kernel that has headers, again on each kernel upgrade. Install the
headers that match your kernel (`linux-headers`, `linux-lts-headers`,
`linux-cachyos-headers`, ...). `make stage-dkms DESTDIR=...` produces the same
tree on other distributions; copy it to `/usr/src/` and run
`dkms add`/`dkms install` yourself.

The daemon's service loads the module (`modprobe hid_ds4_translator`) and
quietly carries on without it if it is missing, so no module configuration is
needed. The module is inert until the daemon attaches, which only happens with
`ds4-ctl set-hide-method driver`.

A kernel built with Clang (CachyOS, for one) needs a module built with Clang
too; `kernel/Makefile` detects `CONFIG_CC_IS_CLANG` in the kernel's `.config`
and passes `LLVM=1` itself, so DKMS needs no extra configuration. The
supported kernels start at 6.12 LTS.

### Secure Boot

With Secure Boot on, the kernel refuses unsigned modules. DKMS signs the
module it builds with a Machine Owner Key when one is configured (DKMS 3
creates `/var/lib/dkms/mok.key` and `mok.pub` the first time it needs them;
`/etc/dkms/framework.conf` can point at your own key). That key must be
enrolled in the firmware once, which needs a reboot and a console prompt:

```
sudo mokutil --import /var/lib/dkms/mok.pub
```

If the module fails to load with "Key was rejected by service", the key is
not enrolled yet. Nothing breaks in the meantime: the daemon falls back to
the `unbind` method.

## Test without hardware

`make kernel-test` boots a virtme-ng VM and runs
`tests/kernel/test-binder.sh` against `tests/kernel/fake-pad.c`, a DS4 built
on `/dev/uhid`, with `pad-client.c` and `transport-check.cpp` (the daemon's
own `src/driver-transport.cpp`) as the node's users. Install virtme-ng with `uv tool install virtme-ng`.
