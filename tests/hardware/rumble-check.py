#!/usr/bin/env python3
"""Buzzes the controller once through each path a game can use, so you can tell
which ones reach it. Run it with the translator running and a controller
connected; it prints a line before each one-second buzz.

  tests/hardware/rumble-check.py

Paths, in order:
  hidraw      the emulated pad's hidraw node, as Steam/SDL/Chrome write to it
  evdev       the emulated pad's event node force feedback, as most native games
  steam       Steam Input's virtual Xbox 360 pad (when Steam is running), which
              Steam turns into a write to the emulated pad's hidraw node
"""
import fcntl
import glob
import os
import struct
import sys
import time

FF_RUMBLE = 0x50
EV_FF = 0x15
EVIOCSFF = 0x40304580  # _IOW('E', 0x80, struct ff_effect) on 64-bit
EVIOCRMFF = 0x40044581  # _IOW('E', 0x81, int)


def sysfs_nodes():
    """(hidraw path, event path) of the emulated pad: the Sony pad on the
    translator's own bus (dummy_hcd, uhid or the kernel module's control node)."""
    hidraw = event = None
    for dev in glob.glob("/sys/bus/hid/devices/0003:054C:*"):
        real = os.path.realpath(dev)
        if not any(p in real for p in ("dummy_hcd", "/virtual/misc/uhid/", "/virtual/misc/ds4t-control/")):
            continue
        for h in glob.glob(dev + "/hidraw/hidraw*"):
            hidraw = "/dev/" + os.path.basename(h)
        for e in glob.glob(dev + "/input/input*/event*"):
            name = open(os.path.dirname(e) + "/name").read().strip()
            if "Motion" not in name and "Touchpad" not in name:
                event = "/dev/input/" + os.path.basename(e)
    return hidraw, event


def steam_pad():
    for e in glob.glob("/sys/class/input/event*"):
        try:
            if open(e + "/device/name").read().strip().startswith("Microsoft X-Box 360 pad"):
                return "/dev/input/" + os.path.basename(e)
        except OSError:
            pass
    return None


def buzz_hidraw(path):
    fd = os.open(path, os.O_WRONLY)
    def report(left, right):
        b = bytearray(32)
        b[0], b[1], b[4], b[5], b[8] = 0x05, 0x07, right, left, 255
        return bytes(b)
    os.write(fd, report(255, 255))
    time.sleep(1)
    os.write(fd, report(0, 0))
    os.close(fd)


def buzz_evdev(path):
    fd = os.open(path, os.O_RDWR)
    # struct ff_effect (48 bytes): type, id, direction, trigger{button,
    # interval}, replay{length, delay}, padding, then the union at offset 16
    # (rumble: strong, weak magnitude).
    buf = bytearray(48)
    struct.pack_into("=HhHHHHH", buf, 0, FF_RUMBLE, -1, 0, 0, 0, 1000, 0)
    struct.pack_into("=HH", buf, 16, 0xFFFF, 0xFFFF)
    fcntl.ioctl(fd, EVIOCSFF, buf)
    effect_id = struct.unpack_from("=h", buf, 2)[0]
    os.write(fd, struct.pack("@llHHi", 0, 0, EV_FF, effect_id, 1))
    time.sleep(1.2)
    fcntl.ioctl(fd, EVIOCRMFF, effect_id)
    os.close(fd)


def main():
    hidraw, event = sysfs_nodes()
    print(f"emulated pad: hidraw={hidraw} event={event}")
    for label, path, fn in (("hidraw", hidraw, buzz_hidraw), ("evdev", event, buzz_evdev),
                            ("steam", steam_pad(), buzz_evdev)):
        if not path:
            print(f"[{label}] not available, skipped")
            continue
        print(f"[{label}] buzzing {path} for 1 s ...", flush=True)
        try:
            fn(path)
        except OSError as err:
            print(f"[{label}] failed: {err}")
        time.sleep(1)
    print("done: which of hidraw / evdev / steam did you feel?")


if __name__ == "__main__":
    sys.exit(main())
