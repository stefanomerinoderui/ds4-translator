/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DS4T_INTERNAL_H
#define DS4T_INTERNAL_H

#include <linux/hid.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/rwsem.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include "ds4t.h"

#define DS4T_NAME		"hid-ds4-translator"

/* Feature reports that return the calibration data (and switch BT to 0x11). */
#define DS4T_FEATURE_CAL_USB	0x02
#define DS4T_FEATURE_CAL_USB_SZ	37
#define DS4T_FEATURE_CAL_BT	0x05
#define DS4T_FEATURE_CAL_BT_SZ	41

#define DS4T_RING_SIZE		32

struct ds4t_report {
	u16 len;
	u8 data[DS4T_MAX_REPORT];
};

/* Fixed-size FIFO; pushing into a full one drops the oldest report. */
struct ds4t_ring {
	struct ds4t_report slots[DS4T_RING_SIZE];
	unsigned int head;		/* next slot to write */
	unsigned int count;		/* valid entries, <= DS4T_RING_SIZE */
	u64 total;			/* reports ever pushed */
};

/*
 * One claimed pad. Reference counted so the char device can outlive the
 * hid_device: the driver holds one reference from probe to remove, and every
 * open file holds one more.
 */
struct ds4t_pad {
	struct kref ref;
	struct list_head list;		/* ds4t_pads, while registered */
	struct hid_device *hdev;
	struct ds4t_info info;
	struct miscdevice misc;
	char devname[DS4T_HID_ID_LEN + 8];	/* "ds4t-<hid_id>" */
	char nodename[DS4T_HID_ID_LEN + 16];	/* "ds4-translator/<hid_id>" */
	struct dentry *dbg_file;

	/*
	 * exist_lock keeps hdev valid for the hid_hw_* calls made on behalf of
	 * user space (write): they hold it for reading, remove() for writing
	 * while it clears exist. exist is also read under lock.
	 */
	struct rw_semaphore exist_lock;
	bool exist;

	spinlock_t lock;		/* protects the fields below */
	wait_queue_head_t wait;
	struct ds4t_ring dbg;		/* what debugfs shows, never consumed */
	struct ds4t_ring queue;		/* what read() consumes */
	bool opened;

	u8 calib[DS4T_FEATURE_CAL_BT_SZ];
	int calib_len;			/* 0 when the read failed */
};

struct ds4t_pad *ds4t_pad_alloc(struct hid_device *hdev);
void ds4t_pad_get(struct ds4t_pad *pad);
void ds4t_pad_put(struct ds4t_pad *pad);
/* The registered pad called hid_id, with a reference taken; NULL if none. */
struct ds4t_pad *ds4t_pad_find(const char *hid_id);

/* Creates the device node and the debugfs file; fails if the node exists. */
int ds4t_pad_register(struct ds4t_pad *pad);
/* Removes both, wakes blocked readers; open files see -ENODEV afterwards. */
void ds4t_pad_unregister(struct ds4t_pad *pad);
/* Called from raw_event(): queue one input report for user space. */
void ds4t_pad_report(struct ds4t_pad *pad, const u8 *data, int size);

/* Attach state (ds4t-driver.c): the module only claims pads while attached. */
bool ds4t_attached(void);
/* Takes over every matching pad another driver holds. */
void ds4t_attach(void);
/* Releases every claimed pad back to the regular drivers. */
void ds4t_detach(void);

/* The daemon's emulated MAC (ds4t-control.c), never to be claimed. */
bool ds4t_mac_ignored(const char *uniq);

/*
 * The virtual controller (ds4t-vcore.c, ds4t-vhid.c): translates a claimed
 * pad's input into a DualShock 4 or DualSense that hid-playstation drives, and
 * relays its output back.
 */
/* Whether hdev is the virtual controller itself. */
bool ds4t_virtual_owns(struct hid_device *hdev);
/* Called from raw_event(); atomic context. */
void ds4t_virtual_input(struct ds4t_pad *pad, const u8 *data, int size);
/* A pad is going away: stop following it. Called before it is unregistered. */
void ds4t_virtual_pad_gone(struct ds4t_pad *pad);
int ds4t_virtual_create(const struct ds4t_virtual_cfg *cfg);
void ds4t_virtual_destroy(void);
int ds4t_virtual_link(const char *hid_id);
int ds4t_virtual_inject(const u8 *report, size_t len);

/* The device the virtual controller hangs off (the control node's). */
struct device *ds4t_control_device(void);

int ds4t_control_init(void);
void ds4t_control_exit(void);

void ds4t_debugfs_init(void);
void ds4t_debugfs_exit(void);

#endif /* DS4T_INTERNAL_H */
