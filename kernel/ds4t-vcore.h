/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DS4T_VCORE_H
#define DS4T_VCORE_H

/*
 * The virtual controller's core (ds4t-vcore.c) and the frontends that present
 * it to the host: a bare hid_device (ds4t-vhid.c, the "kernel" backend) or a
 * USB gadget (the "kernel-usb" backend, a module of its own).
 *
 * The core owns everything that does not depend on how the pad reaches the
 * host: the followed pad, translation, rumble and LED state and their trip
 * back to the pad, and the feature reports. A frontend only creates and
 * removes the device and carries reports both ways.
 */

#include <linux/hid.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "ds4t.h"
#include "ds4t-translate.h"

struct ds4t_pad;
struct ds4t_virtual;

/* Forwarded feature reports, read from a USB pad ahead of time. */
#define DS4T_FCACHE_MAX		48
#define DS4T_FEATURE_MAX	64

struct ds4t_fcache {
	unsigned int count;
	struct {
		u8 id;
		u8 len;
		u8 data[DS4T_FEATURE_MAX];
	} e[DS4T_FCACHE_MAX];
};

struct ds4t_vfront_ops {
	/* Sleeps. Creates the device the host sees; 0 or a negative errno. */
	int (*start)(struct ds4t_virtual *v, const struct ds4t_virtual_cfg *cfg);
	/*
	 * Sleeps. Removes it; no op may run once this returns, and no
	 * ds4t_vcore_*() call may be in flight from this frontend.
	 */
	void (*stop)(struct ds4t_virtual *v);
	/* Atomic, v->lock held: hand one input report to the host. */
	void (*input)(struct ds4t_virtual *v, u8 *report, size_t len);
	struct module *owner;
};

struct ds4t_out_state {
	u8 motor_left, motor_right;
	u8 red, green, blue;
};

struct ds4t_virtual {
	const struct ds4t_vfront_ops *ops;
	void *front;		/* the frontend's own state */
	u32 type;		/* DS4T_VIRTUAL_DS4 or _DUALSENSE */
	u8 mac_le[6];		/* the pad's address, byte-reversed as on the wire */

	/* Set once ops->start() has returned; input is dropped before. */
	bool live;

	spinlock_t lock;	/* everything below */
	struct ds4t_pad *src;	/* followed pad, holds a reference; or NULL */
	u8 seq;			/* DualSense report counter */
	u8 inbuf[DS4T_VIRT_REPORT_SIZE];
	struct ds4t_fcache *fcache;	/* NULL until read, or for a BT pad */

	struct ds4t_out_state out;
	/* hid-playstation pushes a default LED colour at probe; drop that one. */
	bool skip_next_led;
	unsigned long last_sent;
	struct delayed_work output_work;
	struct work_struct fcache_work;
};

/* For frontends; all safe in atomic context unless noted. */

/*
 * A feature report the core builds (calibration, MAC, firmware, ...), or one
 * from the cache. Returns its size, or -EPIPE for one to stall on.
 */
int ds4t_vcore_get_feature(struct ds4t_virtual *v, u8 reportnum, u8 *buf,
			   size_t len);
/*
 * Sleeps. The same, but a forwarded report not cached yet is read from the
 * pad there and then.
 */
int ds4t_vcore_get_feature_sync(struct ds4t_virtual *v, u8 reportnum, u8 *buf,
				size_t len);
/* An output report from the host: rumble and LED, relayed to the pad. */
void ds4t_vcore_output(struct ds4t_virtual *v, const u8 *data, size_t len);

/* The hid_device frontend (ds4t-vhid.c). */
extern const struct ds4t_vfront_ops ds4t_vhid_ops;

#endif /* DS4T_VCORE_H */
