/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * ds4t.h - user-space interface of the hid-ds4-translator kernel module,
 * shared by the module (kernel/) and the daemon (src/driver-transport.cpp).
 *
 * Each claimed pad appears as /dev/ds4-translator/<hid_id>, where hid_id is
 * the HID device's sysfs name (for example "0003:054C:05C4.0007"):
 *
 *   read()   one input report, exactly as the pad sent it (0x01 USB, 0x11
 *            Bluetooth); blocks unless O_NONBLOCK, -ENODEV once the pad is gone
 *   write()  one output report, report ID in the first byte
 *   poll()   POLLIN when a report is queued, POLLHUP|POLLERR after removal
 *   ioctl()  pad info, raw feature reports, the calibration read at probe
 *
 * Only one process may have a pad open at a time (-EBUSY otherwise).
 *
 * The module claims nothing until a daemon attaches: it opens
 * /dev/ds4-translator/control (one opener at a time, -EBUSY otherwise), names
 * its emulated pad with DS4T_IOC_CTL_SET_IGNORE_MAC, and then issues
 * DS4T_IOC_CTL_ATTACH, which takes over every matching pad another driver
 * already holds. Closing the node (including the daemon being killed) hands
 * every pad back to the regular drivers. Until the attach, pads behave as if
 * the module were not loaded.
 */
#ifndef _UAPI_DS4T_H
#define _UAPI_DS4T_H

#include <linux/ioctl.h>
#include <linux/types.h>

/* Bumped on any incompatible change to this header. */
#define DS4T_ABI_VERSION	3

/* Largest report the node transfers (the largest real one is 78 bytes). */
#define DS4T_MAX_REPORT		128

#define DS4T_UNIQ_LEN		64
#define DS4T_MAC_LEN		18	/* "AA:BB:CC:DD:EE:FF" plus NUL */
#define DS4T_HID_ID_LEN		32

struct ds4t_info {
	__u32 abi_version;		/* DS4T_ABI_VERSION of the module */
	__u16 bus;			/* BUS_USB (0x03) or BUS_BLUETOOTH (0x05) */
	__u16 vendor;
	__u16 product;
	__u16 reserved;
	char uniq[DS4T_UNIQ_LEN];	/* the pad's MAC, NUL-terminated */
	char hid_id[DS4T_HID_ID_LEN];	/* sysfs name, NUL-terminated */
};

/*
 * A feature report as it crosses the HID layer: data[0] is the report ID,
 * len counts every byte including it.
 */
struct ds4t_feature {
	__u16 len;
	__u16 reserved;
	__u8 data[DS4T_MAX_REPORT];
};

/* ioctl argument of DS4T_IOC_CTL_SET_IGNORE_MAC. */
struct ds4t_mac {
	char str[DS4T_MAC_LEN];		/* NUL-terminated; empty clears it */
};

#define DS4T_IOC_MAGIC		0xD4

/* Pad nodes. */

#define DS4T_IOC_GET_INFO	_IOR(DS4T_IOC_MAGIC, 0x01, struct ds4t_info)

/*
 * Reads a feature report from the pad: set data[0] to the report ID and len
 * to the size wanted; on return len is the size received. Blocks for as long
 * as the transport does (USB control transfer, or a round trip through
 * bluetoothd for a Bluetooth pad), and fails with -ENODEV once the pad is
 * gone.
 */
#define DS4T_IOC_GET_FEATURE	_IOWR(DS4T_IOC_MAGIC, 0x02, struct ds4t_feature)

/* Sends data[0..len) as a feature report. Same blocking as GET_FEATURE. */
#define DS4T_IOC_SET_FEATURE	_IOW(DS4T_IOC_MAGIC, 0x03, struct ds4t_feature)

/*
 * The calibration report the module read when it claimed the pad: report 0x02
 * (37 bytes) over USB, 0x05 (41 bytes, CRC included) over Bluetooth. -ENODATA
 * if that read failed.
 */
#define DS4T_IOC_GET_CALIBRATION _IOR(DS4T_IOC_MAGIC, 0x04, struct ds4t_feature)

/* Control node. */

/* The module's DS4T_ABI_VERSION, as a __u32. */
#define DS4T_IOC_CTL_GET_ABI	_IOR(DS4T_IOC_MAGIC, 0x10, __u32)

/*
 * The MAC (a pad's uniq) of the daemon's own emulated controller, which the
 * module must never claim. Forgotten when the control node is closed.
 */
#define DS4T_IOC_CTL_SET_IGNORE_MAC _IOW(DS4T_IOC_MAGIC, 0x11, struct ds4t_mac)

/*
 * Starts claiming pads, taking over the ones other drivers hold now. Separate
 * from open() so the ignore MAC can be set first; otherwise an emulated pad
 * that already exists would be claimed too. Idempotent.
 */
#define DS4T_IOC_CTL_ATTACH	_IO(DS4T_IOC_MAGIC, 0x12)

#endif /* _UAPI_DS4T_H */
