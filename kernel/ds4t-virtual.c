// SPDX-License-Identifier: GPL-2.0-only
/*
 * The virtual controller of the "kernel" emulation backend.
 *
 * A hid_device with its own low-level driver, like uhid's, that the regular
 * hid-playstation driver binds as if it were a real DualShock 4 or DualSense.
 * The claimed pad's input reports are translated here (ds4t-translate.c) and
 * handed to it; what hid-playstation sends back (rumble, LED) is relayed to
 * the pad, and the feature reports it asks for at probe are answered from the
 * tables below, with the pad's own IMU calibration when there is one.
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

#define VIRT_VENDOR		0x054c
#define VIRT_PRODUCT_DS4	0x05c4
#define VIRT_PRODUCT_DUALSENSE	0x0ce6
#define VIRT_VERSION		0x8111

/*
 * Over Bluetooth the pad's own link cannot take output at input-report rate:
 * bluetoothd drops the whole HID session when its socket fills up. So output
 * is sent at most once per interval there, always the newest state.
 */
#define BT_OUTPUT_INTERVAL	msecs_to_jiffies(25)

/* What the pad is told to do: rumble and the light bar. */
struct ds4t_out_state {
	u8 motor_left, motor_right;
	u8 red, green, blue;
};

struct ds4t_virtual {
	struct hid_device *hdev;
	struct hid_ll_driver ll;
	u32 type;
	u8 mac_le[6];		/* the pad's address, byte-reversed as on the wire */

	/* Set once hid_add_device() has returned; input is dropped before. */
	bool live;

	spinlock_t lock;	/* everything below */
	struct ds4t_pad *src;	/* followed pad, holds a reference; or NULL */
	u8 seq;			/* DualSense report counter */
	u8 inbuf[DS4T_VIRT_REPORT_SIZE];

	struct ds4t_out_state out;
	/* hid-playstation pushes a default LED colour at probe; drop that one. */
	bool skip_next_led;
	unsigned long last_sent;
	struct delayed_work output_work;
};

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

	memcpy(cal, ds4_default_calibration, sizeof(cal));
	spin_lock_irq(&v->lock);
	pad = v->src;
	if (pad && pad->calib_len)
		ds4t_calibration_to_usb_layout(pad->calib, pad->calib_len, cal);
	spin_unlock_irq(&v->lock);

	buf[0] = reportnum;
	memcpy(buf + 1, cal, sizeof(cal));
}

/*
 * Fills buf with the feature report hid-playstation asks for; returns its
 * size, or -EINVAL for one this pad type does not have. len is the room.
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
			return -EINVAL;
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
			return -EINVAL;
		}
	}

	size = min(size, len);
	memcpy(buf, report, size);
	return size;
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
	struct ds4t_out_state next = v->out;

	if (!len)
		return false;

	if (v->type == DS4T_VIRTUAL_DS4) {
		if (data[0] != 0x05 || len < 9)
			return false;
		if (data[1] & 0x01) {
			next.motor_right = data[4];
			next.motor_left = data[5];
		}
		if ((data[1] & 0x02) && ds4t_virt_take_led(v)) {
			next.red = data[6];
			next.green = data[7];
			next.blue = data[8];
		}
	} else {
		if (data[0] != 0x02 || len < 48)
			return false;
		if (data[1] & 0x01) {
			next.motor_right = data[3];
			next.motor_left = data[4];
			/*
			 * With haptics also selected, running both motors lets
			 * the big one drown out the light one.
			 */
			if ((data[1] & 0x02) && next.motor_right &&
			    next.motor_left)
				next.motor_left = 0;
		}
		if ((data[2] & 0x04) && ds4t_virt_take_led(v)) {
			next.red = data[45];
			next.green = data[46];
			next.blue = data[47];
		}
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

static void ds4t_virt_queue_output(struct ds4t_virtual *v)
{
	unsigned long delay = 0;
	struct ds4t_pad *pad;

	spin_lock_irq(&v->lock);
	pad = v->src;
	if (pad && pad->hdev->bus == BUS_BLUETOOTH &&
	    time_before(jiffies, v->last_sent + BT_OUTPUT_INTERVAL))
		delay = v->last_sent + BT_OUTPUT_INTERVAL - jiffies;
	spin_unlock_irq(&v->lock);

	/* A pending run already sends the newest state. */
	if (!delayed_work_pending(&v->output_work))
		schedule_delayed_work(&v->output_work, delay);
}

/* ------------------------------------------------------- hid_ll_driver ops */

static struct ds4t_virtual *ds4t_virt_of(struct hid_device *hdev)
{
	return container_of(hdev->ll_driver, struct ds4t_virtual, ll);
}

static int ds4t_virt_ll_parse(struct hid_device *hdev)
{
	struct ds4t_virtual *v = ds4t_virt_of(hdev);

	if (v->type == DS4T_VIRTUAL_DS4)
		return hid_parse_report(hdev, (__u8 *)ds4_usb_rdesc,
					sizeof(ds4_usb_rdesc));
	return hid_parse_report(hdev, (__u8 *)dualsense_usb_rdesc,
				sizeof(dualsense_usb_rdesc));
}

static int ds4t_virt_ll_start(struct hid_device *hdev)
{
	return 0;
}

static void ds4t_virt_ll_stop(struct hid_device *hdev)
{
}

static int ds4t_virt_ll_open(struct hid_device *hdev)
{
	return 0;
}

static void ds4t_virt_ll_close(struct hid_device *hdev)
{
}

static int ds4t_virt_ll_raw_request(struct hid_device *hdev,
				    unsigned char reportnum, __u8 *buf,
				    size_t len, unsigned char rtype, int reqtype)
{
	struct ds4t_virtual *v = ds4t_virt_of(hdev);

	if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_GET_REPORT)
		return ds4t_virt_feature(v, reportnum, buf, len);
	/* Everything else is accepted and ignored, as the daemon's uhid does. */
	if (reqtype == HID_REQ_SET_REPORT)
		return len;
	return -EINVAL;
}

static int ds4t_virt_ll_output_report(struct hid_device *hdev, __u8 *buf,
				      size_t len)
{
	struct ds4t_virtual *v = ds4t_virt_of(hdev);
	bool changed;

	spin_lock_irq(&v->lock);
	changed = ds4t_virt_parse_output(v, buf, len);
	spin_unlock_irq(&v->lock);

	if (changed)
		ds4t_virt_queue_output(v);
	return len;
}

static const struct hid_ll_driver ds4t_virt_ll_template = {
	.parse = ds4t_virt_ll_parse,
	.start = ds4t_virt_ll_start,
	.stop = ds4t_virt_ll_stop,
	.open = ds4t_virt_ll_open,
	.close = ds4t_virt_ll_close,
	.raw_request = ds4t_virt_ll_raw_request,
	.output_report = ds4t_virt_ll_output_report,
};

bool ds4t_virtual_owns(struct hid_device *hdev)
{
	return hdev->ll_driver && hdev->ll_driver->parse == ds4t_virt_ll_parse;
}

/* ------------------------------------------------------------------- input */

/* Translates and delivers one input; v->lock held with interrupts off. */
static void ds4t_virt_emit(struct ds4t_virtual *v, const struct ds4t_input *in)
{
	const bool real_battery = v->src;

	if (v->type == DS4T_VIRTUAL_DS4)
		ds4t_build_ds4(in, real_battery, v->inbuf);
	else
		ds4t_build_dualsense(in, v->seq++, real_battery, v->inbuf);

	hid_input_report(v->hdev, HID_INPUT_REPORT, v->inbuf,
			 DS4T_VIRT_REPORT_SIZE, 1);
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

void ds4t_virtual_pad_gone(struct ds4t_pad *pad)
{
	struct ds4t_virtual *v;
	struct ds4t_input in;
	bool followed = false;

	rcu_read_lock();
	v = rcu_dereference(ds4t_virt);
	if (v) {
		spin_lock_irq(&v->lock);
		if (v->src == pad) {
			followed = true;
			WRITE_ONCE(v->src, NULL);
			/* Nothing is held down on a pad that is no longer there. */
			if (v->live) {
				ds4t_virt_neutral(&in);
				ds4t_virt_emit(v, &in);
			}
		}
		spin_unlock_irq(&v->lock);
	}
	rcu_read_unlock();

	if (followed)
		ds4t_pad_put(pad);
}

/* Releases the pad v follows, if any, and shows neutral input. */
static struct ds4t_pad *ds4t_virt_unfollow(struct ds4t_virtual *v)
{
	struct ds4t_input in;
	struct ds4t_pad *old;

	spin_lock_irq(&v->lock);
	old = v->src;
	WRITE_ONCE(v->src, NULL);
	if (old && v->live) {
		ds4t_virt_neutral(&in);
		ds4t_virt_emit(v, &in);
	}
	spin_unlock_irq(&v->lock);
	return old;
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

	old = ds4t_virt_unfollow(v);
	if (old)
		ds4t_pad_put(old);
	if (pad) {
		spin_lock_irq(&v->lock);
		WRITE_ONCE(v->src, pad);	/* the reference moves to v */
		spin_unlock_irq(&v->lock);
	}
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
	v->ll = ds4t_virt_ll_template;
	for (i = 0; i < 6; i++)
		v->mac_le[i] = mac[5 - i];
	v->out.blue = 255;
	v->skip_next_led = true;
	spin_lock_init(&v->lock);
	INIT_DELAYED_WORK(&v->output_work, ds4t_virt_output_workfn);
	return v;
}

static struct hid_device *ds4t_virt_hid(struct ds4t_virtual *v,
					const struct ds4t_virtual_cfg *cfg)
{
	struct hid_device *hdev = hid_allocate_device();

	if (IS_ERR(hdev))
		return hdev;

	hdev->ll_driver = &v->ll;
	hdev->bus = BUS_USB;
	hdev->vendor = VIRT_VENDOR;
	hdev->product = cfg->type == DS4T_VIRTUAL_DS4 ?
			VIRT_PRODUCT_DS4 : VIRT_PRODUCT_DUALSENSE;
	hdev->version = VIRT_VERSION;
	hdev->country = 0;
	hdev->dev.parent = ds4t_control_device();
	strscpy(hdev->name, cfg->name, sizeof(hdev->name));
	strscpy(hdev->uniq, cfg->mac, sizeof(hdev->uniq));
	return hdev;
}

int ds4t_virtual_create(const struct ds4t_virtual_cfg *cfg)
{
	struct ds4t_virtual *v;
	struct ds4t_pad *pad = NULL;
	int ret;

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
		goto err_pad;
	}
	v->src = pad;		/* the reference moves to the virtual pad */

	v->hdev = ds4t_virt_hid(v, cfg);
	if (IS_ERR(v->hdev)) {
		ret = PTR_ERR(v->hdev);
		goto err_free;
	}

	/* Published first: hid-playstation's probe reaches the ll ops. */
	rcu_assign_pointer(ds4t_virt, v);
	ret = hid_add_device(v->hdev);
	if (ret) {
		RCU_INIT_POINTER(ds4t_virt, NULL);
		synchronize_rcu();
		hid_destroy_device(v->hdev);
		cancel_delayed_work_sync(&v->output_work);
		goto err_free;
	}
	WRITE_ONCE(v->live, true);
	mutex_unlock(&ds4t_virt_mutex);
	return 0;

err_free:
	kfree(v);
err_pad:
	if (pad)
		ds4t_pad_put(pad);
out:
	mutex_unlock(&ds4t_virt_mutex);
	return ret;
}

void ds4t_virtual_destroy(void)
{
	struct ds4t_virtual *v;
	struct ds4t_pad *pad;

	mutex_lock(&ds4t_virt_mutex);
	v = rcu_dereference_protected(ds4t_virt,
				      lockdep_is_held(&ds4t_virt_mutex));
	if (!v) {
		mutex_unlock(&ds4t_virt_mutex);
		return;
	}

	WRITE_ONCE(v->live, false);
	RCU_INIT_POINTER(ds4t_virt, NULL);
	synchronize_rcu();		/* no input path is inside it any more */
	hid_destroy_device(v->hdev);	/* unbinds hid-playstation */
	/* Its last output may have queued the worker, so only now is it final. */
	cancel_delayed_work_sync(&v->output_work);

	spin_lock_irq(&v->lock);
	pad = v->src;
	v->src = NULL;
	spin_unlock_irq(&v->lock);
	if (pad)
		ds4t_pad_put(pad);
	kfree(v);
	mutex_unlock(&ds4t_virt_mutex);
}
