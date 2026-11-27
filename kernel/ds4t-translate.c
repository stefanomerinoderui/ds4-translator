// SPDX-License-Identifier: GPL-2.0-only
/*
 * Report translation, see ds4t-translate.h. Offsets follow hid-playstation's
 * struct layouts; they are spelled out as constants rather than packed
 * structs so the file is endianness- and alignment-clean.
 */

#ifdef __KERNEL__
#include <linux/crc32.h>
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "ds4t-translate.h"

/* DS4 USB input report 0x01: id, common, num_touch, 3 touch reports. */
#define USB_COMMON_OFF		1
#define USB_NUM_TOUCH_OFF	(USB_COMMON_OFF + DS4T_COMMON_SIZE)
#define USB_TOUCH_OFF		(USB_NUM_TOUCH_OFF + 1)
#define USB_MAX_TOUCH		3

/* DS4 Bluetooth input report 0x11: id, 2 reserved, common, num_touch, 4 touch. */
#define BT_COMMON_OFF		3
#define BT_NUM_TOUCH_OFF	(BT_COMMON_OFF + DS4T_COMMON_SIZE)
#define BT_TOUCH_OFF		(BT_NUM_TOUCH_OFF + 1)
#define BT_CRC_OFF		(DS4T_BT_INPUT_SIZE - 4)

/* Touch report: timestamp, then two contacts of contact/x_lo/x_hi|y_lo/y_hi. */
#define TOUCH_POINT_OFF		1
#define TOUCH_POINT_SIZE	4
#define TOUCH_NONE		0x80	/* contact byte with bit 7 set: no finger */

/* common[] fields shared by the DS4 and DualSense builders. */
#define C_X			0
#define C_Y			1
#define C_RX			2
#define C_RY			3
#define C_BUTTONS		4
#define C_Z			7
#define C_RZ			8
#define C_TIMESTAMP		9
#define C_GYRO			12
#define C_ACCEL			18
#define C_STATUS		29

/* DualSense input report 0x01 payload offsets (after the report ID). */
#define DS5_X			1
#define DS5_Z			5
#define DS5_SEQ			7
#define DS5_BUTTONS		8
#define DS5_GYRO		16
#define DS5_ACCEL		22
#define DS5_TIMESTAMP		28
#define DS5_POINTS		33
#define DS5_STATUS		53

/* Status bytes for a pad that has no hardware behind it: full, on cable. */
#define DS4_STATUS_NO_BATTERY	0x1B
#define DS5_STATUS_NO_BATTERY	0x2B

u32 ds4t_crc32(u8 seed, const u8 *data, size_t len)
{
#ifdef __KERNEL__
	u32 crc = crc32_le(0xFFFFFFFF, &seed, 1);

	return ~crc32_le(crc, data, len);
#else
	u32 crc = 0xFFFFFFFF;
	size_t i;
	int bit;

	/* Bitwise form of the same reflected IEEE 802.3 CRC; speed is moot. */
	crc ^= seed;
	for (bit = 0; bit < 8; bit++)
		crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (bit = 0; bit < 8; bit++)
			crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
	}
	return ~crc;
#endif
}

static u32 le32(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) |
	       ((u32)p[3] << 24);
}

int ds4t_parse_input(const u8 *report, size_t len, struct ds4t_input *in)
{
	size_t common_off, touch_off;
	u8 num, max_touch;
	unsigned int i;

	if (len >= DS4T_USB_INPUT_SIZE && report[0] == DS4T_USB_INPUT_ID) {
		common_off = USB_COMMON_OFF;
		num = report[USB_NUM_TOUCH_OFF];
		touch_off = USB_TOUCH_OFF;
		max_touch = USB_MAX_TOUCH;
	} else if (len >= DS4T_BT_INPUT_SIZE &&
		   report[0] == DS4T_BT_INPUT_ID) {
		if (ds4t_crc32(0xA1, report, BT_CRC_OFF) !=
		    le32(report + BT_CRC_OFF))
			return -1;
		common_off = BT_COMMON_OFF;
		num = report[BT_NUM_TOUCH_OFF];
		touch_off = BT_TOUCH_OFF;
		max_touch = DS4T_MAX_TOUCH;
	} else {
		return -1;
	}

	memset(in, 0, sizeof(*in));
	memcpy(in->common, report + common_off, DS4T_COMMON_SIZE);
	in->num_touch = num > max_touch ? max_touch : num;
	for (i = 0; i < in->num_touch; i++)
		memcpy(in->touch[i], report + touch_off + i * DS4T_TOUCH_SIZE,
		       DS4T_TOUCH_SIZE);
	return 0;
}

void ds4t_build_ds4(const struct ds4t_input *in, bool real_battery,
		    u8 out[DS4T_VIRT_REPORT_SIZE])
{
	unsigned int i;
	u8 num = in->num_touch > USB_MAX_TOUCH ? USB_MAX_TOUCH : in->num_touch;
	/* A Bluetooth report can carry one packet more than USB has room for:
	 * keep the newest ones, as the daemon does. */
	u8 first = in->num_touch - num;

	memset(out, 0, DS4T_VIRT_REPORT_SIZE);
	out[0] = DS4T_USB_INPUT_ID;
	memcpy(out + USB_COMMON_OFF, in->common, DS4T_COMMON_SIZE);
	if (!real_battery) {
		out[USB_COMMON_OFF + C_STATUS] = DS4_STATUS_NO_BATTERY;
		out[USB_COMMON_OFF + C_STATUS + 1] = 0;
	}
	out[USB_NUM_TOUCH_OFF] = num;
	for (i = 0; i < num; i++)
		memcpy(out + USB_TOUCH_OFF + i * DS4T_TOUCH_SIZE, in->touch[first + i],
		       DS4T_TOUCH_SIZE);
	/* Unused slots read as "no finger", not as a touch at (0,0). */
	for (; i < USB_MAX_TOUCH; i++) {
		u8 *slot = out + USB_TOUCH_OFF + i * DS4T_TOUCH_SIZE;

		slot[TOUCH_POINT_OFF] = TOUCH_NONE;
		slot[TOUCH_POINT_OFF + TOUCH_POINT_SIZE] = TOUCH_NONE;
	}
}

void ds4t_build_dualsense(const struct ds4t_input *in, u8 seq,
			  bool real_battery, u8 out[DS4T_VIRT_REPORT_SIZE])
{
	const u8 *c = in->common;
	u8 *p = out;	/* out[0] is the report ID, fields are 1-based */

	memset(out, 0, DS4T_VIRT_REPORT_SIZE);
	out[0] = DS4T_USB_INPUT_ID;
	p[DS5_X + 0] = c[C_X];
	p[DS5_X + 1] = c[C_Y];
	p[DS5_X + 2] = c[C_RX];
	p[DS5_X + 3] = c[C_RY];
	p[DS5_Z + 0] = c[C_Z];
	p[DS5_Z + 1] = c[C_RZ];
	p[DS5_SEQ] = seq;
	p[DS5_BUTTONS + 0] = c[C_BUTTONS + 0];
	p[DS5_BUTTONS + 1] = c[C_BUTTONS + 1];
	p[DS5_BUTTONS + 2] = c[C_BUTTONS + 2] & 0x03;
	p[DS5_BUTTONS + 3] = 0;
	memcpy(p + DS5_GYRO, c + C_GYRO, 6);
	memcpy(p + DS5_ACCEL, c + C_ACCEL, 6);
	/* The 16-bit DS4 timestamp, widened as is (little endian). */
	p[DS5_TIMESTAMP + 0] = c[C_TIMESTAMP + 0];
	p[DS5_TIMESTAMP + 1] = c[C_TIMESTAMP + 1];

	if (in->num_touch > 0) {
		/* Newest report of the batch: the older ones are the same fingers. */
		const u8 *t = in->touch[in->num_touch - 1];

		memcpy(p + DS5_POINTS, t + TOUCH_POINT_OFF,
		       2 * TOUCH_POINT_SIZE);
	} else {
		p[DS5_POINTS] = TOUCH_NONE;
		p[DS5_POINTS + TOUCH_POINT_SIZE] = TOUCH_NONE;
	}

	if (real_battery) {
		enum ds4t_battery_state state;
		int percent;

		ds4t_decode_ds4_battery(c[C_STATUS], &percent, &state);
		p[DS5_STATUS] = ds4t_encode_dualsense_battery(percent, state);
	} else {
		p[DS5_STATUS] = DS5_STATUS_NO_BATTERY;
	}
}

void ds4t_decode_ds4_battery(u8 status0, int *percent,
			     enum ds4t_battery_state *state)
{
	u8 level = status0 & 0x0F;

	if (status0 & 0x10) {
		if (level > 10) {
			*percent = 100;
			*state = DS4T_BATTERY_FULL;
		} else {
			*percent = level * 10;
			*state = DS4T_BATTERY_CHARGING;
		}
	} else {
		*percent = level > 8 ? 100 : (level + 1) * 10;
		*state = DS4T_BATTERY_DISCHARGING;
	}
	if (*percent > 100)
		*percent = 100;
}

u8 ds4t_encode_dualsense_battery(int percent, enum ds4t_battery_state state)
{
	u8 bits;
	int level;

	switch (state) {
	case DS4T_BATTERY_FULL:
		bits = 0x2;
		break;
	case DS4T_BATTERY_CHARGING:
		bits = 0x1;
		break;
	default:
		bits = 0x0;
		break;
	}
	level = (percent - 5) / 10;
	if (level < 0)
		level = 0;
	if (level > 10)
		level = 10;
	return (u8)((bits << 4) | (level & 0x0F));
}

/* Bluetooth orders the gyro plus/minus words differently from USB. */
static const u8 bt_word_for_usb_word[6] = { 0, 2, 4, 1, 3, 5 };

int ds4t_calibration_to_usb_layout(const u8 *report, size_t len,
				   u8 out[DS4T_CALIBRATION_SIZE])
{
	const size_t usb_size = 1 + DS4T_CALIBRATION_SIZE;
	const size_t bt_size = usb_size + 4;
	const size_t gyro_words = 6;	/* after the three gyro biases */
	unsigned int i;

	if (len == usb_size && report[0] == 0x02) {
		memcpy(out, report + 1, DS4T_CALIBRATION_SIZE);
		return 0;
	}
	if (len != bt_size || report[0] != 0x05)
		return -1;
	if (ds4t_crc32(0xA3, report, len - 4) != le32(report + len - 4))
		return -1;

	memcpy(out, report + 1, DS4T_CALIBRATION_SIZE);
	for (i = 0; i < 6; i++)
		memcpy(out + gyro_words + i * 2,
		       report + 1 + gyro_words + bt_word_for_usb_word[i] * 2, 2);
	return 0;
}

/*
 * Output report layouts (hid-playstation's dualshock4_output_report_* and
 * dualsense_output_report_*): the common block starts right after the report
 * ID on USB, three bytes in on Bluetooth.
 */
#define DS4_OUT_FLAG0		0	/* bit 0 motors, bit 1 LED */
#define DS4_OUT_MOTOR_RIGHT	3
#define DS4_OUT_MOTOR_LEFT	4
#define DS4_OUT_RGB		5
#define DS4_OUT_COMMON_SIZE	8

#define DS5_OUT_FLAG0		0	/* bit 0 vibration, bit 1 haptics select */
#define DS5_OUT_FLAG1		1	/* bit 2 light bar */
#define DS5_OUT_MOTOR_RIGHT	2
#define DS5_OUT_MOTOR_LEFT	3
#define DS5_OUT_FLAG2		38	/* bit 2 vibration, newer firmware */
#define DS5_OUT_RGB		44
#define DS5_OUT_COMMON_SIZE	47

static size_t output_common_offset(bool dualsense, u8 report_id)
{
	if (dualsense)
		return report_id == 0x02 ? 1 : report_id == 0x31 ? 3 : 0;
	return report_id == 0x05 ? 1 : report_id == 0x11 ? 3 : 0;
}

unsigned int ds4t_parse_output(bool dualsense, const u8 *data, size_t len,
			       struct ds4t_output *out)
{
	const size_t common_size = dualsense ? DS5_OUT_COMMON_SIZE
					     : DS4_OUT_COMMON_SIZE;
	unsigned int found = 0;
	size_t base;
	const u8 *c;

	if (!len)
		return 0;
	base = output_common_offset(dualsense, data[0]);
	if (!base || len < base + common_size)
		return 0;
	c = data + base;

	if (!dualsense) {
		if (c[DS4_OUT_FLAG0] & 0x01) {
			out->motor_right = c[DS4_OUT_MOTOR_RIGHT];
			out->motor_left = c[DS4_OUT_MOTOR_LEFT];
			found |= DS4T_OUT_RUMBLE;
		}
		if (c[DS4_OUT_FLAG0] & 0x02) {
			out->red = c[DS4_OUT_RGB];
			out->green = c[DS4_OUT_RGB + 1];
			out->blue = c[DS4_OUT_RGB + 2];
			found |= DS4T_OUT_LED;
		}
		return found;
	}

	if ((c[DS5_OUT_FLAG0] & 0x01) || (c[DS5_OUT_FLAG2] & 0x04)) {
		out->motor_right = c[DS5_OUT_MOTOR_RIGHT];
		out->motor_left = c[DS5_OUT_MOTOR_LEFT];
		/*
		 * With haptics also selected, running both motors lets the big
		 * one drown out the light one (as the daemon's uhid path does).
		 */
		if ((c[DS5_OUT_FLAG0] & 0x02) && out->motor_right &&
		    out->motor_left)
			out->motor_left = 0;
		found |= DS4T_OUT_RUMBLE;
	}
	if (c[DS5_OUT_FLAG1] & 0x04) {
		out->red = c[DS5_OUT_RGB];
		out->green = c[DS5_OUT_RGB + 1];
		out->blue = c[DS5_OUT_RGB + 2];
		found |= DS4T_OUT_LED;
	}
	return found;
}
