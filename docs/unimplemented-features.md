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
- Short input report `0x01` (basic mode): sticks, buttons and triggers are
  forwarded when the full-report kick fails; motion and touch stay zeroed.
- The newest three of four Bluetooth touchpad packets reach a USB DS4.
- Light bar flash: durations from the games' output reports (`0x05` bytes
  9–10, `0x11` bytes 11–12) are written to the pad.
- Real motion calibration (feature `0x02` over USB, `0x05` over Bluetooth,
  converted to the USB layout) and real firmware info (`0xA3`) are read
  when the pad connects and served to games, and `ds4-ctl status` prints the
  firmware. A failed read keeps the canned reports.

## Gaps, in suggested implementation order

### 1. Volume control (output `0x11` bytes 21–24)

**Status:** not implemented.

**Wiki layout** (hidraw offsets): headphone left volume, headphone right
volume, mic volume (speculative), speaker volume.

**Why it matters:** only useful once audio output (item 2) exists.

**Effort:** small, but blocked on item 2.

### 2. Audio output reports `0x14`, `0x15`, `0x17`, `0x18`, `0x19`

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
| `0x02` | GET | 36 bytes after the report ID (ds4-tools requests 41) | IMU calibration (read side served to games, see Part 1) |
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

### 4. IMU calibration write

**Status:** the read side is done (see "Already implemented" in Part 1).
Writing (`0x04`, 36 bytes) is not implemented.

**Fix:** a write command is only useful together with a procedure that
computes new values, which ds4-tools does not provide.

**Effort:** low priority.

### 5. Real firmware info

**Status:** done; see "Already implemented" in Part 1. The `0xA3` payload
is read on connect and shown by `ds4-ctl status`.

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
