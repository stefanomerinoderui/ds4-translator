// SPDX-License-Identifier: GPL-2.0-only
/*
 * hid-ds4-translator - claims a physical DualShock 4 before hid-playstation
 *
 * The pad is bound with HID_CONNECT_DRIVER only, so the HID core creates no
 * hidraw, input, battery or LED device for it. Incoming reports are kept in
 * a small ring that is dumped through debugfs (prototype stage: the char
 * device that hands them to the ds4-translator daemon comes next).
 */

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/workqueue.h>

#define DS4T_NAME		"hid-ds4-translator"

/* The IDs live in drivers/hid/hid-ids.h, which is not an installed header. */
#define DS4T_VENDOR_SONY	0x054c
#define DS4T_PRODUCT_PS4_V1	0x05c4
#define DS4T_PRODUCT_PS4_V2	0x09cc

/* Same bound the daemon uses for one report (largest is 78 bytes, Bluetooth). */
#define DS4T_MAX_REPORT		128
#define DS4T_RING_SIZE		16

/* Feature reports that return the calibration data (and switch BT to 0x11). */
#define DS4T_FEATURE_CAL_USB	0x02
#define DS4T_FEATURE_CAL_USB_SZ	37
#define DS4T_FEATURE_CAL_BT	0x05
#define DS4T_FEATURE_CAL_BT_SZ	41

static char ignore_mac[18];
module_param_string(ignore_mac, ignore_mac, sizeof(ignore_mac), 0644);
MODULE_PARM_DESC(ignore_mac,
		 "MAC (hdev->uniq) of the emulated pad, never claimed");

struct ds4t_report {
	u16 len;
	u8 data[DS4T_MAX_REPORT];
};

struct ds4t_pad {
	struct hid_device *hdev;
	struct dentry *dbg_file;

	spinlock_t lock;		/* protects ring, head, count, total */
	struct ds4t_report ring[DS4T_RING_SIZE];
	unsigned int head;		/* next slot to write */
	unsigned int count;		/* valid entries, <= DS4T_RING_SIZE */
	u64 total;			/* reports seen since probe */

	u8 calib[DS4T_FEATURE_CAL_BT_SZ];
	int calib_len;			/* 0 when the read failed */
};

struct ds4t_steal_work {
	struct work_struct work;
	struct device *dev;
};

static struct hid_driver ds4t_driver;
static struct dentry *ds4t_dbg_dir;
static struct workqueue_struct *ds4t_wq;

static const struct hid_device_id ds4t_devices[] = {
	{ HID_USB_DEVICE(DS4T_VENDOR_SONY, DS4T_PRODUCT_PS4_V1) },
	{ HID_USB_DEVICE(DS4T_VENDOR_SONY, DS4T_PRODUCT_PS4_V2) },
	{ HID_BLUETOOTH_DEVICE(DS4T_VENDOR_SONY, DS4T_PRODUCT_PS4_V1) },
	{ HID_BLUETOOTH_DEVICE(DS4T_VENDOR_SONY, DS4T_PRODUCT_PS4_V2) },
	{ }
};
MODULE_DEVICE_TABLE(hid, ds4t_devices);

/*
 * The daemon's own emulated pad has the same IDs. Under the functionfs
 * backend it is a real USB device behind dummy_hcd; under uhid it looks like
 * a Bluetooth pad, so only its MAC tells it apart.
 */
static bool ds4t_is_emulated(struct hid_device *hdev)
{
	struct device *d;

	if (ignore_mac[0] && strcasecmp(hdev->uniq, ignore_mac) == 0)
		return true;

	if (hdev->bus != BUS_USB)
		return false;

	for (d = &hdev->dev; d; d = d->parent)
		if (strstarts(dev_name(d), "dummy_hcd"))
			return true;

	return false;
}

static void ds4t_ring_push(struct ds4t_pad *pad, const u8 *data, int size)
{
	struct ds4t_report *slot;
	unsigned long flags;

	spin_lock_irqsave(&pad->lock, flags);
	slot = &pad->ring[pad->head];
	slot->len = min(size, DS4T_MAX_REPORT);
	memcpy(slot->data, data, slot->len);
	pad->head = (pad->head + 1) % DS4T_RING_SIZE;
	pad->count = min(pad->count + 1, (unsigned int)DS4T_RING_SIZE);
	pad->total++;
	spin_unlock_irqrestore(&pad->lock, flags);
}

static int ds4t_ring_show(struct seq_file *m, void *unused)
{
	struct ds4t_pad *pad = m->private;
	unsigned long flags;
	unsigned int i, idx;

	spin_lock_irqsave(&pad->lock, flags);
	seq_printf(m, "total %llu\n", pad->total);
	for (i = 0; i < pad->count; i++) {
		idx = (pad->head + DS4T_RING_SIZE - pad->count + i) %
		      DS4T_RING_SIZE;
		seq_printf(m, "%u: %*phN\n", pad->ring[idx].len,
			   pad->ring[idx].len, pad->ring[idx].data);
	}
	spin_unlock_irqrestore(&pad->lock, flags);
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(ds4t_ring);

static int ds4t_read_feature(struct hid_device *hdev, u8 reportnum, u8 *buf,
			     size_t len)
{
	int ret;

	memset(buf, 0, len);
	buf[0] = reportnum;
	ret = hid_hw_raw_request(hdev, reportnum, buf, len, HID_FEATURE_REPORT,
				 HID_REQ_GET_REPORT);
	if (ret < 0)
		return ret;
	return ret == len ? 0 : -EIO;
}

/*
 * Reading the full calibration report is what makes a Bluetooth pad switch
 * from the short 0x01 report to the extended 0x11 one, as hid-playstation
 * does in its probe.
 */
static void ds4t_read_calibration(struct ds4t_pad *pad)
{
	struct hid_device *hdev = pad->hdev;
	u8 reportnum = hdev->bus == BUS_BLUETOOTH ? DS4T_FEATURE_CAL_BT
						  : DS4T_FEATURE_CAL_USB;
	size_t len = hdev->bus == BUS_BLUETOOTH ? DS4T_FEATURE_CAL_BT_SZ
						: DS4T_FEATURE_CAL_USB_SZ;
	int ret;

	ret = ds4t_read_feature(hdev, reportnum, pad->calib, len);
	if (ret) {
		hid_warn(hdev, "calibration report 0x%02x read failed: %d\n",
			 reportnum, ret);
		return;
	}
	pad->calib_len = len;
}

static int ds4t_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct ds4t_pad *pad;
	int ret;

	if (ds4t_is_emulated(hdev))
		return -ENODEV;

	pad = devm_kzalloc(&hdev->dev, sizeof(*pad), GFP_KERNEL);
	if (!pad)
		return -ENOMEM;
	pad->hdev = hdev;
	spin_lock_init(&pad->lock);
	hid_set_drvdata(hdev, pad);

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	/*
	 * HID_CONNECT_DRIVER alone: nothing is claimed for hidraw, input or
	 * force feedback, but the core still needs a claim, otherwise
	 * hid_connect() fails with "device has no listeners".
	 */
	ret = hid_hw_start(hdev, HID_CONNECT_DRIVER);
	if (ret)
		return ret;

	ret = hid_hw_open(hdev);
	if (ret)
		goto err_stop;

	ds4t_read_calibration(pad);

	pad->dbg_file = debugfs_create_file(dev_name(&hdev->dev), 0400,
					    ds4t_dbg_dir, pad, &ds4t_ring_fops);
	hid_info(hdev, "claimed %s pad %s\n",
		 hdev->bus == BUS_BLUETOOTH ? "Bluetooth" : "USB", hdev->uniq);
	return 0;

err_stop:
	hid_hw_stop(hdev);
	return ret;
}

static void ds4t_remove(struct hid_device *hdev)
{
	struct ds4t_pad *pad = hid_get_drvdata(hdev);

	debugfs_remove(pad->dbg_file);
	hid_hw_close(hdev);
	hid_hw_stop(hdev);
}

static int ds4t_raw_event(struct hid_device *hdev, struct hid_report *report,
			  u8 *data, int size)
{
	struct ds4t_pad *pad = hid_get_drvdata(hdev);

	ds4t_ring_push(pad, data, size);
	/* Nonzero tells the HID core the report is fully handled. */
	return 1;
}

static struct hid_driver ds4t_driver = {
	.name = DS4T_NAME,
	.id_table = ds4t_devices,
	.probe = ds4t_probe,
	.remove = ds4t_remove,
	.raw_event = ds4t_raw_event,
};

/*
 * Moves a pad that another driver (hid-playstation, hid-sony, ...) bound
 * first over to this driver. A driver registered earlier is always tried
 * first, so when it loses the load-order race the pad has to be taken over.
 * If our probe refuses it, hand it back so it is never left driverless.
 */
static void ds4t_steal(struct device *dev)
{
	struct hid_device *hdev = to_hid_device(dev);

	if (!dev->driver || dev->driver == &ds4t_driver.driver)
		return;
	if (!hid_match_id(hdev, ds4t_devices) || ds4t_is_emulated(hdev))
		return;

	hid_info(hdev, "taking over from %s\n", dev->driver->name);
	device_release_driver(dev);
	if (device_driver_attach(&ds4t_driver.driver, dev) &&
	    device_attach(dev) < 0)
		hid_warn(hdev, "could not rebind after takeover failed\n");
}

static int ds4t_steal_cb(struct device *dev, void *unused)
{
	ds4t_steal(dev);
	return 0;
}

static void ds4t_steal_workfn(struct work_struct *work)
{
	struct ds4t_steal_work *sw =
		container_of(work, struct ds4t_steal_work, work);

	ds4t_steal(sw->dev);
	put_device(sw->dev);
	kfree(sw);
}

/*
 * Covers a pad that appears after this module loaded but is claimed by an
 * earlier-registered driver. The notifier runs with the device lock held, so
 * the takeover itself is deferred to a workqueue.
 */
static int ds4t_bus_event(struct notifier_block *nb, unsigned long action,
			  void *data)
{
	struct device *dev = data;
	struct ds4t_steal_work *sw;

	if (action != BUS_NOTIFY_BOUND_DRIVER || dev->driver == &ds4t_driver.driver)
		return NOTIFY_DONE;
	if (!hid_match_id(to_hid_device(dev), ds4t_devices))
		return NOTIFY_DONE;

	sw = kmalloc(sizeof(*sw), GFP_KERNEL);
	if (!sw)
		return NOTIFY_DONE;
	INIT_WORK(&sw->work, ds4t_steal_workfn);
	sw->dev = get_device(dev);
	queue_work(ds4t_wq, &sw->work);
	return NOTIFY_OK;
}

static struct notifier_block ds4t_nb = {
	.notifier_call = ds4t_bus_event,
};

static int __init ds4t_init(void)
{
	int ret;

	ds4t_wq = alloc_ordered_workqueue(DS4T_NAME, 0);
	if (!ds4t_wq)
		return -ENOMEM;

	ds4t_dbg_dir = debugfs_create_dir("ds4t", NULL);

	ret = hid_register_driver(&ds4t_driver);
	if (ret)
		goto err_dbg;

	ret = bus_register_notifier(&hid_bus_type, &ds4t_nb);
	if (ret)
		goto err_unregister;

	bus_for_each_dev(&hid_bus_type, NULL, NULL, ds4t_steal_cb);
	return 0;

err_unregister:
	hid_unregister_driver(&ds4t_driver);
err_dbg:
	debugfs_remove_recursive(ds4t_dbg_dir);
	destroy_workqueue(ds4t_wq);
	return ret;
}

static void __exit ds4t_exit(void)
{
	bus_unregister_notifier(&hid_bus_type, &ds4t_nb);
	destroy_workqueue(ds4t_wq);	/* drains pending takeovers */
	hid_unregister_driver(&ds4t_driver);
	/* Unbinding does not reprobe, so give every freed pad back. */
	if (bus_rescan_devices(&hid_bus_type))
		pr_warn(DS4T_NAME ": rescan after unload failed\n");
	debugfs_remove_recursive(ds4t_dbg_dir);
}

module_init(ds4t_init);
module_exit(ds4t_exit);

MODULE_DESCRIPTION("Claims a physical DualShock 4 for the ds4-translator daemon");
MODULE_LICENSE("GPL");
