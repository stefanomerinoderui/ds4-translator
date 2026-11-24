# Unimplemented DualShock 4 Features

This document lists DualShock 4 protocol features that `ds4-translator` does
not implement yet. It is split in two parts, one per source:

1. [Bluetooth HID protocol](#part-1-bluetooth-hid-protocol), from the
   [PS4 Developer wiki DS4-BT page](https://www.psdevwiki.com/ps4/DS4-BT).
2. [Controller configuration and calibration](#part-2-controller-configuration-and-calibration),
   from [dualshock-tools/ds4-tools](https://github.com/dualshock-tools/ds4-tools).

Both were compared against `src/main.cpp`, `src/usb-hid-transport.cpp`,
`src/functionfs-backend.c` and `src/ctl.cpp`.

# Part 1: Bluetooth HID protocol

Everything below the HID layer (HCI, SDP, Class of Device, L2CAP framing,
pairing) is handled by BlueZ: the daemon only ever sees the controller
through `hidraw`, so those sections of the wiki are out of scope here.

## Already implemented (for reference)

- Extended input report `0x11` (78 bytes), including the CRC-32 check over
  the first 74 bytes with seed `0xA1`, and all four touchpad packets.
- Output report `0x11` for rumble and light bar color, with `hw_control =
  0xC0` and CRC-32 seed `0xA2`, paced to one write per 25 ms by
  `BtOutputPacer`.
- Switching the pad into extended-report mode by reading feature report
  `0x02` (falling back to `0x05`) in `kick_bt_full_report_mode()`.
- Battery level and cable/charging status from input byte 33.

## Gaps, in suggested implementation order

### 1. Short input report `0x01` (basic mode)

**Status:** ignored. The parser only accepts `0x01` reports of at least 64
bytes (the USB format), so the 10-byte Bluetooth basic report is dropped.

**Wiki layout** (after the `0xA1` transaction header, which hidraw strips):

| Byte | Content |
| --- | --- |
| 0 | Report ID `0x01` |
| 1 | Left stick X (0 = left) |
| 2 | Left stick Y (0 = up) |
| 3 | Right stick X |
| 4 | Right stick Y |
| 5 | Triangle, Circle, Cross, Square (bits 7–4); D-pad hat (bits 3–0, `0x08` = released) |
| 6 | R3, L3, Options, Share, R2, L2, R1, L1 (bits 7–0) |
| 7 | Counter (bits 7–2), touchpad click (bit 1), PS (bit 0) |
| 8 | L2 analog |
| 9 | R2 analog |

These are the same as the first bytes of the common input block, so a
fallback can fill `dualshock4_input_report_common` with the buttons, sticks
and triggers and leave motion and touch zeroed.

**Why it matters:** if the full-report kick fails, the daemon currently
forwards nothing (logged as "Basic Bluetooth mode -- the full-report kick
did not take"). With a fallback the controller remains usable, minus
gyro, accelerometer and touchpad.

**Effort:** small.

### 2. Newest touchpad packet dropped when emulating a USB DS4

**Status:** bug. A Bluetooth report can carry up to four touchpad packets,
while the USB report has room for three. `emit_input_report()` copies
packets 0–2, so when four arrive the newest one (index 3) is the one lost.

**Fix:** when `num_touch > 3`, copy the last three packets
(`touch_reps[num_touch - 3 .. num_touch - 1]`) instead of the first three.

**Effort:** trivial.

### 3. Light bar flash passthrough

**Status:** not forwarded, and actively disabled.

- `write_physical_output_report()` sets the blink valid flag (`0x04` in
  `valid_flag0`) but always writes flash on/off duration as `0`, which
  turns flashing off on every write.
- The UHID and functionfs output paths only read the rumble (`0x01`) and
  color (`0x02`) flags from games' output reports. The blink flag and its
  on/off bytes (bytes 9–10 of USB report `0x05`; bytes 11–12 of Bluetooth
  report `0x11`) are discarded.

**Wiki layout** (Bluetooth output `0x11`, hidraw offsets): byte 11 = flash
bright duration, byte 12 = flash dark duration.

**Fix:** add the two durations to `BtOutputPacer::State` and the
`cur_*` state, parse them from the virtual device's output reports when
`flags & 0x04` is set, and write them to the physical report. The
`ds4-ctl identify` command could then use the pad's own flash instead of
toggling colors from a thread.

**Effort:** small.

### 4. Real IMU calibration passthrough

**Status:** implemented for the `driver` hide method only
([kernel-driver.md](kernel-driver.md)): the module caches the pad's
calibration report when it claims it, and the daemon serves it, converted to
the USB layout, as feature `0x02` from the emulated DS4 on both the `uhid`
and `functionfs` backends. Verified in a VM against the real
`hid-playstation`, which accepts the converted values. Every other method
still discards the payload of the read and serves a fixed calibration blob;
passing it through there means making `usb-hid-transport` and the HID-BPF
path fetch it too. The emulated DualSense keeps its fixed blob.

**Why it matters:** gyro and accelerometer scaling seen by games does not
match the actual controller.

**Fix:** keep the reply from the kick read, convert the Bluetooth `0x05`
layout to the USB `0x02` layout if needed, and serve it from the
`UHID_GET_REPORT` and functionfs feature handlers. The wiki leaves `0x02`
as "TODO"; the layout is documented in the kernel's `hid-playstation.c`
(`dualshock4_get_calibration_data()`). On USB, ds4-tools reads the same
data with a `0x02` GET and can rewrite it with feature `0x04` (see
[Part 2, item 4](#4-imu-calibration-read-and-write)).

**Effort:** medium.

### 5. Volume control (output `0x11` bytes 21–24)

**Status:** not implemented.

**Wiki layout** (hidraw offsets): headphone left volume, headphone right
volume, mic volume (speculative), speaker volume.

**Why it matters:** only useful once audio output (item 6) exists.

**Effort:** small, but blocked on item 6.

### 6. Audio output reports `0x14`, `0x15`, `0x17`, `0x18`, `0x19`

**Status:** not implemented.

These reports stream SBC-encoded audio to the controller's speaker and
headset jack:

| Report | Content |
| --- | --- |
| `0x14` | Audio only |
| `0x15` | Rumble, light bar and audio |
| `0x17` | Audio only (larger) |
| `0x18` | Audio only (larger, padded) |
| `0x19` | Same leading block as `0x11` (rumble/LED) followed by audio |

Each carries a frame counter, an audio header and SBC frames (sync word
`0x9C`), followed by a CRC-32. The wiki notes the console streams 32 kHz
audio for up to two controllers and 16 kHz for three or more.

**Why it matters:** speaker and headset audio over Bluetooth.

**Open problems:**

- Needs an SBC encoder and a virtual audio sink on the PC side for games to
  output to.
- Bluetooth output already overruns `bluetoothd` above ~40 Hz (see
  `BtOutputPacer`); sustained audio streaming needs a different pacing
  strategy.
- Parts of the format (bytes 2–3, trailing padding) are still marked
  "TODO" or speculative on the wiki.

**Effort:** large; a project of its own.

### 7. Real firmware info (feature `0x06` / `0xA3`)

**Status:** the emulated DS4 answers `0xA3` with a fixed build date
("Mar 25 2016"). The physical pad exposes its real build date through
`0x06` (with CRC) and `0xA3` (without).

**Fix:** read `0xA3` from the physical pad on connect and forward it.

**Effort:** small, low value. ds4-tools documents the payload layout; see
[Part 2, item 5](#5-real-firmware-info).

## Out of scope

- **Authentication (feature `0xF0`, `0xF1`, `0xF2`):** the PS4's RSA
  challenge-response handshake. PC games never issue it, and the emulated
  device never connects to a console.
- **Periodic feature reports `0x03` / `0x04`:** a console keep-alive whose
  payload the wiki marks as unknown.
- **Bluetooth poll interval:** the wiki's update-rate table hints that the
  low bits of the output header byte (`hw_control`) set the report rate,
  which hid-sony exposes as `bt_poll_interval`. The daemon sends `0xC0`
  (bits clear). A setting could be added, but the wiki does not document
  these bits.
- **HCI, SDP, Class of Device, L2CAP, pairing:** handled by BlueZ.

# Part 2: Controller configuration and calibration

[ds4-tools](https://github.com/dualshock-tools/ds4-tools) (also behind the
[DualShock Calibration GUI](https://dualshock-tools.github.io) website)
drives a set of undocumented vendor feature reports to recalibrate the
sticks and triggers, read and change the controller's identity, and pair it
to a Bluetooth host.

## Feature report reference

All of these are sent by ds4-tools as plain USB HID `GET_REPORT` /
`SET_REPORT` control transfers. None of them has been tested over
Bluetooth.

| Report | Direction | Payload | Purpose |
| --- | --- | --- | --- |
| `0x02` | GET | 36 bytes after the report ID (ds4-tools requests 41) | IMU calibration (see Part 1, item 4) |
| `0x04` | SET | 36 bytes | Write IMU calibration |
| `0x08` | SET | `FF, offset_hi, offset_lo` | Select a flash-mirror word to read (`offset < 0x800`) |
| `0x08` | SET | `10, b0, b1` | Write 2-byte serial number |
| `0x11` | GET | 2 bytes | Read the flash-mirror word selected by `0x08` |
| `0x12` | GET | pad MAC (6), `08 25 00`, host MAC (6) | Bluetooth link info |
| `0x13` | SET | host MAC (6), link key (16) | Pair the pad to a Bluetooth host |
| `0x80` | SET | MAC (6) | Overwrite the pad's own Bluetooth MAC |
| `0x81` | GET | MAC (6) | Read the pad's Bluetooth MAC |
| `0x85` / `0x86` | SET / GET | 6 bytes | PCBA manufacturer ID |
| `0x90` | SET | `cmd, device, target[, ...]` | Calibration control (see item 1) |
| `0x91` | GET | `device, target, state` | Calibration state: `1` running, `2` stored |
| `0x92` | GET | `device, target, result` | Calibration result: `0xFF` pending, `1` success |
| `0x93` | GET | 13 bytes | Calibration debug samples, chunked |
| `0xA0` | SET | `04 01 00` | Reset the controller |
| `0xA0` | SET | `0A 01 00` | Flash-mirror changes temporary (lost on reset/standby) |
| `0xA0` | SET | `0A 02 3E 71 7F 89` | Flash-mirror changes permanent |
| `0xA1` | SET | 1 byte | Bluetooth enable bit |
| `0xA3` | GET | 48 bytes | Firmware/hardware version info |

Notable flash-mirror offsets: `12` holds the temporary/permanent flag
(`1` = temporary), `0x700` holds the Bluetooth enable bit.

## Why the daemon is involved at all

While the daemon runs it owns the physical pad: under `unbind`/`unlink`
the USB interface is detached from the kernel and read through libusb, and
under `legacy` the `hidraw` node is permission-blocked. ds4-tools, the
calibration website and BlueZ's cable-pairing plugin therefore cannot reach
the controller without stopping the service. Tools that open the
*emulated* controller instead talk to a device that acknowledges every
`SET_REPORT` and invents or omits every `GET_REPORT` reply, so they either
fail or report success without the physical pad being touched.

## Gaps, in suggested implementation order

### 1. Stick and trigger calibration

**Status:** not implemented.

**Protocol** (from `ds4-calibration-tool.py`). `0x90` takes
`cmd, device, target`, where `cmd` is `1` = start, `2` = store, `3` =
take a sample.

- **Stick center** (`device 1`, `target 1`): send start, then for each
  sample hold both sticks centered and send `3`; send `2` to store. Before
  storing, `0x91` should read `01 01 01` and `0x92` `01 01 FF`; after,
  `01 01 02` and `01 01 01`.
- **Stick range** (`device 1`, `target 2`): send start, move both sticks
  around their full range, send `2` to store. Same `0x91`/`0x92` checks.
- **Triggers** (`device 3`, marked beta upstream): start with
  `01 03 00 03`; for each of L2 and R2 and each of released/mid/full,
  send `03 03 <pos> <side>` twice (`pos` 1/2/3, `side` 1 = L2, 2 = R2);
  store with `02 03 00 03`.

Results only survive a reset or standby if the flash mirror is set to
permanent first (item 2).

**Fix:** add `ds4-ctl calibrate <stick-center|stick-range|triggers>` that
asks the daemon to run the sequence on the physical pad over USB,
prompting the user between steps, and a matching page in the UI. The live
input view of `ds4-ctl test` already shows the sticks, which makes it easy
to check the result.

An alternative is to pass reports `0x90`–`0x93` (and the flash-mirror
reports) from the emulated device through to the physical one, so the
existing website works unmodified on the virtual controller. That is less
code but lets any program with access to the virtual device write to the
pad's flash.

**Risk:** ds4-tools warns these commands can brick a controller, and was
tested on two units. Default to temporary mode, and refuse to run over
Bluetooth until that path has been verified.

**Effort:** medium.

### 2. Flash-mirror mode (temporary / permanent)

**Status:** not implemented.

**Protocol:** read with `0x08 = FF 00 0C` then `0x11` (first byte: `1`
temporary, `0` permanent). Set with `0xA0` as in the table above.

**Fix:** expose as part of the calibration flow (switch to permanent only
for the store step, then back to temporary), rather than as a standalone
setting.

**Effort:** small, once item 1 exists.

### 3. Learn the pad's MAC over USB

**Status:** the MAC used by the MAC squat
(`/var/lib/ds4-translator/last-physical-mac`) is only learned from
`HID_UNIQ` on the first Bluetooth connection.

**Fix:** when the pad is connected over USB, read its MAC with feature
`0x81` (or bytes 0–5 of `0x12`) and store it the same way. A pad that was
first seen on a cable then gets its first Bluetooth connection blocked
from `hid-playstation` too.

**Effort:** small.

### 4. IMU calibration read and write

**Status:** see [Part 1, item 4](#4-real-imu-calibration-passthrough) for
passthrough of the read side. Writing (`0x04`, 36 bytes) is not
implemented.

**Fix:** passthrough first. A write command is only useful together with
a procedure that computes new values, which ds4-tools does not provide.

**Effort:** read side medium (Part 1, item 4); write side low priority.

### 5. Real firmware info

**Status:** see [Part 1, item 7](#7-real-firmware-info-feature-0x06--0xa3).
ds4-tools documents the `0xA3` payload:

| Bytes | Field |
| --- | --- |
| 0–15 | Compile date, ASCII, zero-padded |
| 16–31 | Compile time, ASCII, zero-padded |
| 32–33 | Hardware version major (LE) |
| 34–35 | Hardware version minor (LE) |
| 36–39 | Software version major (LE) |
| 40–41 | Software version minor (LE) |
| 42–43 | Software series (LE) |
| 44–47 | Code size (LE) |

This matches the fixed layout the daemon already fakes. Besides
forwarding it to the emulated device, `ds4-ctl status` could print the
real firmware and hardware version.

**Effort:** small.

### 6. Cable pairing

**Status:** probably broken under `unbind`/`unlink` (not verified).
BlueZ's sixaxis plugin pairs a DS4 plugged in over USB by reading its MAC
and writing the host adapter's MAC and a link key with feature `0x13`. It
finds the pad through its `hidraw` node, which `unbind` never creates, so
a pad that has never been paired over Bluetooth cannot be paired by
plugging it in while the daemon runs.

**Fix:** either let the sixaxis plugin see the pad briefly on first USB
connect, or perform the `0x13` write from the daemon. The second option
also has to create the matching BlueZ device entry under
`/var/lib/bluetooth/<adapter>/<pad MAC>/info` with the same link key.

**Effort:** medium; verify the breakage first.

### 7. Controller reset

**Status:** not implemented. `0xA0 = 04 01 00` reboots the controller (the
USB device disconnects and re-enumerates).

**Fix:** a `ds4-ctl reset-controller` command for recovering a pad in a bad
state without unplugging it. The daemon's existing hot-swap handling has
to treat the re-enumeration as a normal reconnect.

**Effort:** small, low value.

## Out of scope

- **Overwriting the MAC (`0x80`), PCBA ID (`0x85`), serial number (`0x08`
  with `10`) and the Bluetooth enable bit (`0xA1`):** identity and
  factory settings with no use for a translator and a real risk of making
  the controller unusable.
- **Flash dump (`0x08` + `0x11` over `0x000`–`0x7FF`):** a reverse
  engineering aid; ds4-tools itself notes the checksum does not verify.
- **`ds5-calibration-tool.py`:** calibrates a physical DualSense. The daemon
  only ever drives a physical DualShock 4; the DualSense it presents is
  emulated.
