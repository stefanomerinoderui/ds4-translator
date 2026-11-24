# Design Study: A ds4-translator Kernel Module

This document describes how `ds4-translator` could ship its own Linux kernel
module, what it would replace, what it would cost, and a suggested plan. It
is a design study, not a commitment: nothing here is implemented.

Statements about kernel behavior that have not been checked against real
hardware or the current kernel source are marked **(verify)**.

## 1. Why consider a kernel module

Almost all of the daemon's complexity comes from one problem: the kernel's
`hid-playstation` driver binds the physical pad first, and the daemon then
has to take it away without anything else noticing. The hide methods in
[how-it-works.md](how-it-works.md#2a-hide-methods-ds4-ctl-set-hide-method)
stack several workarounds to do that:

| Workaround | Where | Why it exists |
| --- | --- | --- |
| `chmod 000`, `setfacl -b`, `EVIOCGRAB` | `open_and_hide_physical()` | Keep other programs off nodes `hid-playstation` created |
| udev rule stripping `uaccess` | `72-ds4-translator-hide.rules` | Close the window before the daemon reacts |
| sysfs unbind + libusb interrupt transport | `hid-unbind-detect.*`, `usb-hid-transport.*` | Remove USB nodes entirely, then read the pad without a driver |
| `usbhid` reclaim retry loop | `usb-hid-transport.cpp` | The kernel grabs the interface back during BT→USB hot-swap |
| HID-BPF swallow program + ring buffer | `hid-bpf-transport.*`, `src/hid-bpf/` | Bluetooth can't be unbound, so reports are intercepted instead |
| `udev-hid-bpf` runtime dependency | `Makefile` | Loads and attaches the BPF program |
| `hid-generic` swap + global `ignore_special_drivers` toggle | `main.cpp` | Get rid of the physical pad's battery, LED and force-feedback devices |
| Full-report kick (`HIDIOCGFEATURE 0x02`/`0x05`) | `kick_bt_full_report_mode()` | `hid-generic` doesn't switch the pad out of basic mode |
| MAC squat, emulated device created at startup | `select_virtual_mac()` | Make `hid-playstation` refuse the real pad before it registers anything |
| `/dev` node unlink, `mknod` restore, 500 ms re-unlink sweep | `reapply_unlinked_nodes()` | Hide nodes that can't be removed by unbinding |
| Crash recovery rebind script | `rebind-unbound-controllers.sh` | A killed daemon leaves pads driverless or on `hid-generic` |

Each one has a documented race, side effect or global impact: the
`ignore_special_drivers` window affects every HID device that probes during
it, the MAC squat makes a controller visible to games whenever the daemon
runs, and the HID-BPF route already crashed the kernel once (see
`tools/hid-bpf-probe/README.md`).

A HID driver that binds the physical pad itself, before `hid-playstation`,
removes the problem at its source: no other driver ever creates a node,
battery, LED or input device for the pad, so there is nothing to hide.

## 2. Scope options

| Option | What runs in the kernel | Daemon role |
| --- | --- | --- |
| **A. Binder only** (recommended) | Claim the physical pad; expose a private report channel | Unchanged above the transport layer |
| B. Binder + in-kernel translation | A plus report translation into a virtual HID device | Configuration and IPC only |
| C. Full in-kernel emulation | B plus an in-kernel USB gadget function for the virtual pad | Configuration only |

Option A delivers almost all of the benefit. B and C move latency-insensitive
logic into the kernel, where bugs crash the machine instead of a service:
the daemon's own timing view already shows translation costs only
microseconds, so there is little latency to win. C would also mean
reimplementing what `functionfs` already does in user space, including the
vendor-class Xbox descriptors. The rest of this document covers option A.

## 3. Option A design: `hid-ds4-translator`

### 3.1 Binding

The module is an ordinary HID driver (`module_hid_driver()`), matching the
same IDs the daemon handles today:

The Sony vendor and DS4 product IDs live in the kernel's private
`drivers/hid/hid-ids.h`, which is not part of the installed headers, so the
module defines them itself (`0x054c`, `0x05c4`, `0x09cc`).

```c
static const struct hid_device_id ds4t_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS4_CONTROLLER) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS4_CONTROLLER_2) },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS4_CONTROLLER) },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS4_CONTROLLER_2) },
	{ }
};
MODULE_DEVICE_TABLE(hid, ds4t_devices);
```

**Winning the race against `hid-playstation`.** When a device appears, the
driver core tries the bus's drivers in registration order and binds the
first one whose probe succeeds **(verify for the HID bus specifically)**.
So the module has to be registered before `hid-playstation`:

- A modprobe soft dependency helps when `hid-playstation` is loaded through
  the same modprobe call:

  ```
  # /usr/lib/modprobe.d/ds4-translator.conf
  softdep hid_playstation pre: hid_ds4_translator
  ```

  It is not a guarantee: udev can autoload `hid_playstation` by modalias
  concurrently, and a built-in driver always registers first.

- Therefore the rebind is the primary mechanism, not a fallback. After the
  module is loaded (and whenever a pad is already bound), it must move any
  pad bound to `playstation` over to `ds4-translator`, either in the module
  (detach and reprobe) or by the daemon via sysfs unbind/bind at startup.
  That is the mechanism the daemon already uses, but needed once per boot
  instead of on every connection.

**Other drivers matching the same IDs.** `hid-sony` is also shipped as a
module, but on kernel 7.3.5 its aliases do not include `054c:05c4` or
`054c:09cc`; only `hid-playstation` does (checked with `modinfo`). Older
kernels may differ, and the module does not depend on it: the takeover
applies to whichever driver holds a matching pad.

**Not touching the emulated controller.** The emulated DS4 uses the same
`054c:05c4` ID, and under the `functionfs` backend it is a real USB device
(behind `dummy_hcd`), so ID matching alone would claim it too. `probe()`
must decline it with `-ENODEV` so it falls through to `hid-playstation` as
today. Reliable discriminators, in order of preference:

1. The USB device's host controller is `dummy_hcd` (walk up from
   `hid_to_usb_dev()` to the bus and check the HCD driver name).
2. For `uhid`-backed emulation, real Bluetooth pads are also `uhid`
   devices (owned by `bluetoothd`), so the transport cannot tell them
   apart. The daemon must tell the module which MAC it is emulating
   through the control interface (section 3.3) and the module skips any
   device whose `uniq` matches. The daemon has to set this before it
   creates the virtual pad, or the module could claim it.

**Daemon not running.** The module only claims pads while a daemon is
attached, which fixes the question of what a pad does without one: it simply
belongs to `hid-playstation`, as if the module were not loaded.

- The daemon attaches by opening `/dev/ds4-translator/control` (one opener at
  a time), naming its emulated pad's MAC with `DS4T_IOC_CTL_SET_IGNORE_MAC`,
  and then issuing `DS4T_IOC_CTL_ATTACH`. The MAC comes first because the
  attach takes over every matching pad on the bus, and the emulated pad
  (same IDs) must not be among them.
- Closing the control node, including by the daemon being killed, detaches:
  every claimed pad is unbound and the bus is rescanned so each lands back on
  its regular driver. This replaces `rebind-unbound-controllers.sh` for the
  driver method, with no `ExecStopPost`.
- Both takeover and release run on the module's own workqueue, not in the
  caller's context. Probing `hid-playstation` reads feature reports with an
  interruptible wait, which fails at once with `-EIO` when a signal is
  pending, as it is while a killed daemon's file descriptors are closed;
  done there, the pad would come back driverless.
- The open control node pins the module, so `rmmod` is refused while a daemon
  is attached; there is no need for `ExecStopPost=modprobe -r`.

Briefly letting `hid-playstation` bind, with its battery notification, on
attach is the price of this policy. It happens once per daemon start, and
only when the pad was connected before the daemon.

### 3.2 Probe

```c
static int ds4t_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;

	if (ds4t_is_emulated(hdev))
		return -ENODEV;

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	/* Start the transport but create no hidraw, input or ff device. */
	ret = hid_hw_start(hdev, HID_CONNECT_DRIVER);
	if (ret)
		return ret;

	ret = hid_hw_open(hdev);
	if (ret)
		goto err_stop;

	/* Bluetooth: switch to the extended 0x11 report (see Part 1 of
	 * unimplemented-features.md), as hid-playstation does in probe(). */
	if (hdev->bus == BUS_BLUETOOTH)
		ds4t_read_feature(hdev, 0x05, 41);

	ret = ds4t_register_pad(hdev);  /* char device, section 3.3 */
	if (ret)
		goto err_close;

	return 0;

err_close:
	hid_hw_close(hdev);
err_stop:
	hid_hw_stop(hdev);
	return ret;
}
```

`hid_hw_start(hdev, HID_CONNECT_DRIVER)` is the key line:
`HID_CONNECT_DRIVER` marks the device as claimed by the driver itself, and
without the `HID_CONNECT_HIDINPUT`/`HIDRAW`/`FF` flags the HID core creates
no `hidraw`, `input` or force-feedback device, so there are no `/dev`
nodes, no `power_supply`, and no LED class device to hide. Passing `0`
would not work: with nothing claimed, `hid_connect()` fails with
`-ENODEV` ("device has no listeners") **(verify against the current
kernel source)**.

Reports arrive in `.raw_event`, which copies them into a per-pad queue and
returns `1` so the HID core does no further processing.

The full-report read also gives the module the pad's calibration data,
which can be kept and served to the daemon (item 4 in
[unimplemented-features.md](unimplemented-features.md#4-real-imu-calibration-passthrough)).

### 3.3 Interface to the daemon

Every transport in `main.cpp` already returns a file descriptor with the
same contract: pollable, one input report per `read()`, one output report
per `write()`, EOF or `POLLHUP` on disconnect. The module exposes exactly
that, so the daemon swaps transports without touching the report path.

One character device per pad, created with `0600 root:root` and named
after the HID device's sysfs name, so the daemon can open it from the
`hid_id` it already has from the uevent monitor without any lookup:

```
/dev/ds4-translator/0003:054C:05C4.0007
```

It is a `misc` device (so no class or `cdev` bookkeeping) and only one
process may have it open (`-EBUSY` otherwise). Reports queued before an
`open()` are discarded, and a full queue drops its oldest report, since
input reports are state snapshots. The interface is defined in
`include/uapi/ds4t.h`, which carries an ABI version.

The name deliberately is not `hidraw*` and carries no input properties,
so SDL, Steam, browsers and Wine never enumerate it.

| Operation | Behavior |
| --- | --- |
| `read()` | One input report, exactly as the pad sent it (`0x01` USB, `0x11` Bluetooth) |
| `write()` | One output report, sent with `hid_hw_output_report()`, falling back to `hid_hw_raw_request(HID_REQ_SET_REPORT)` |
| `poll()` | `POLLIN` when a report is queued; `POLLHUP` after the pad is removed |
| `ioctl(DS4T_GET_INFO)` | Bus, VID, PID, MAC (`hdev->uniq`), HID ID string |
| `ioctl(DS4T_GET_FEATURE)` / `ioctl(DS4T_SET_FEATURE)` | Feature reports via `hid_hw_raw_request()`, for calibration and pairing (Part 2 of [unimplemented-features.md](unimplemented-features.md)) |

A misc control device, `/dev/ds4-translator/control`, carries the
module-wide state: attach (which turns the module on) and detach (the close),
and the emulated MAC to ignore; see section 3.1. Announcing pad arrival and
removal through it, to replace the daemon's raw uevent socket, is not
implemented: the HID-bus uevents already carry everything the daemon needs.

Exposing feature reports through the module also unlocks the calibration
work: the daemon becomes the one process that can talk to the pad, so
`ds4-ctl calibrate` goes through it.

### 3.4 Output pacing over Bluetooth

The module does not fix the `bluetoothd` send-buffer problem documented
on `BtOutputPacer`. With BlueZ's default `UserspaceHID=true`, the
Bluetooth `hid_device` is itself a `uhid` device owned by `bluetoothd`, so
every output report still goes through `bluetoothd`'s socket. The pacer
stays in the daemon.

Setting `UserspaceHID=false` in `/etc/bluetooth/input.conf` moves the
Bluetooth HID session into the kernel's `hidp` driver, which may behave
better under load. That is an independent experiment, unrelated to the
module, and it changes BlueZ behavior system-wide **(verify)**.

## 4. What changes in the daemon

**Removed** (around a quarter of `main.cpp` plus three source files, by
rough estimate):

- `usb-hid-transport.*` and the libusb dependency.
- `hid-bpf-transport.*`, `src/hid-bpf/`, the `udev-hid-bpf` dependency.
- The `hid-generic` swap, the `ignore_special_drivers` toggle, the
  full-report kick.
- The MAC squat, `/var/lib/ds4-translator/last-physical-mac`, creating the
  emulated device at startup, `ds4-ctl set-mac-squat`.
- Node unlinking and restoring, the 500 ms re-unlink sweep, the
  `chmod`/`setfacl`/`EVIOCGRAB` paths.
- `72-ds4-translator-hide.rules` and `rebind-unbound-controllers.sh`.
- The BT→USB hot-swap rebind dance: `hid-playstation`'s duplicate-MAC
  refusal never comes into play, because it never sees either connection.

**Kept:**

- `ds4-ctl set-hide-method`, with a new `driver` method as the default and
  the current methods as fallbacks when the module isn't loaded (no headers
  installed, Secure Boot refusing an unsigned module, unsupported kernel).
  That fallback is not optional: a missing module must never leave a user
  without a working controller.
- Virtual device creation (`uhid` and `functionfs`), `udev-spoof`, IPC,
  configuration, the Bluetooth output pacer.

## 5. Costs and risks

**Stability.** A bug in the module crashes or hangs the kernel, not the
service. Use-after-free on disconnect is the classic HID driver bug: the
pad can vanish while the daemon is blocked in `read()` or mid-`ioctl()`, so
the character device needs reference counting decoupled from the
`hid_device` lifetime (see how `hidraw.c` handles `hidraw->exist`).
Feature-report ioctls over Bluetooth are the worst case: with `UserspaceHID`
the request round-trips through `bluetoothd` and can time out, so every
blocking call needs a timeout and must be woken on disconnect.

**Daemon crash.** Pads are released when the control node closes (section
3.1), so a killed daemon leaves no pad dead. This is covered by a
`SIGKILL` test in `tests/kernel/test-binder.sh`.

**Licensing.** The HID and driver-core symbols the module needs are
`EXPORT_SYMBOL_GPL`, so the module must be GPL-2.0 compatible. The BPF
program is already GPL-2.0-only, but the repository has no top-level
license yet: there is no `LICENSE` file and `packaging/arch/PKGBUILD.in`
uses `LicenseRef-unknown`. This needs settling before a module ships.

**Building and distribution.**

- Out-of-tree builds need the running kernel's headers. Ship via DKMS
  (`ds4-translator-dkms` package) so the module rebuilds on kernel
  upgrades.
- CachyOS kernels are built with Clang and often LTO; out-of-tree modules
  must be built with the same toolchain (`make LLVM=1`), which DKMS needs
  to be told about.
- Secure Boot systems refuse unsigned modules. DKMS can sign with a
  Machine Owner Key of the user's own (the distribution's kernel signing key
  is not available), but the user has to enroll it once (`mokutil
  --import`), which needs a reboot and console interaction.
- Kernel API churn: HID driver callbacks change signature from time to
  time (for example `report_fixup` became `const` in 6.12). Expect to keep
  `#if LINUX_VERSION_CODE` shims and to test against several kernels per
  release.

**Interaction with other software.** `hid-playstation` will not drive a
physical DS4 at all while the module is loaded, so other programs that
expect it (for example a game launcher reading the battery level) lose
access, by design. This is already true of the `unlink` method.

## 6. Testing

- **Without hardware:** `uhid` can create a device with `bus =
  BUS_BLUETOOTH` or `BUS_USB` and the DS4 VID/PID; the module binds it like
  a real pad. Feeding recorded reports (for example from the wiki or
  `tools/ds4-intercept`) gives a repeatable test of the probe, report and
  disconnect paths.
- **Kernel under test:** run the module in a throwaway VM with
  [virtme-ng](https://github.com/arighi/virtme-ng), with `KASAN`, `lockdep`
  and `CONFIG_DEBUG_OBJECTS` enabled, so a crash costs a VM, not the
  desktop.
- **Disconnect torture:** unplug and Bluetooth-drop the pad while the
  daemon is blocked in `read()`, `write()` and each `ioctl()`.
- **CI:** build against the headers of the current stable, the current
  LTS, and the CachyOS kernel in the existing GitHub workflow; `checkpatch.pl
  --strict` and `sparse` as the linters.

## 7. Alternatives to an out-of-tree module

- **Upstream a hook in `hid-playstation`.** A module parameter or per-device
  sysfs attribute that makes the driver skip input, battery and LED
  registration for a given MAC would remove most workarounds with no
  out-of-tree code to maintain. It needs a convincing use case for
  upstream maintainers and a long wait before it reaches distributions.
- **`driver_override` for the HID bus.** The HID bus lacks the per-device
  `driver_override` that PCI and platform buses have (the reason the
  `ignore_special_drivers` toggle exists). Adding it upstream would allow
  binding the pad to `hid-generic` race-free, which, together with HID-BPF,
  covers much of what the module would do.
- **Write the module in Rust.** The running kernel has `CONFIG_RUST=y`, but
  the Rust HID abstractions are not a stable out-of-tree target, so a C
  module is the practical choice for a small shim.
- **Keep the current design.** Everything in section 1 works today. The
  module is worth it only if maintaining the workarounds costs more than
  maintaining kernel code and DKMS packaging.

## 8. Suggested plan

1. **Prototype** the binder with `hid_hw_start(hdev, HID_CONNECT_DRIVER)`
   and a debugfs dump of incoming reports, tested first against a `uhid`
   fake pad in a VM (section 6). Verify the open questions marked above, in
   this order: that `HID_CONNECT_DRIVER` binds with no `hidraw`/`input`
   nodes, rebinding a pad from `hid-playstation` and `hid-sony`, declining
   the emulated device, and disconnect while `read()` is blocked.
2. **Character device** with the read/write/poll contract, plus a new
   daemon transport behind `hide_method=driver`, falling back to `unbind`
   when the module is absent.
3. **Feature-report ioctls**, then calibration passthrough (Part 1, item 4
   of the unimplemented-features doc).
4. **Packaging** (section 9): DKMS, modprobe `softdep`, service
   `ExecStartPre=-modprobe` (no `ExecStopPost` needed), Secure Boot
   instructions in the README.
5. **Remove** the workarounds in section 4 only after the module has run
   as the default for a release, keeping `unbind` as the fallback.

## 9. Packaging as a separate package

The module ships as its own package so the daemon stays buildable and
usable on systems without kernel headers.

- **`ds4-translator-dkms`:** module source, `dkms.conf` (with `LLVM=1`
  handling and MOK signing), the modprobe config, and the uapi header
  `ds4t.h` (ioctl numbers and structs).
- **Daemon:** gains only a `driver` transport that includes `ds4t.h`. It
  probes `/dev/ds4-translator/control` and falls back to `unbind` when it
  is absent.
- **ABI versioning:** `DS4T_GET_INFO` returns a `DS4T_ABI_VERSION`, so a
  daemon and module of different releases refuse each other cleanly
  instead of misreading structs.
- **Location:** start as a `kernel/` subdirectory of this repository so
  the uapi header has one source of truth; split into its own repository
  only once the ABI is stable.
- **Decisions needed before shipping:**
  - License. GPL-2.0 is forced for the module; the repository has no
    `LICENSE` file at all.
  - Minimum supported kernel (for example the 6.12 LTS), which sets the
    size of the `LINUX_VERSION_CODE` shim layer.
