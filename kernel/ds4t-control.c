// SPDX-License-Identifier: GPL-2.0-only
/*
 * /dev/ds4-translator/control: the daemon's attachment point.
 *
 * Issuing DS4T_IOC_CTL_ATTACH on it switches the module on (it takes over the
 * pads other drivers hold); closing it, however that happens, switches it off
 * and hands the pads back, so a killed daemon never leaves a controller dead.
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "ds4t-internal.h"

static atomic_t ds4t_control_open_count = ATOMIC_INIT(0);

static DEFINE_SPINLOCK(ds4t_mac_lock);
static char ds4t_ignore_mac[DS4T_MAC_LEN];

bool ds4t_mac_ignored(const char *uniq)
{
	bool match;

	spin_lock(&ds4t_mac_lock);
	match = ds4t_ignore_mac[0] && !strcasecmp(uniq, ds4t_ignore_mac);
	spin_unlock(&ds4t_mac_lock);
	return match;
}

static int ds4t_control_open(struct inode *inode, struct file *file)
{
	/* A single daemon at a time: two would fight over the ignore MAC. */
	if (atomic_cmpxchg(&ds4t_control_open_count, 0, 1))
		return -EBUSY;

	return stream_open(inode, file);
}

static int ds4t_control_release(struct inode *inode, struct file *file)
{
	/* Before the pads are handed back: it holds references to them. */
	ds4t_virtual_destroy();

	if (ds4t_attached())
		ds4t_detach();

	spin_lock(&ds4t_mac_lock);
	ds4t_ignore_mac[0] = '\0';
	spin_unlock(&ds4t_mac_lock);

	atomic_set(&ds4t_control_open_count, 0);
	return 0;
}

static long ds4t_control_set_ignore_mac(const void __user *arg)
{
	struct ds4t_mac mac;

	if (copy_from_user(&mac, arg, sizeof(mac)))
		return -EFAULT;
	if (!memchr(mac.str, '\0', sizeof(mac.str)))
		return -EINVAL;

	spin_lock(&ds4t_mac_lock);
	strscpy(ds4t_ignore_mac, mac.str, sizeof(ds4t_ignore_mac));
	spin_unlock(&ds4t_mac_lock);
	return 0;
}

static long ds4t_control_create_virtual(const void __user *arg)
{
	struct ds4t_virtual_cfg cfg;

	if (copy_from_user(&cfg, arg, sizeof(cfg)))
		return -EFAULT;
	if (!memchr(cfg.name, '\0', sizeof(cfg.name)) ||
	    !memchr(cfg.mac, '\0', sizeof(cfg.mac)) ||
	    !memchr(cfg.hid_id, '\0', sizeof(cfg.hid_id)))
		return -EINVAL;
	return ds4t_virtual_create(&cfg);
}

static long ds4t_control_inject(const void __user *arg)
{
	struct ds4t_feature report;

	if (copy_from_user(&report, arg, sizeof(report)))
		return -EFAULT;
	if (report.len > sizeof(report.data))
		return -EINVAL;
	return ds4t_virtual_inject(report.data, report.len);
}

static long ds4t_control_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	switch (cmd) {
	case DS4T_IOC_CTL_GET_ABI:
		return put_user(DS4T_ABI_VERSION, (__u32 __user *)arg);
	case DS4T_IOC_CTL_SET_IGNORE_MAC:
		return ds4t_control_set_ignore_mac((const void __user *)arg);
	case DS4T_IOC_CTL_ATTACH:
		ds4t_attach();
		return 0;
	case DS4T_IOC_CTL_CREATE_VIRTUAL:
		return ds4t_control_create_virtual((const void __user *)arg);
	case DS4T_IOC_CTL_DESTROY_VIRTUAL:
		ds4t_virtual_destroy();
		return 0;
	case DS4T_IOC_CTL_INJECT:
		return ds4t_control_inject((const void __user *)arg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations ds4t_control_fops = {
	.owner = THIS_MODULE,
	.open = ds4t_control_open,
	.release = ds4t_control_release,
	.unlocked_ioctl = ds4t_control_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};

static struct miscdevice ds4t_control_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ds4t-control",
	.nodename = "ds4-translator/control",
	.mode = 0600,
	.fops = &ds4t_control_fops,
};

struct device *ds4t_control_device(void)
{
	return ds4t_control_dev.this_device;
}

int ds4t_control_init(void)
{
	return misc_register(&ds4t_control_dev);
}

void ds4t_control_exit(void)
{
	misc_deregister(&ds4t_control_dev);
}
