// SPDX-License-Identifier: GPL-2.0-only
/*
 * The core of the virtual controller (the "kernel" and "kernel-usb" emulation
 * backends), whatever frontend shows it to the host (ds4t-vcore.h).
 *
 * The claimed pad's input reports are translated here (ds4t-translate.c) and
 * handed to the frontend; what the host sends back (rumble, LED) is relayed to
 * the pad, and feature reports are answered from the tables below, with the
 * pad's own IMU calibration when there is one, or from a cache of what a USB
 * pad answers itself.
 */

#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/hid.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/unaligned.h>
#include <linux/workqueue.h>

#include "ds4t-descriptors.h"
#include "ds4t-internal.h"
#include "ds4t-translate.h"
#include "ds4t-vcore.h"

/*
 * Over Bluetooth the pad's own link cannot take output at input-report rate:
 * bluetoothd drops the whole HID session when its socket fills up. So output
 * is sent at most once per interval there, always the newest state.
 */
#define BT_OUTPUT_INTERVAL	msecs_to_jiffies(25)

static struct ds4t_virtual __rcu *ds4t_virt;
static DEFINE_MUTEX(ds4t_virt_mutex);	/* create/destroy */

/* Default IMU calibration when the followed pad has none (feature 0x02). */
static const u8 ds4_default_calibration[DS4T_CALIBRATION_SIZE] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x04, 0x00, 0xfc, 0x00, 0x04,
	0x00, 0xfc, 0x00, 0x04, 0x00, 0xfc,
	0x00, 0x04, 0x00, 0x04, 0x00, 0x20,
	0x00, 0xe0, 0x00, 0x20, 0x00, 0xe0,
	0x00, 0x20, 0x00, 0xe0, 0x00, 0x00,
};

/* Host Bluetooth identity that follows the MAC in the pairing report. */
static const u8 ds4_pairing_host[9] = {
	0x08, 0x25, 0x00, 0x1e, 0x00, 0xee, 0x74, 0xd0, 0xbc
};

static const u8 dualsense_calibration[41] = {
	0x05,
	0xff, 0xfc, 0xff, 0xfe, 0xff, 0x83, 0x22, 0x78,
	0xdd, 0x92, 0x22, 0x5f, 0xdd, 0x95, 0x22, 0x6d,
	0xdd, 0x1c, 0x02, 0x1c, 0x02, 0xf2, 0x1f, 0xed,
	0xdf, 0xe3, 0x20, 0xda, 0xe0, 0xee, 0x1f, 0xdf,
	0xdf, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const u8 dualsense_firmware[64] = {
	0x20,
	0x4a, 0x75, 0x6e, 0x20, 0x31, 0x39, 0x20, 0x32, 0x30, 0x32, 0x33,
	0x31, 0x34, 0x3a, 0x34, 0x37, 0x3a, 0x33, 0x34,
	0x03, 0x00, 0x44, 0x00, 0x08, 0x02, 0x00, 0x01,
	0x36, 0x00, 0x00, 0x01, 0xc1, 0xc8, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x54, 0x01, 0x00, 0x00, 0x14, 0x00,
	0x00, 0x00, 0x0b, 0x00, 0x01, 0x00, 0x06, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

/* ---------------------------------------------------------------- features */

static void ds4_calibration_report(struct ds4t_virtual *v, u8 reportnum,
				   u8 *buf)
{
	u8 cal[DS4T_CALIBRATION_SIZE];
	struct ds4t_pad *pad;
	unsigned long flags;

	memcpy(cal, ds4_default_calibration, sizeof(cal));
	spin_lock_irqsave(&v->lock, flags);
	pad = v->src;
	if (pad && pad->calib_len)
		ds4t_calibration_to_usb_layout(pad->calib, pad->calib_len, cal);
	spin_unlock_irqrestore(&v->lock, flags);

	buf[0] = reportnum;
	memcpy(buf + 1, cal, sizeof(cal));
}

/*
 * Fills buf with a feature report the core builds; returns its size, -ENOENT
 * for a DS4 one left to the pad (ds4t_virt_cached_feature()), or -EPIPE for
 * one a DualSense does not have. len is the room.
 */
static int ds4t_virt_feature(struct ds4t_virtual *v, u8 reportnum, u8 *buf,
			     size_t len)
{
	u8 report[64] = { };
	size_t size;

	if (v->type == DS4T_VIRTUAL_DS4) {
		switch (reportnum) {
		case 0x02:
		case 0x25:
			size = 1 + DS4T_CALIBRATION_SIZE;
			ds4_calibration_report(v, reportnum, report);
			break;
		case 0x10:
		case 0x12:
			size = 16;
			report[0] = reportnum;
			memcpy(report + 1, v->mac_le, sizeof(v->mac_le));
			memcpy(report + 7, ds4_pairing_host,
			       sizeof(ds4_pairing_host));
			break;
		case 0x31:
		case 0xa3:
			/*
			 * Build dates, then hardware and firmware versions. Some
			 * titles reject an all-zero version as not a real pad.
			 */
			size = 49;
			report[0] = reportnum;
			memcpy(report + 1, "Mar 25 2016", 11);
			memcpy(report + 17, "12:00:00", 8);
			report[34] = 0x01;
			report[35] = 0x38;
			report[36] = 0x54;
			report[37] = 0x01;
			report[41] = 0x33;
			report[42] = 0x20;
			break;
		default:
			return -ENOENT;	/* asked of the pad itself */
		}
	} else {
		switch (reportnum) {
		case 0x05:
			size = sizeof(dualsense_calibration);
			memcpy(report, dualsense_calibration, size);
			break;
		case 0x20:
			size = sizeof(dualsense_firmware);
			memcpy(report, dualsense_firmware, size);
			break;
		case 0x09:
			size = 20;
			report[0] = reportnum;
			memcpy(report + 1, v->mac_le, sizeof(v->mac_le));
			report[7] = 0x08;
			report[8] = 0x25;
			report[10] = 0x1e;
			report[12] = 0xee;
			report[13] = 0x74;
			report[14] = 0xd0;
			report[15] = 0xbc;
			break;
		default:
			return -EPIPE;
		}
	}

	size = min(size, len);
	memcpy(buf, report, size);
	return size;
}

/* Feature 0x81: the pad's MAC (USB only), replaced like the one in 0x12. */
#define DS4_FEATURE_MAC		0x81

/*
 * The feature reports a real USB DualShock 4 declares beyond those the core
 * builds itself. Sony's own pad library (Unreal's WinDualShock, for one) reads
 * more of them than hid-playstation does, and takes a pad that fails them for
 * something else, so the emulated DS4 answers them with what the followed pad
 * does. Only a pad on USB has them in the layout a USB DS4 is asked for.
 */
static const u8 ds4_forwarded_features[] = {
	0x04, 0x08, 0x11, 0x13, 0x14, 0x15, 0x80, 0x81, 0x82, 0x83, 0x84,
	0x85, 0x86, 0x87, 0x88, 0x89, 0x90, 0x91, 0x92, 0x93, 0xa0, 0xa1,
	0xa2, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad,
	0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xf0, 0xf1, 0xf2,
};

static_assert(ARRAY_SIZE(ds4_forwarded_features) <= DS4T_FCACHE_MAX);

/* The followed pad with a reference taken, or NULL. */
static struct ds4t_pad *ds4t_virt_get_src(struct ds4t_virtual *v)
{
	struct ds4t_pad *pad;
	unsigned long flags;

	spin_lock_irqsave(&v->lock, flags);
	pad = v->src;
	if (pad)
		ds4t_pad_get(pad);
	spin_unlock_irqrestore(&v->lock, flags);
	return pad;
}

/* Sleeps. Reads one feature report from pad; its size or a negative errno. */
static int ds4t_virt_read_pad_feature(struct ds4t_virtual *v,
				      struct ds4t_pad *pad, u8 reportnum,
				      u8 *buf, size_t len)
{
	int ret;

	might_sleep();
	down_read(&pad->exist_lock);
	if (!pad->exist || pad->hdev->bus != BUS_USB)
		ret = -EPIPE;
	else
		ret = hid_hw_raw_request(pad->hdev, reportnum, buf, len,
					 HID_FEATURE_REPORT, HID_REQ_GET_REPORT);
	up_read(&pad->exist_lock);

	if (ret > 0 && reportnum == DS4_FEATURE_MAC)
		memcpy(buf + 1, v->mac_le,
		       min_t(size_t, sizeof(v->mac_le), ret - 1));
	return ret;
}

/*
 * Fills the cache from the followed pad. Runs on the system workqueue, so a
 * frontend answering a request in interrupt context never has to wait for the
 * pad; a pad that changed meanwhile gets a fresh run of its own.
 */
static void ds4t_virt_fcache_workfn(struct work_struct *work)
{
	struct ds4t_virtual *v = container_of(work, struct ds4t_virtual,
					      fcache_work);
	struct ds4t_fcache *cache, *old = NULL;
	struct ds4t_pad *pad;
	unsigned long flags;
	unsigned int i;
	int ret;

	pad = ds4t_virt_get_src(v);
	if (!pad)
		return;

	cache = kzalloc(sizeof(*cache), GFP_KERNEL);
	if (!cache)
		goto out;
	for (i = 0; i < ARRAY_SIZE(ds4_forwarded_features); i++) {
		u8 *data = cache->e[cache->count].data;

		data[0] = ds4_forwarded_features[i];
		ret = ds4t_virt_read_pad_feature(v, pad, data[0], data,
						 DS4T_FEATURE_MAX);
		if (ret <= 0)
			continue;
		cache->e[cache->count].id = data[0];
		cache->e[cache->count].len = ret;
		cache->count++;
	}

	spin_lock_irqsave(&v->lock, flags);
	if (v->src == pad) {
		old = v->fcache;
		v->fcache = cache;
		cache = NULL;
	}
	spin_unlock_irqrestore(&v->lock, flags);
	kfree(old);
	kfree(cache);
out:
	ds4t_pad_put(pad);
}

/*
 * The followed pad changed (v->lock held): the old cache is no longer its
 * answers. Returns the cache for the caller to free once the lock is dropped.
 */
static struct ds4t_fcache *ds4t_virt_fcache_reset(struct ds4t_virtual *v)
{
	struct ds4t_fcache *old = v->fcache;

	lockdep_assert_held(&v->lock);
	v->fcache = NULL;
	if (v->type == DS4T_VIRTUAL_DS4 && v->src &&
	    v->src->hdev->bus == BUS_USB)
		schedule_work(&v->fcache_work);
	return old;
}

/* A forwarded report from the cache; its size, or -EPIPE when not there. */
static int ds4t_virt_cached_feature(struct ds4t_virtual *v, u8 reportnum,
				    u8 *buf, size_t len)
{
	const struct ds4t_fcache *cache;
	unsigned long flags;
	unsigned int i;
	int ret = -EPIPE;

	spin_lock_irqsave(&v->lock, flags);
	cache = v->fcache;
	for (i = 0; cache && i < cache->count; i++) {
		if (cache->e[i].id != reportnum)
			continue;
		ret = min_t(size_t, cache->e[i].len, len);
		memcpy(buf, cache->e[i].data, ret);
		break;
	}
	spin_unlock_irqrestore(&v->lock, flags);
	return ret;
}

int ds4t_vcore_get_feature(struct ds4t_virtual *v, u8 reportnum, u8 *buf,
			   size_t len)
{
	int ret = ds4t_virt_feature(v, reportnum, buf, len);

	if (ret != -ENOENT)
		return ret;
	return ds4t_virt_cached_feature(v, reportnum, buf, len);
}

int ds4t_vcore_get_feature_sync(struct ds4t_virtual *v, u8 reportnum, u8 *buf,
				size_t len)
{
	struct ds4t_pad *pad;
	int ret;

	might_sleep();
	ret = ds4t_vcore_get_feature(v, reportnum, buf, len);
	if (ret != -EPIPE || v->type != DS4T_VIRTUAL_DS4)
		return ret;

	/* Not read into the cache yet: ask the pad itself. */
	pad = ds4t_virt_get_src(v);
	if (!pad)
		return -EPIPE;
	ret = ds4t_virt_read_pad_feature(v, pad, reportnum, buf, len);
	ds4t_pad_put(pad);
	return ret;
}

/* ------------------------------------------------------------------ output */

/*
 * The LED colour hid-playstation pushes right after probe is its own default
 * (a player-slot colour), not something a game asked for: drop that one.
 */
static bool ds4t_virt_take_led(struct ds4t_virtual *v)
{
	if (!v->skip_next_led)
		return true;
	v->skip_next_led = false;
	return false;
}

/*
 * Updates the rumble and LED state from an output report hid-playstation
 * built. Returns whether anything the pad shows changed.
 */
static bool ds4t_virt_parse_output(struct ds4t_virtual *v, const u8 *data,
				   size_t len)
{
	struct ds4t_output req = {
		.motor_left = v->out.motor_left,
		.motor_right = v->out.motor_right,
		.red = v->out.red,
		.green = v->out.green,
		.blue = v->out.blue,
	};
	struct ds4t_out_state next = v->out;
	unsigned int found;

	found = ds4t_parse_output(v->type == DS4T_VIRTUAL_DUALSENSE, data, len,
				  &req);
	if (found & DS4T_OUT_RUMBLE) {
		next.motor_left = req.motor_left;
		next.motor_right = req.motor_right;
	}
	if ((found & DS4T_OUT_LED) && ds4t_virt_take_led(v)) {
		next.red = req.red;
		next.green = req.green;
		next.blue = req.blue;
	}

	if (!memcmp(&next, &v->out, sizeof(next)))
		return false;
	v->out = next;
	return true;
}

/* Builds the pad's own output report from the current state. */
static int ds4t_virt_build_pad_output(struct hid_device *pad_hdev,
				      const struct ds4t_out_state *out, u8 *buf)
{
	if (pad_hdev->bus == BUS_BLUETOOTH) {
		u32 crc;

		memset(buf, 0, 78);
		buf[0] = 0x11;
		buf[1] = 0xC0;		/* hw control: HID | CRC32 */
		buf[3] = 0x07;		/* valid: motor | LED | LED blink */
		buf[6] = out->motor_right;
		buf[7] = out->motor_left;
		buf[8] = out->red;
		buf[9] = out->green;
		buf[10] = out->blue;
		crc = ds4t_crc32(0xA2, buf, 74);
		put_unaligned_le32(crc, buf + 74);
		return 78;
	}

	memset(buf, 0, 32);
	buf[0] = 0x05;
	buf[1] = 0x07;			/* valid: motor | LED | LED blink */
	buf[4] = out->motor_right;
	buf[5] = out->motor_left;
	buf[6] = out->red;
	buf[7] = out->green;
	buf[8] = out->blue;
	return 32;
}

static void ds4t_virt_output_workfn(struct work_struct *work)
{
	struct ds4t_virtual *v = container_of(to_delayed_work(work),
					      struct ds4t_virtual, output_work);
	struct ds4t_pad *pad;
	struct ds4t_out_state out;
	u8 buf[DS4T_MAX_REPORT];
	int len;

	spin_lock_irq(&v->lock);
	pad = v->src;
	if (pad)
		ds4t_pad_get(pad);
	out = v->out;
	v->last_sent = jiffies;
	spin_unlock_irq(&v->lock);
	if (!pad)
		return;

	down_read(&pad->exist_lock);
	if (pad->exist) {
		len = ds4t_virt_build_pad_output(pad->hdev, &out, buf);
		hid_hw_output_report(pad->hdev, buf, len);
	}
	up_read(&pad->exist_lock);
	ds4t_pad_put(pad);
}

/* Schedules the trip to the pad; v->lock held. */
static void ds4t_virt_queue_output(struct ds4t_virtual *v)
{
	unsigned long delay = 0;
	struct ds4t_pad *pad = v->src;

	lockdep_assert_held(&v->lock);
	if (pad && pad->hdev->bus == BUS_BLUETOOTH &&
	    time_before(jiffies, v->last_sent + BT_OUTPUT_INTERVAL))
		delay = v->last_sent + BT_OUTPUT_INTERVAL - jiffies;

	/* A pending run already sends the newest state. */
	if (!delayed_work_pending(&v->output_work))
		schedule_delayed_work(&v->output_work, delay);
}

void ds4t_vcore_output(struct ds4t_virtual *v, const u8 *data, size_t len)
{
	unsigned long flags;

	spin_lock_irqsave(&v->lock, flags);
	if (ds4t_virt_parse_output(v, data, len))
		ds4t_virt_queue_output(v);
	spin_unlock_irqrestore(&v->lock, flags);
}

/* ------------------------------------------------------------------- input */

/* Translates and delivers one input; v->lock held with interrupts off. */
static void ds4t_virt_emit(struct ds4t_virtual *v, const struct ds4t_input *in)
{
	const bool real_battery = v->src;

	lockdep_assert_held(&v->lock);
	if (v->type == DS4T_VIRTUAL_DS4)
		ds4t_build_ds4(in, real_battery, v->inbuf);
	else
		ds4t_build_dualsense(in, v->seq++, real_battery, v->inbuf);

	v->ops->input(v, v->inbuf, DS4T_VIRT_REPORT_SIZE);
}

void ds4t_virtual_input(struct ds4t_pad *pad, const u8 *data, int size)
{
	struct ds4t_virtual *v;
	struct ds4t_input in;
	unsigned long flags;

	rcu_read_lock();
	v = rcu_dereference(ds4t_virt);
	if (v && READ_ONCE(v->live) && READ_ONCE(v->src) == pad &&
	    !ds4t_parse_input(data, size, &in)) {
		spin_lock_irqsave(&v->lock, flags);
		ds4t_virt_emit(v, &in);
		spin_unlock_irqrestore(&v->lock, flags);
	}
	rcu_read_unlock();
}

/* Sticks centred, nothing pressed, no finger down. */
static void ds4t_virt_neutral(struct ds4t_input *in)
{
	memset(in, 0, sizeof(*in));
	in->common[0] = 128;
	in->common[1] = 128;
	in->common[2] = 128;
	in->common[3] = 128;
	in->common[4] = 0x08;	/* d-pad released */
}

/*
 * Follows pad instead (NULL: none; its reference moves to v) and returns the
 * pad followed before, whose reference the caller drops. With expect set,
 * only when that is the pad followed now (else NULL, nothing done). Without a new pad
 * the game sees neutral input: nothing is held down on a pad that is gone.
 * Straight from one pad to another (a Bluetooth/USB switch) it does not: the
 * next report comes from the new pad, with no neutral input in between for a
 * game to see as every button let go.
 */
static struct ds4t_pad *ds4t_virt_follow(struct ds4t_virtual *v,
					 struct ds4t_pad *pad,
					 const struct ds4t_pad *expect)
{
	struct ds4t_fcache *old_cache;
	struct ds4t_pad *old;
	struct ds4t_input in;

	spin_lock_irq(&v->lock);
	old = v->src;
	if ((expect && old != expect) || (!pad && !old)) {
		spin_unlock_irq(&v->lock);
		return NULL;
	}
	WRITE_ONCE(v->src, pad);
	if (!pad && old && v->live) {
		ds4t_virt_neutral(&in);
		ds4t_virt_emit(v, &in);
	}
	old_cache = old != pad ? ds4t_virt_fcache_reset(v) : NULL;
	spin_unlock_irq(&v->lock);
	kfree(old_cache);
	return old;
}

void ds4t_virtual_pad_gone(struct ds4t_pad *pad)
{
	struct ds4t_virtual *v;
	struct ds4t_pad *old = NULL;

	/* Not under ds4t_virt_mutex: destroy may be waiting for this pad. */
	rcu_read_lock();
	v = rcu_dereference(ds4t_virt);
	if (v)
		old = ds4t_virt_follow(v, NULL, pad);
	rcu_read_unlock();

	if (old)
		ds4t_pad_put(old);
}

int ds4t_virtual_link(const char *hid_id)
{
	struct ds4t_virtual *v;
	struct ds4t_pad *pad = NULL, *old;
	int ret = 0;

	mutex_lock(&ds4t_virt_mutex);
	v = rcu_dereference_protected(ds4t_virt,
				      lockdep_is_held(&ds4t_virt_mutex));
	if (!v) {
		ret = -ENODEV;
		goto out;
	}
	if (hid_id[0]) {
		pad = ds4t_pad_find(hid_id);
		if (!pad) {
			ret = -ENODEV;
			goto out;
		}
	}

	old = ds4t_virt_follow(v, pad, NULL);
	if (old)
		ds4t_pad_put(old);
out:
	mutex_unlock(&ds4t_virt_mutex);
	return ret;
}

int ds4t_virtual_inject(const u8 *report, size_t len)
{
	struct ds4t_virtual *v;
	struct ds4t_input in;
	unsigned long flags;
	int ret = -ENODEV;

	if (ds4t_parse_input(report, len, &in))
		return -EINVAL;

	rcu_read_lock();
	v = rcu_dereference(ds4t_virt);
	if (v && READ_ONCE(v->live)) {
		spin_lock_irqsave(&v->lock, flags);
		ds4t_virt_emit(v, &in);
		spin_unlock_irqrestore(&v->lock, flags);
		ret = 0;
	}
	rcu_read_unlock();
	return ret;
}

/* ------------------------------------------------------ create and destroy */

/* "AA:BB:CC:DD:EE:FF" to its six bytes; false for anything else. */
static bool ds4t_parse_mac(const char *str, u8 mac[6])
{
	int n = 0;

	return sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx%n", &mac[0], &mac[1],
		      &mac[2], &mac[3], &mac[4], &mac[5], &n) == 6 &&
	       n == 17 && !str[n];
}

static struct ds4t_virtual *ds4t_virt_alloc(const struct ds4t_virtual_cfg *cfg)
{
	struct ds4t_virtual *v;
	u8 mac[6];
	int i;

	if (!ds4t_parse_mac(cfg->mac, mac))
		return ERR_PTR(-EINVAL);

	v = kzalloc(sizeof(*v), GFP_KERNEL);
	if (!v)
		return ERR_PTR(-ENOMEM);

	v->type = cfg->type;
	for (i = 0; i < 6; i++)
		v->mac_le[i] = mac[5 - i];
	v->out.blue = 255;
	v->skip_next_led = true;
	spin_lock_init(&v->lock);
	INIT_DELAYED_WORK(&v->output_work, ds4t_virt_output_workfn);
	INIT_WORK(&v->fcache_work, ds4t_virt_fcache_workfn);
	return v;
}

/*
 * Once nothing reaches v any more (unpublished, frontend stopped): waits for
 * the workers, drops the followed pad and frees it all.
 */
static void ds4t_virt_free(struct ds4t_virtual *v)
{
	struct ds4t_pad *pad;

	/* Neither queues the other, and nothing else can queue them now. */
	cancel_work_sync(&v->fcache_work);
	cancel_delayed_work_sync(&v->output_work);

	spin_lock_irq(&v->lock);
	pad = v->src;
	v->src = NULL;
	spin_unlock_irq(&v->lock);
	if (pad)
		ds4t_pad_put(pad);
	kfree(v->fcache);
	kfree(v);
}

int ds4t_virtual_create(const struct ds4t_virtual_cfg *cfg)
{
	const struct ds4t_vfront_ops *ops = &ds4t_vhid_ops;
	struct ds4t_virtual *v;
	struct ds4t_pad *pad = NULL;
	int ret;

	might_sleep();
	if (cfg->type != DS4T_VIRTUAL_DS4 &&
	    cfg->type != DS4T_VIRTUAL_DUALSENSE)
		return -EINVAL;

	mutex_lock(&ds4t_virt_mutex);
	if (rcu_access_pointer(ds4t_virt)) {
		ret = -EEXIST;
		goto out;
	}

	if (cfg->hid_id[0]) {
		pad = ds4t_pad_find(cfg->hid_id);
		if (!pad) {
			ret = -ENODEV;
			goto out;
		}
	}

	v = ds4t_virt_alloc(cfg);
	if (IS_ERR(v)) {
		ret = PTR_ERR(v);
		if (pad)
			ds4t_pad_put(pad);
		goto out;
	}
	v->ops = ops;
	/* The reference moves to the virtual pad; the cache is read meanwhile. */
	spin_lock_irq(&v->lock);
	v->src = pad;
	ds4t_virt_fcache_reset(v);
	spin_unlock_irq(&v->lock);

	/* Published first: a frontend's probe may already ask for features. */
	rcu_assign_pointer(ds4t_virt, v);
	ret = ops->start(v, cfg);
	if (ret) {
		RCU_INIT_POINTER(ds4t_virt, NULL);
		synchronize_rcu();
		ds4t_virt_free(v);
		goto out;
	}
	WRITE_ONCE(v->live, true);
out:
	mutex_unlock(&ds4t_virt_mutex);
	return ret;
}

void ds4t_virtual_destroy(void)
{
	struct ds4t_virtual *v;

	might_sleep();
	mutex_lock(&ds4t_virt_mutex);
	v = rcu_dereference_protected(ds4t_virt,
				      lockdep_is_held(&ds4t_virt_mutex));
	if (v) {
		WRITE_ONCE(v->live, false);
		RCU_INIT_POINTER(ds4t_virt, NULL);
		synchronize_rcu();	/* no input path is inside it any more */
		v->ops->stop(v);	/* the host lets go of it */
		ds4t_virt_free(v);
	}
	mutex_unlock(&ds4t_virt_mutex);
}
