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
 *
 * Only one process may have a pad open at a time (-EBUSY otherwise).
 */
#ifndef _UAPI_DS4T_H
#define _UAPI_DS4T_H

#include <linux/ioctl.h>
#include <linux/types.h>

/* Bumped on any incompatible change to this header. */
#define DS4T_ABI_VERSION	1

/* Largest report the node transfers (the largest real one is 78 bytes). */
#define DS4T_MAX_REPORT		128

#define DS4T_UNIQ_LEN		64
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

#define DS4T_IOC_MAGIC		0xD4

#define DS4T_IOC_GET_INFO	_IOR(DS4T_IOC_MAGIC, 0x01, struct ds4t_info)

#endif /* _UAPI_DS4T_H */
