// SPDX-License-Identifier: GPL-2.0-only
/*
 * The "kernel" backend's frontend: the virtual controller as a hid_device with
 * its own low-level driver, like uhid's, that the regular hid-playstation
 * driver binds as if it were a real DualShock 4 or DualSense. It has no USB
 * device behind it, so whatever reads one (Wine, for its version and strings)
 * finds none; the "kernel-usb" backend's gadget has one.
 */

#include <linux/hid.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "ds4t-descriptors.h"
#include "ds4t-internal.h"
#include "ds4t-vcore.h"

#define VIRT_VENDOR		0x054c
#define VIRT_PRODUCT_DS4	0x05c4
#define VIRT_PRODUCT_DUALSENSE	0x0ce6
#define VIRT_VERSION		0x8111

struct ds4t_vhid {
	struct ds4t_virtual *v;
	struct hid_device *hdev;
	struct hid_ll_driver ll;
};

static struct ds4t_virtual *ds4t_vhid_core(struct hid_device *hdev)
{
	return container_of(hdev->ll_driver, struct ds4t_vhid, ll)->v;
}

static int ds4t_vhid_parse(struct hid_device *hdev)
{
	struct ds4t_virtual *v = ds4t_vhid_core(hdev);

	if (v->type == DS4T_VIRTUAL_DS4)
		return hid_parse_report(hdev, (__u8 *)ds4_usb_rdesc,
					sizeof(ds4_usb_rdesc));
	return hid_parse_report(hdev, (__u8 *)dualsense_usb_rdesc,
				sizeof(dualsense_usb_rdesc));
}

static int ds4t_vhid_start(struct hid_device *hdev)
{
	return 0;
}

static void ds4t_vhid_stop(struct hid_device *hdev)
{
}

static int ds4t_vhid_open(struct hid_device *hdev)
{
	return 0;
}

static void ds4t_vhid_close(struct hid_device *hdev)
{
}

static int ds4t_vhid_output_report(struct hid_device *hdev, __u8 *buf,
				   size_t len)
{
	ds4t_vcore_output(ds4t_vhid_core(hdev), buf, len);
	return len;
}

static int ds4t_vhid_raw_request(struct hid_device *hdev,
				 unsigned char reportnum, __u8 *buf,
				 size_t len, unsigned char rtype, int reqtype)
{
	struct ds4t_virtual *v = ds4t_vhid_core(hdev);

	/* hidraw and hid-playstation ask from process context: it may sleep. */
	if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_GET_REPORT)
		return ds4t_vcore_get_feature_sync(v, reportnum, buf, len);
	/* An output report sent as SET_REPORT is still rumble and LED. */
	if (rtype == HID_OUTPUT_REPORT && reqtype == HID_REQ_SET_REPORT)
		return ds4t_vhid_output_report(hdev, buf, len);
	/* Everything else is accepted and ignored, as the daemon's uhid does. */
	if (reqtype == HID_REQ_SET_REPORT)
		return len;
	return -EINVAL;
}

static const struct hid_ll_driver ds4t_vhid_ll_template = {
	.parse = ds4t_vhid_parse,
	.start = ds4t_vhid_start,
	.stop = ds4t_vhid_stop,
	.open = ds4t_vhid_open,
	.close = ds4t_vhid_close,
	.raw_request = ds4t_vhid_raw_request,
	.output_report = ds4t_vhid_output_report,
};

bool ds4t_virtual_owns(struct hid_device *hdev)
{
	return hdev->ll_driver && hdev->ll_driver->parse == ds4t_vhid_parse;
}

static int ds4t_vhid_front_start(struct ds4t_virtual *v,
				 const struct ds4t_virtual_cfg *cfg)
{
	struct ds4t_vhid *vh;
	struct hid_device *hdev;
	int ret;

	vh = kzalloc(sizeof(*vh), GFP_KERNEL);
	if (!vh)
		return -ENOMEM;
	vh->v = v;
	vh->ll = ds4t_vhid_ll_template;

	hdev = hid_allocate_device();
	if (IS_ERR(hdev)) {
		kfree(vh);
		return PTR_ERR(hdev);
	}
	hdev->ll_driver = &vh->ll;
	hdev->bus = BUS_USB;
	hdev->vendor = VIRT_VENDOR;
	hdev->product = cfg->type == DS4T_VIRTUAL_DS4 ?
			VIRT_PRODUCT_DS4 : VIRT_PRODUCT_DUALSENSE;
	hdev->version = VIRT_VERSION;
	hdev->country = 0;
	hdev->dev.parent = ds4t_control_device();
	strscpy(hdev->name, cfg->name, sizeof(hdev->name));
	strscpy(hdev->uniq, cfg->mac, sizeof(hdev->uniq));
	vh->hdev = hdev;

	/* Set before adding: hid-playstation's probe sends input and output. */
	v->front = vh;
	ret = hid_add_device(hdev);
	if (ret) {
		hid_destroy_device(hdev);
		v->front = NULL;
		kfree(vh);
	}
	return ret;
}

static void ds4t_vhid_front_stop(struct ds4t_virtual *v)
{
	struct ds4t_vhid *vh = v->front;

	hid_destroy_device(vh->hdev);	/* unbinds hid-playstation */
	v->front = NULL;
	kfree(vh);
}

/* Atomic, v->lock held. */
static void ds4t_vhid_front_input(struct ds4t_virtual *v, u8 *report,
				  size_t len)
{
	struct ds4t_vhid *vh = v->front;

	hid_input_report(vh->hdev, HID_INPUT_REPORT, report, len, 1);
}

const struct ds4t_vfront_ops ds4t_vhid_ops = {
	.start = ds4t_vhid_front_start,
	.stop = ds4t_vhid_front_stop,
	.input = ds4t_vhid_front_input,
	.owner = THIS_MODULE,
};
