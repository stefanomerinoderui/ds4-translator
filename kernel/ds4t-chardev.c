// SPDX-License-Identifier: GPL-2.0-only
/*
 * Per-pad character device (/dev/ds4-translator/<hid_id>) and its debugfs
 * report dump. See include/uapi/ds4t.h for the contract.
 */

#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "ds4t-internal.h"

static struct dentry *ds4t_dbg_dir;

/* Registered pads, for the virtual controller to find the one it follows. */
static LIST_HEAD(ds4t_pads);
static DEFINE_MUTEX(ds4t_pads_lock);

/* Ring helpers: callers hold pad->lock. */

static void ds4t_ring_push(struct ds4t_ring *ring, const u8 *data, int size)
{
	struct ds4t_report *slot = &ring->slots[ring->head];

	slot->len = min(size, DS4T_MAX_REPORT);
	memcpy(slot->data, data, slot->len);
	ring->head = (ring->head + 1) % DS4T_RING_SIZE;
	ring->count = min(ring->count + 1, (unsigned int)DS4T_RING_SIZE);
	ring->total++;
}

static const struct ds4t_report *ds4t_ring_at(const struct ds4t_ring *ring,
					      unsigned int i)
{
	return &ring->slots[(ring->head + DS4T_RING_SIZE - ring->count + i) %
			    DS4T_RING_SIZE];
}

/* Copies out the oldest report and drops it; the ring must not be empty. */
static void ds4t_ring_pop(struct ds4t_ring *ring, struct ds4t_report *out)
{
	*out = *ds4t_ring_at(ring, 0);
	ring->count--;
}

struct ds4t_pad *ds4t_pad_alloc(struct hid_device *hdev)
{
	struct ds4t_pad *pad = kzalloc(sizeof(*pad), GFP_KERNEL);

	if (!pad)
		return NULL;

	kref_init(&pad->ref);
	INIT_LIST_HEAD(&pad->list);
	pad->hdev = hdev;
	pad->exist = true;
	init_rwsem(&pad->exist_lock);
	spin_lock_init(&pad->lock);
	init_waitqueue_head(&pad->wait);

	pad->info.abi_version = DS4T_ABI_VERSION;
	pad->info.bus = hdev->bus;
	pad->info.vendor = hdev->vendor;
	pad->info.product = hdev->product;
	strscpy(pad->info.uniq, hdev->uniq, sizeof(pad->info.uniq));
	strscpy(pad->info.hid_id, dev_name(&hdev->dev),
		sizeof(pad->info.hid_id));

	snprintf(pad->devname, sizeof(pad->devname), "ds4t-%s", pad->info.hid_id);
	snprintf(pad->nodename, sizeof(pad->nodename), "ds4-translator/%s",
		 pad->info.hid_id);
	return pad;
}

static void ds4t_pad_free(struct kref *ref)
{
	kfree(container_of(ref, struct ds4t_pad, ref));
}

void ds4t_pad_get(struct ds4t_pad *pad)
{
	kref_get(&pad->ref);
}

void ds4t_pad_put(struct ds4t_pad *pad)
{
	kref_put(&pad->ref, ds4t_pad_free);
}

struct ds4t_pad *ds4t_pad_find(const char *hid_id)
{
	struct ds4t_pad *pad, *found = NULL;

	mutex_lock(&ds4t_pads_lock);
	list_for_each_entry(pad, &ds4t_pads, list) {
		if (!strcmp(pad->info.hid_id, hid_id)) {
			ds4t_pad_get(pad);
			found = pad;
			break;
		}
	}
	mutex_unlock(&ds4t_pads_lock);
	return found;
}

void ds4t_pad_report(struct ds4t_pad *pad, const u8 *data, int size)
{
	unsigned long flags;

	spin_lock_irqsave(&pad->lock, flags);
	ds4t_ring_push(&pad->dbg, data, size);
	/* Reports are state snapshots: when nobody reads, keep the newest. */
	ds4t_ring_push(&pad->queue, data, size);
	spin_unlock_irqrestore(&pad->lock, flags);
	wake_up_interruptible(&pad->wait);
}

static int ds4t_open(struct inode *inode, struct file *file)
{
	/* misc_open() stored the miscdevice here; the driver's own reference
	 * is still held, because remove() deregisters before dropping it.
	 */
	struct ds4t_pad *pad = container_of(file->private_data,
					    struct ds4t_pad, misc);
	int ret = 0;

	spin_lock_irq(&pad->lock);
	if (!pad->exist) {
		ret = -ENODEV;
	} else if (pad->opened) {
		ret = -EBUSY;
	} else {
		pad->opened = true;
		/* Anything queued before this open is stale. */
		pad->queue.count = 0;
		kref_get(&pad->ref);
	}
	spin_unlock_irq(&pad->lock);
	if (ret)
		return ret;

	file->private_data = pad;
	return stream_open(inode, file);
}

static int ds4t_release(struct inode *inode, struct file *file)
{
	struct ds4t_pad *pad = file->private_data;

	spin_lock_irq(&pad->lock);
	pad->opened = false;
	spin_unlock_irq(&pad->lock);
	ds4t_pad_put(pad);
	return 0;
}

static ssize_t ds4t_read(struct file *file, char __user *buf, size_t count,
			 loff_t *ppos)
{
	struct ds4t_pad *pad = file->private_data;
	struct ds4t_report report;
	size_t len;

	spin_lock_irq(&pad->lock);
	while (!pad->queue.count) {
		spin_unlock_irq(&pad->lock);
		if (!READ_ONCE(pad->exist))
			return -ENODEV;
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(pad->wait,
					     READ_ONCE(pad->queue.count) ||
					     !READ_ONCE(pad->exist)))
			return -ERESTARTSYS;
		spin_lock_irq(&pad->lock);
	}
	ds4t_ring_pop(&pad->queue, &report);
	spin_unlock_irq(&pad->lock);

	/* One report per read(); a short buffer truncates, like hidraw. */
	len = min_t(size_t, count, report.len);
	if (copy_to_user(buf, report.data, len))
		return -EFAULT;
	return len;
}

static ssize_t ds4t_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct ds4t_pad *pad = file->private_data;
	u8 *data;
	int ret;

	if (!count || count > DS4T_MAX_REPORT)
		return -EINVAL;

	data = memdup_user(buf, count);
	if (IS_ERR(data))
		return PTR_ERR(data);

	down_read(&pad->exist_lock);
	if (!pad->exist) {
		ret = -ENODEV;
	} else {
		ret = hid_hw_output_report(pad->hdev, data, count);
		/* Same fallback as hidraw for transports without an output
		 * endpoint.
		 */
		if (ret == -ENOSYS)
			ret = hid_hw_raw_request(pad->hdev, data[0], data, count,
						 HID_OUTPUT_REPORT,
						 HID_REQ_SET_REPORT);
	}
	up_read(&pad->exist_lock);

	kfree(data);
	return ret < 0 ? ret : count;
}

static __poll_t ds4t_poll(struct file *file, poll_table *wait)
{
	struct ds4t_pad *pad = file->private_data;
	__poll_t mask = 0;

	poll_wait(file, &pad->wait, wait);

	spin_lock_irq(&pad->lock);
	if (pad->queue.count)
		mask |= EPOLLIN | EPOLLRDNORM;
	if (pad->exist)
		mask |= EPOLLOUT | EPOLLWRNORM;
	else
		mask |= EPOLLERR | EPOLLHUP;
	spin_unlock_irq(&pad->lock);
	return mask;
}

/*
 * Feature reports go straight to the HID layer. The lower drivers bound the
 * wait (usbhid and uhid both give up after about five seconds), and remove()
 * waits for the call to return before it tears the device down.
 */
static long ds4t_feature_request(struct ds4t_pad *pad, void __user *arg,
				 enum hid_class_request reqtype)
{
	struct ds4t_feature *feat;
	long ret;

	feat = memdup_user(arg, sizeof(*feat));
	if (IS_ERR(feat))
		return PTR_ERR(feat);

	if (!feat->len || feat->len > sizeof(feat->data)) {
		ret = -EINVAL;
		goto out;
	}

	down_read(&pad->exist_lock);
	if (!pad->exist)
		ret = -ENODEV;
	else
		ret = hid_hw_raw_request(pad->hdev, feat->data[0], feat->data,
					 feat->len, HID_FEATURE_REPORT,
					 reqtype);
	up_read(&pad->exist_lock);
	if (ret < 0)
		goto out;

	if (reqtype == HID_REQ_GET_REPORT) {
		feat->len = ret;
		ret = copy_to_user(arg, feat, sizeof(*feat)) ? -EFAULT : 0;
	} else {
		ret = 0;
	}
out:
	kfree(feat);
	return ret;
}

static long ds4t_get_calibration(struct ds4t_pad *pad, void __user *arg)
{
	struct ds4t_feature feat = { .len = pad->calib_len };

	if (!pad->calib_len)
		return -ENODATA;

	memcpy(feat.data, pad->calib, pad->calib_len);
	return copy_to_user(arg, &feat, sizeof(feat)) ? -EFAULT : 0;
}

static long ds4t_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ds4t_pad *pad = file->private_data;
	void __user *uarg = (void __user *)arg;

	switch (cmd) {
	case DS4T_IOC_GET_INFO:
		return copy_to_user(uarg, &pad->info, sizeof(pad->info)) ?
		       -EFAULT : 0;
	case DS4T_IOC_GET_FEATURE:
		return ds4t_feature_request(pad, uarg, HID_REQ_GET_REPORT);
	case DS4T_IOC_SET_FEATURE:
		return ds4t_feature_request(pad, uarg, HID_REQ_SET_REPORT);
	case DS4T_IOC_GET_CALIBRATION:
		return ds4t_get_calibration(pad, uarg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations ds4t_fops = {
	.owner = THIS_MODULE,
	.open = ds4t_open,
	.release = ds4t_release,
	.read = ds4t_read,
	.write = ds4t_write,
	.poll = ds4t_poll,
	.unlocked_ioctl = ds4t_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};

static int ds4t_dbg_show(struct seq_file *m, void *unused)
{
	struct ds4t_pad *pad = m->private;
	unsigned int i;

	spin_lock_irq(&pad->lock);
	seq_printf(m, "total %llu\n", pad->dbg.total);
	for (i = 0; i < pad->dbg.count; i++) {
		const struct ds4t_report *r = ds4t_ring_at(&pad->dbg, i);

		seq_printf(m, "%u: %*phN\n", r->len, r->len, r->data);
	}
	spin_unlock_irq(&pad->lock);
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(ds4t_dbg);

int ds4t_pad_register(struct ds4t_pad *pad)
{
	int ret;

	pad->misc.minor = MISC_DYNAMIC_MINOR;
	pad->misc.name = pad->devname;
	pad->misc.nodename = pad->nodename;
	pad->misc.mode = 0600;
	pad->misc.fops = &ds4t_fops;

	ret = misc_register(&pad->misc);
	if (ret)
		return ret;

	pad->dbg_file = debugfs_create_file(pad->info.hid_id, 0400,
					    ds4t_dbg_dir, pad, &ds4t_dbg_fops);

	mutex_lock(&ds4t_pads_lock);
	list_add_tail(&pad->list, &ds4t_pads);
	mutex_unlock(&ds4t_pads_lock);
	return 0;
}

void ds4t_pad_unregister(struct ds4t_pad *pad)
{
	/*
	 * The virtual controller stops following the pad first: its output
	 * worker takes exist_lock for reading, which the write lock below must
	 * not be held against while waiting for it.
	 */
	ds4t_virtual_pad_gone(pad);

	mutex_lock(&ds4t_pads_lock);
	list_del_init(&pad->list);
	mutex_unlock(&ds4t_pads_lock);

	/* Wait for writers inside hid_hw_*(), then keep new ones out. */
	down_write(&pad->exist_lock);
	spin_lock_irq(&pad->lock);
	WRITE_ONCE(pad->exist, false);
	spin_unlock_irq(&pad->lock);
	up_write(&pad->exist_lock);

	wake_up_interruptible_all(&pad->wait);
	misc_deregister(&pad->misc);
	debugfs_remove(pad->dbg_file);
}

void ds4t_debugfs_init(void)
{
	ds4t_dbg_dir = debugfs_create_dir("ds4t", NULL);
}

void ds4t_debugfs_exit(void)
{
	debugfs_remove_recursive(ds4t_dbg_dir);
}
