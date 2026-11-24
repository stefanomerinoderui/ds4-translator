# hid-ds4-translator (kernel module)

HID driver that claims a physical DualShock 4 before `hid-playstation`, so
no other driver creates a hidraw, input, battery or LED device for it. See
[docs/kernel-driver.md](../docs/kernel-driver.md) for the design.

**Status: M1 prototype.** The module binds the pad and dumps incoming reports
to debugfs. The char device the daemon will read from is not implemented yet.

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
sudo insmod kernel/hid-ds4-translator.ko
sudo cat /sys/kernel/debug/ds4t/*      # last 16 reports of each claimed pad
sudo rmmod hid_ds4_translator          # pads go back to hid-playstation
```

`ignore_mac=AA:BB:CC:DD:EE:FF` (module parameter, also writable at runtime
under `/sys/module/hid_ds4_translator/parameters/`) names the emulated pad,
which is never claimed. Pads already bound to another driver, or that
connect later and lose the load-order race, are taken over.

## Test without hardware

`make kernel-test` boots a virtme-ng VM and runs
`tests/kernel/test-binder.sh` against `tests/kernel/fake-pad.c`, a DS4 built
on `/dev/uhid`. Install virtme-ng with `uv tool install virtme-ng`.
