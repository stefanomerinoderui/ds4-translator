// SPDX-License-Identifier: GPL-2.0-only
/*
 * The "kernel-usb" backend's frontend: the virtual controller as a USB gadget.
 *
 * On a UDC such as dummy_hcd's, the host side sees a real USB device: usbhid
 * and hid-playstation bind it as they would a DualShock 4 or DualSense plugged
 * in, and whatever reads the USB device itself (Wine hands games its version
 * and strings) finds what a real pad has. The pad's behaviour stays in
 * hid-ds4-translator's core (ds4t-vcore.c); this only carries reports.
 *
 * A plain usb_gadget_driver rather than a composite one: it has to bind to a
 * UDC of our choosing and fail at once (-EBUSY) when that one is taken, which
 * usb_composite_probe() does not allow. So the few standard requests a
 * one-interface HID device gets are answered here.
 *
 * Contexts: setup(), the endpoint completions and input() run in atomic
 * context. Lock order: the core's v->lock, then g->lock; nothing here calls
 * back into the core, or into the UDC, with g->lock held.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "ds4t-descriptors.h"
#include "ds4t-vcore.h"

#define DS4T_GADGET_NAME	"ds4t-gadget"

static char *udc;
module_param(udc, charp, 0444);
MODULE_PARM_DESC(udc, "UDC to bind to (e.g. dummy_udc.0); any free one if unset");

#define EP0_BUF_SIZE		1024	/* the largest answer is a report descriptor */
#define EP_PACKET		64

/* HID class requests. */
#define HID_REQ_GET_REPORT_	0x01
#define HID_REQ_GET_IDLE_	0x02
#define HID_REQ_GET_PROTOCOL_	0x03
#define HID_REQ_SET_REPORT_	0x09
#define HID_REQ_SET_IDLE_	0x0a
#define HID_REQ_SET_PROTOCOL_	0x0b
#define HID_DT_HID_		0x21
#define HID_DT_REPORT_		0x22

#define REPORT_TYPE_INPUT	1
#define REPORT_TYPE_OUTPUT	2
#define REPORT_TYPE_FEATURE	3

enum { STR_MANUFACTURER = 1, STR_PRODUCT, STR_SERIAL };

struct ds4t_gadget {
	struct ds4t_virtual *v;
	struct usb_gadget_driver driver;
	const u8 *rdesc;
	u16 rdesc_len;

	struct usb_device_descriptor dev_desc;
	struct usb_endpoint_descriptor in_desc, out_desc;
	char manufacturer[48], product[DS4T_NAME_LEN], serial[DS4T_MAC_LEN];
	struct usb_string strings[STR_SERIAL + 1 + 1];
	struct usb_gadget_strings string_tab;

	/* Set in bind(), cleared in unbind(); the UDC serialises the two. */
	struct usb_gadget *gadget;
	struct usb_ep *ep_in, *ep_out;
	struct usb_request *req0, *req_in, *req_out;

	spinlock_t lock;	/* everything below */
	bool configured;
	bool in_busy;		/* req_in is queued */
	bool in_pending;	/* a newer report waits in in_next */
	u8 in_next[DS4T_VIRT_REPORT_SIZE];
	size_t in_next_len;
	u8 ep0_report_type;	/* SET_REPORT data stage in flight */
};

static struct ds4t_gadget *ds4t_gadget_of(struct usb_gadget *gadget)
{
	return get_gadget_data(gadget);
}

/* --------------------------------------------------------------- descriptors */

static const struct usb_interface_descriptor ds4t_intf_desc = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_HID,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
};

static const struct usb_endpoint_descriptor ds4t_in_template = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize = cpu_to_le16(EP_PACKET),
	.bInterval = 4,
};

static const struct usb_endpoint_descriptor ds4t_out_template = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize = cpu_to_le16(EP_PACKET),
	.bInterval = 4,
};

#define HID_DESC_SIZE		9
#define CONFIG_TOTAL		(USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE + \
				 HID_DESC_SIZE + 2 * USB_DT_ENDPOINT_SIZE)

static void ds4t_gadget_hid_desc(const struct ds4t_gadget *g, u8 *p)
{
	p[0] = HID_DESC_SIZE;
	p[1] = HID_DT_HID_;
	p[2] = 0x11;		/* bcdHID 1.11 */
	p[3] = 0x01;
	p[4] = 0;		/* bCountryCode */
	p[5] = 1;		/* bNumDescriptors */
	p[6] = HID_DT_REPORT_;
	p[7] = g->rdesc_len & 0xff;
	p[8] = g->rdesc_len >> 8;
}

/* The configuration with its interface, HID and endpoint descriptors. */
static int ds4t_gadget_config_desc(const struct ds4t_gadget *g, u8 *buf)
{
	struct usb_config_descriptor *c = (struct usb_config_descriptor *)buf;
	u8 *p = buf + USB_DT_CONFIG_SIZE;

	c->bLength = USB_DT_CONFIG_SIZE;
	c->bDescriptorType = USB_DT_CONFIG;
	c->wTotalLength = cpu_to_le16(CONFIG_TOTAL);
	c->bNumInterfaces = 1;
	c->bConfigurationValue = 1;
	c->iConfiguration = 0;
	c->bmAttributes = USB_CONFIG_ATT_ONE;	/* bus powered */
	c->bMaxPower = 250;			/* 500 mA */

	memcpy(p, &ds4t_intf_desc, USB_DT_INTERFACE_SIZE);
	p += USB_DT_INTERFACE_SIZE;
	ds4t_gadget_hid_desc(g, p);
	p += HID_DESC_SIZE;
	memcpy(p, &g->in_desc, USB_DT_ENDPOINT_SIZE);
	p += USB_DT_ENDPOINT_SIZE;
	memcpy(p, &g->out_desc, USB_DT_ENDPOINT_SIZE);
	return CONFIG_TOTAL;
}

/* ------------------------------------------------------------------ input */

/*
 * Queues req_in with the newest report; g->lock held on entry, dropped on
 * return either way, as the UDC must not be entered holding it (a completion
 * may run before usb_ep_queue() returns).
 */
static void ds4t_gadget_send_unlock(struct ds4t_gadget *g, unsigned long flags)
{
	struct usb_request *req = g->req_in;

	memcpy(req->buf, g->in_next, g->in_next_len);
	req->length = g->in_next_len;
	g->in_pending = false;
	g->in_busy = true;
	spin_unlock_irqrestore(&g->lock, flags);

	if (usb_ep_queue(g->ep_in, req, GFP_ATOMIC)) {
		spin_lock_irqsave(&g->lock, flags);
		g->in_busy = false;
		spin_unlock_irqrestore(&g->lock, flags);
	}
}

static void ds4t_gadget_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct ds4t_gadget *g = req->context;
	unsigned long flags;

	spin_lock_irqsave(&g->lock, flags);
	g->in_busy = false;
	if (req->status || !g->configured || !g->in_pending) {
		spin_unlock_irqrestore(&g->lock, flags);
		return;
	}
	ds4t_gadget_send_unlock(g, flags);
}

/* Core callback; atomic, the core's v->lock held. Newest report wins. */
static void ds4t_gadget_input(struct ds4t_virtual *v, u8 *report, size_t len)
{
	struct ds4t_gadget *g = v->front;
	unsigned long flags;

	if (WARN_ON_ONCE(len > sizeof(g->in_next)))
		return;

	spin_lock_irqsave(&g->lock, flags);
	memcpy(g->in_next, report, len);
	g->in_next_len = len;
	if (!g->configured || g->in_busy) {
		g->in_pending = g->configured;
		spin_unlock_irqrestore(&g->lock, flags);
		return;
	}
	ds4t_gadget_send_unlock(g, flags);
}

/* ----------------------------------------------------------------- output */

/* Stop codes: the endpoint is going away, so the request is not requeued. */
static bool ds4t_gadget_req_stopped(int status)
{
	return status == -ESHUTDOWN || status == -ECONNRESET ||
	       status == -ECONNABORTED;
}

static void ds4t_gadget_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct ds4t_gadget *g = req->context;
	unsigned long flags;
	bool requeue;

	if (!req->status && req->actual)
		ds4t_vcore_output(g->v, req->buf, min(req->actual, req->length));

	spin_lock_irqsave(&g->lock, flags);
	requeue = g->configured && !ds4t_gadget_req_stopped(req->status);
	spin_unlock_irqrestore(&g->lock, flags);
	if (requeue) {
		req->length = EP_PACKET;
		usb_ep_queue(ep, req, GFP_ATOMIC);
	}
}

/* ------------------------------------------------------------ configuration */

/* Process or atomic context; not with g->lock held (completions run). */
static void ds4t_gadget_disable(struct ds4t_gadget *g)
{
	unsigned long flags;
	bool was;

	spin_lock_irqsave(&g->lock, flags);
	was = g->configured;
	g->configured = false;
	g->in_pending = false;
	spin_unlock_irqrestore(&g->lock, flags);
	if (!was)
		return;
	usb_ep_disable(g->ep_in);	/* completes queued requests: -ESHUTDOWN */
	usb_ep_disable(g->ep_out);
}

static int ds4t_gadget_enable(struct ds4t_gadget *g)
{
	unsigned long flags;
	int ret;

	ds4t_gadget_disable(g);

	g->ep_in->desc = &g->in_desc;
	g->ep_out->desc = &g->out_desc;
	ret = usb_ep_enable(g->ep_in);
	if (ret)
		return ret;
	ret = usb_ep_enable(g->ep_out);
	if (ret) {
		usb_ep_disable(g->ep_in);
		return ret;
	}

	spin_lock_irqsave(&g->lock, flags);
	g->configured = true;
	g->in_busy = false;
	spin_unlock_irqrestore(&g->lock, flags);

	g->req_out->length = EP_PACKET;
	if (usb_ep_queue(g->ep_out, g->req_out, GFP_ATOMIC))
		dev_warn(&g->gadget->dev, "could not queue the OUT endpoint\n");
	return 0;
}

/* ---------------------------------------------------------------------- ep0 */

static void ds4t_gadget_ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct ds4t_gadget *g = req->context;
	unsigned long flags;
	u8 type;

	spin_lock_irqsave(&g->lock, flags);
	type = g->ep0_report_type;
	g->ep0_report_type = 0;
	spin_unlock_irqrestore(&g->lock, flags);

	/* A SET_REPORT's data: an output report is rumble and LED. */
	if (type == REPORT_TYPE_OUTPUT && !req->status && req->actual)
		ds4t_vcore_output(g->v, req->buf, min(req->actual, req->length));
}

static int ds4t_gadget_get_descriptor(struct ds4t_gadget *g,
				      const struct usb_ctrlrequest *ctrl, u8 *buf)
{
	u16 value = le16_to_cpu(ctrl->wValue);
	u8 recip = ctrl->bRequestType & USB_RECIP_MASK;

	switch (value >> 8) {
	case USB_DT_DEVICE:
		g->dev_desc.bMaxPacketSize0 = g->gadget->ep0->maxpacket;
		memcpy(buf, &g->dev_desc, sizeof(g->dev_desc));
		return sizeof(g->dev_desc);
	case USB_DT_CONFIG:
		return ds4t_gadget_config_desc(g, buf);
	case USB_DT_STRING:
		return usb_gadget_get_string(&g->string_tab, value & 0xff, buf);
	case HID_DT_HID_:
		if (recip != USB_RECIP_INTERFACE)
			break;
		ds4t_gadget_hid_desc(g, buf);
		return HID_DESC_SIZE;
	case HID_DT_REPORT_:
		if (recip != USB_RECIP_INTERFACE)
			break;
		memcpy(buf, g->rdesc, g->rdesc_len);
		return g->rdesc_len;
	}
	/* A full-speed device: no qualifier, no other-speed configuration. */
	return -EOPNOTSUPP;
}

static int ds4t_gadget_standard(struct ds4t_gadget *g,
				const struct usb_ctrlrequest *ctrl, u8 *buf)
{
	u16 value = le16_to_cpu(ctrl->wValue);
	unsigned long flags;
	bool configured;

	switch (ctrl->bRequest) {
	case USB_REQ_GET_DESCRIPTOR:
		if (!(ctrl->bRequestType & USB_DIR_IN))
			break;
		return ds4t_gadget_get_descriptor(g, ctrl, buf);
	case USB_REQ_SET_CONFIGURATION:
		if (ctrl->bRequestType != USB_RECIP_DEVICE)
			break;
		if (value == 0) {
			ds4t_gadget_disable(g);
			return 0;
		}
		if (value != 1)
			break;
		usb_gadget_vbus_draw(g->gadget, 500);
		return ds4t_gadget_enable(g);
	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != (USB_DIR_IN | USB_RECIP_DEVICE))
			break;
		spin_lock_irqsave(&g->lock, flags);
		configured = g->configured;
		spin_unlock_irqrestore(&g->lock, flags);
		buf[0] = configured;
		return 1;
	case USB_REQ_SET_INTERFACE:
		/* One interface, one setting: re-selecting it resets it. */
		if (ctrl->bRequestType != USB_RECIP_INTERFACE || value != 0 ||
		    le16_to_cpu(ctrl->wIndex) != 0)
			break;
		return ds4t_gadget_enable(g);
	case USB_REQ_GET_INTERFACE:
		if (ctrl->bRequestType != (USB_DIR_IN | USB_RECIP_INTERFACE) ||
		    le16_to_cpu(ctrl->wIndex) != 0)
			break;
		buf[0] = 0;
		return 1;
	}
	return -EOPNOTSUPP;
}

static int ds4t_gadget_class(struct ds4t_gadget *g,
			     const struct usb_ctrlrequest *ctrl, u8 *buf)
{
	u16 value = le16_to_cpu(ctrl->wValue);
	u16 length = le16_to_cpu(ctrl->wLength);
	u8 type = value >> 8, id = value & 0xff;
	unsigned long flags;
	int ret;

	if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE ||
	    le16_to_cpu(ctrl->wIndex) != 0)
		return -EOPNOTSUPP;

	switch (ctrl->bRequest) {
	case HID_REQ_GET_REPORT_:
		if (type == REPORT_TYPE_FEATURE) {
			buf[0] = id;
			return ds4t_vcore_get_feature(g->v, id, buf,
						      min_t(u16, length,
							    EP0_BUF_SIZE));
		}
		if (type != REPORT_TYPE_INPUT)
			break;
		spin_lock_irqsave(&g->lock, flags);
		ret = g->in_next_len ? (int)g->in_next_len : -EOPNOTSUPP;
		memcpy(buf, g->in_next, g->in_next_len);
		spin_unlock_irqrestore(&g->lock, flags);
		return ret;
	case HID_REQ_SET_REPORT_:
		/* The data stage completes in ds4t_gadget_ep0_complete(). */
		if (length > EP0_BUF_SIZE)
			break;
		spin_lock_irqsave(&g->lock, flags);
		g->ep0_report_type = type;
		spin_unlock_irqrestore(&g->lock, flags);
		return length;
	case HID_REQ_GET_IDLE_:
	case HID_REQ_GET_PROTOCOL_:
		buf[0] = ctrl->bRequest == HID_REQ_GET_PROTOCOL_;	/* report */
		return 1;
	case HID_REQ_SET_IDLE_:
	case HID_REQ_SET_PROTOCOL_:
		return 0;
	}
	return -EOPNOTSUPP;
}

/* Atomic context. A negative return stalls the request. */
static int ds4t_gadget_setup(struct usb_gadget *gadget,
			     const struct usb_ctrlrequest *ctrl)
{
	struct ds4t_gadget *g = ds4t_gadget_of(gadget);
	struct usb_request *req = g->req0;
	u16 length = le16_to_cpu(ctrl->wLength);
	int value;

	switch (ctrl->bRequestType & USB_TYPE_MASK) {
	case USB_TYPE_STANDARD:
		value = ds4t_gadget_standard(g, ctrl, req->buf);
		break;
	case USB_TYPE_CLASS:
		value = ds4t_gadget_class(g, ctrl, req->buf);
		break;
	default:
		value = -EOPNOTSUPP;
	}
	if (value < 0)
		return value;

	/* The data stage, or the status stage of a request without one. */
	value = min_t(int, value, length);
	req->length = value;
	req->zero = value < length && (ctrl->bRequestType & USB_DIR_IN);
	value = usb_ep_queue(gadget->ep0, req, GFP_ATOMIC);
	if (value)
		dev_dbg(&gadget->dev, "ep0 queue: %d\n", value);
	return value;
}

/* ---------------------------------------------------- bind, unbind, events */

static struct usb_request *ds4t_gadget_alloc_req(struct ds4t_gadget *g,
						 struct usb_ep *ep, size_t size,
						 void (*complete)(struct usb_ep *,
								  struct usb_request *))
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);

	if (!req)
		return NULL;
	req->buf = kzalloc(size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}
	req->complete = complete;
	req->context = g;
	return req;
}

static void ds4t_gadget_free_req(struct usb_ep *ep, struct usb_request *req)
{
	if (!req)
		return;
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

static void ds4t_gadget_free_all(struct ds4t_gadget *g)
{
	ds4t_gadget_free_req(g->gadget->ep0, g->req0);
	if (g->ep_in)
		ds4t_gadget_free_req(g->ep_in, g->req_in);
	if (g->ep_out)
		ds4t_gadget_free_req(g->ep_out, g->req_out);
	g->req0 = NULL;
	g->req_in = NULL;
	g->req_out = NULL;
	usb_ep_autoconfig_reset(g->gadget);
	g->ep_in = NULL;
	g->ep_out = NULL;
}

/* Process context, from the UDC core. */
static int ds4t_gadget_bind(struct usb_gadget *gadget,
			    struct usb_gadget_driver *driver)
{
	struct ds4t_gadget *g = container_of(driver, struct ds4t_gadget, driver);

	g->gadget = gadget;
	set_gadget_data(gadget, g);

	g->in_desc = ds4t_in_template;
	g->out_desc = ds4t_out_template;
	g->ep_in = usb_ep_autoconfig(gadget, &g->in_desc);
	g->ep_out = usb_ep_autoconfig(gadget, &g->out_desc);
	if (!g->ep_in || !g->ep_out)
		goto fail;

	g->req0 = ds4t_gadget_alloc_req(g, gadget->ep0, EP0_BUF_SIZE,
					ds4t_gadget_ep0_complete);
	g->req_in = ds4t_gadget_alloc_req(g, g->ep_in, EP_PACKET,
					  ds4t_gadget_in_complete);
	g->req_out = ds4t_gadget_alloc_req(g, g->ep_out, EP_PACKET,
					   ds4t_gadget_out_complete);
	if (!g->req0 || !g->req_in || !g->req_out)
		goto fail;
	return 0;

fail:
	ds4t_gadget_free_all(g);
	set_gadget_data(gadget, NULL);
	g->gadget = NULL;
	return -ENOMEM;
}

static void ds4t_gadget_unbind(struct usb_gadget *gadget)
{
	struct ds4t_gadget *g = ds4t_gadget_of(gadget);
	unsigned long flags;

	ds4t_gadget_disable(g);
	/*
	 * input() runs entirely under the core's v->lock and checks
	 * configured, cleared above: taking that lock once means none is
	 * still about to queue req_in when it is freed.
	 */
	spin_lock_irqsave(&g->v->lock, flags);
	spin_unlock_irqrestore(&g->v->lock, flags);

	usb_ep_dequeue(gadget->ep0, g->req0);
	ds4t_gadget_free_all(g);
	set_gadget_data(gadget, NULL);
	g->gadget = NULL;
}

/* Unplugged (dummy_hcd's host side gone) or reset: unconfigured. */
static void ds4t_gadget_disconnect(struct usb_gadget *gadget)
{
	struct ds4t_gadget *g = ds4t_gadget_of(gadget);

	if (g)
		ds4t_gadget_disable(g);
}

static void ds4t_gadget_suspend(struct usb_gadget *gadget)
{
}

static void ds4t_gadget_resume(struct usb_gadget *gadget)
{
}

/* ------------------------------------------------------------- the frontend */

static void ds4t_gadget_identity(struct ds4t_gadget *g,
				 const struct ds4t_virtual_cfg *cfg)
{
	const bool ds4 = cfg->type == DS4T_VIRTUAL_DS4;

	/* A real pad's: bcdDevice 0x0100, Sony's strings, the MAC as serial. */
	g->dev_desc = (struct usb_device_descriptor) {
		.bLength = USB_DT_DEVICE_SIZE,
		.bDescriptorType = USB_DT_DEVICE,
		.bcdUSB = cpu_to_le16(0x0200),
		.bDeviceClass = 0,
		.idVendor = cpu_to_le16(0x054c),
		.idProduct = cpu_to_le16(ds4 ? 0x05c4 : 0x0ce6),
		.bcdDevice = cpu_to_le16(0x0100),
		.iManufacturer = STR_MANUFACTURER,
		.iProduct = STR_PRODUCT,
		.iSerialNumber = STR_SERIAL,
		.bNumConfigurations = 1,
	};
	strscpy(g->manufacturer, ds4 ? "Sony Computer Entertainment" :
		"Sony Interactive Entertainment", sizeof(g->manufacturer));
	strscpy(g->product, cfg->name[0] ? cfg->name : ds4 ?
		"Wireless Controller" : "DualSense Wireless Controller",
		sizeof(g->product));
	strscpy(g->serial, cfg->mac, sizeof(g->serial));

	g->strings[0] = (struct usb_string){ STR_MANUFACTURER, g->manufacturer };
	g->strings[1] = (struct usb_string){ STR_PRODUCT, g->product };
	g->strings[2] = (struct usb_string){ STR_SERIAL, g->serial };
	g->string_tab.language = 0x0409;	/* en-US */
	g->string_tab.strings = g->strings;

	g->rdesc = ds4 ? ds4_usb_rdesc : dualsense_usb_rdesc;
	g->rdesc_len = ds4 ? sizeof(ds4_usb_rdesc) : sizeof(dualsense_usb_rdesc);
}

static_assert(sizeof(ds4_usb_rdesc) <= EP0_BUF_SIZE);
static_assert(sizeof(dualsense_usb_rdesc) <= EP0_BUF_SIZE);

static int ds4t_gadget_start(struct ds4t_virtual *v,
			     const struct ds4t_virtual_cfg *cfg)
{
	struct ds4t_gadget *g;
	int ret;

	might_sleep();
	g = kzalloc(sizeof(*g), GFP_KERNEL);
	if (!g)
		return -ENOMEM;
	g->v = v;
	spin_lock_init(&g->lock);
	ds4t_gadget_identity(g, cfg);

	g->driver = (struct usb_gadget_driver) {
		.function = DS4T_GADGET_NAME,
		.max_speed = USB_SPEED_FULL,	/* as a real pad */
		.bind = ds4t_gadget_bind,
		.unbind = ds4t_gadget_unbind,
		.setup = ds4t_gadget_setup,
		.disconnect = ds4t_gadget_disconnect,
		.reset = ds4t_gadget_disconnect,
		.suspend = ds4t_gadget_suspend,
		.resume = ds4t_gadget_resume,
		.driver = { .name = DS4T_GADGET_NAME },
		.udc_name = udc && udc[0] ? udc : NULL,
		.match_existing_only = 1,	/* -EBUSY, not a pending driver */
	};

	v->front = g;
	ret = usb_gadget_register_driver(&g->driver);
	if (ret) {
		v->front = NULL;
		kfree(g);
	}
	return ret;
}

static void ds4t_gadget_stop(struct ds4t_virtual *v)
{
	struct ds4t_gadget *g = v->front;

	might_sleep();
	/* Disconnects, then unbinds: no callback runs once this returns. */
	usb_gadget_unregister_driver(&g->driver);
	v->front = NULL;
	kfree(g);
}

static const struct ds4t_vfront_ops ds4t_gadget_ops = {
	.start = ds4t_gadget_start,
	.stop = ds4t_gadget_stop,
	.input = ds4t_gadget_input,
	.owner = THIS_MODULE,
};

static int __init ds4t_gadget_init(void)
{
	return ds4t_register_usb_frontend(&ds4t_gadget_ops);
}

static void __exit ds4t_gadget_exit(void)
{
	ds4t_unregister_usb_frontend(&ds4t_gadget_ops);
}

module_init(ds4t_gadget_init);
module_exit(ds4t_gadget_exit);

MODULE_DESCRIPTION("ds4-translator: the emulated pad as a USB gadget (kernel-usb backend)");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: hid-ds4-translator");
