/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DS4T_TRANSLATE_H
#define DS4T_TRANSLATE_H

/*
 * Report translation for the "kernel" emulation backend: parse what a
 * physical DualShock 4 sends, repack it as the DS4 or DualSense the virtual
 * device presents, and build the output reports that go back to the pad.
 *
 * Pure functions on byte buffers, no kernel services beyond crc32 and byte
 * order helpers, so the same file also builds in user space where the unit
 * tests compare it byte for byte with the daemon's own translation in
 * src/translate.cpp (the two must never drift apart).
 */

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include "ds4t-userspace.h"
#endif

#define DS4T_COMMON_SIZE	32	/* x..reserved3, identical on USB and BT */
#define DS4T_TOUCH_SIZE		9	/* timestamp + two 4-byte contacts */
#define DS4T_MAX_TOUCH		4

#define DS4T_USB_INPUT_ID	0x01
#define DS4T_USB_INPUT_SIZE	64
#define DS4T_BT_INPUT_ID	0x11
#define DS4T_BT_INPUT_SIZE	78

#define DS4T_VIRT_REPORT_SIZE	64	/* both virtual DS4 and DualSense */

#define DS4T_CALIBRATION_SIZE	36	/* feature 0x02 payload, USB layout */

/* One physical input report, bus framing removed. */
struct ds4t_input {
	u8 common[DS4T_COMMON_SIZE];
	u8 num_touch;				/* clamped to DS4T_MAX_TOUCH */
	u8 touch[DS4T_MAX_TOUCH][DS4T_TOUCH_SIZE];
};

enum ds4t_battery_state {
	DS4T_BATTERY_DISCHARGING,
	DS4T_BATTERY_CHARGING,
	DS4T_BATTERY_FULL,
};

/* CRC32 as hid-playstation computes it: the seed byte, then data. */
u32 ds4t_crc32(u8 seed, const u8 *data, size_t len);

/*
 * Parses a USB (0x01, >= 64 bytes) or Bluetooth (0x11, >= 78 bytes, CRC
 * checked) input report. Returns 0, or -1 for anything else.
 */
int ds4t_parse_input(const u8 *report, size_t len, struct ds4t_input *in);

/* Virtual DualShock 4 report 0x01, 64 bytes into out. */
void ds4t_build_ds4(const struct ds4t_input *in, bool real_battery,
		    u8 out[DS4T_VIRT_REPORT_SIZE]);

/* Virtual DualSense report 0x01, 64 bytes into out. */
void ds4t_build_dualsense(const struct ds4t_input *in, u8 seq,
			  bool real_battery, u8 out[DS4T_VIRT_REPORT_SIZE]);

void ds4t_decode_ds4_battery(u8 status0, int *percent,
			     enum ds4t_battery_state *state);
u8 ds4t_encode_dualsense_battery(int percent, enum ds4t_battery_state state);

/* What ds4t_parse_output() found in a report (bit mask). */
#define DS4T_OUT_RUMBLE		0x1
#define DS4T_OUT_LED		0x2

/* Rumble and light bar, as a game asks the emulated pad for them. */
struct ds4t_output {
	u8 motor_left, motor_right;
	u8 red, green, blue;
};

/*
 * Reads an output report sent to the emulated pad: a DualShock 4's 0x05 (USB)
 * or 0x11 (Bluetooth layout), a DualSense's 0x02 (USB) or 0x31 (Bluetooth
 * layout). A game that does not take the pad for a USB one sends the
 * Bluetooth layout. DualSense rumble counts with either "compatible
 * vibration" flag (the second is what firmware 2.21 and later use). Fills
 * only the fields it returns a bit for; 0 for anything else.
 */
unsigned int ds4t_parse_output(bool dualsense, const u8 *data, size_t len,
			       struct ds4t_output *out);

/*
 * The calibration payload (USB layout) out of a pad's feature report: 0x02,
 * 37 bytes, or 0x05, 41 bytes with a CRC. Returns 0, or -1 for a wrong ID,
 * size or CRC (out untouched).
 */
int ds4t_calibration_to_usb_layout(const u8 *report, size_t len,
				   u8 out[DS4T_CALIBRATION_SIZE]);

#endif /* DS4T_TRANSLATE_H */
