// SPDX-License-Identifier: GPL-2.0-only
//
// Fake DualShock 4 on top of /dev/uhid, for testing hid-ds4-translator
// without hardware. To the kernel it is a HID device with the DS4 IDs on a
// chosen bus, so it binds (or doesn't) exactly like a real pad.
//
//   fake-pad --bus usb|bt --mac AA:BB:CC:DD:EE:FF [--count N] [--hold SECONDS]
//
// Sends N input reports (default 20, 100 Hz), answers feature-report reads
// with a recognisable pattern, then stays alive for --hold seconds (default
// 3600) so the test can poke at it. SIGTERM destroys the device.

#include <errno.h>
#include <fcntl.h>
#include <linux/uhid.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../src/descriptors.h"

#define VID_SONY 0x054c
#define PID_DS4_V1 0x05c4
#define REPORT_SIZE_USB 64
#define REPORT_SIZE_BT 78
#define FEATURE_CAL_USB 0x02
#define FEATURE_CAL_USB_SIZE 37
#define FEATURE_CAL_BT 0x05
#define FEATURE_CAL_BT_SIZE 41

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

static int uhid_write(int fd, const struct uhid_event *ev)
{
	ssize_t n = write(fd, ev, sizeof(*ev));

	if (n != (ssize_t)sizeof(*ev)) {
		perror("fake-pad: write /dev/uhid");
		return -1;
	}
	return 0;
}

static int create_device(int fd, int bus, const char *mac)
{
	struct uhid_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_CREATE2;
	snprintf((char *)ev.u.create2.name, sizeof(ev.u.create2.name),
		 "Wireless Controller");
	snprintf((char *)ev.u.create2.uniq, sizeof(ev.u.create2.uniq), "%s", mac);
	ev.u.create2.rd_size = sizeof(ds4_usb_rdesc);
	memcpy(ev.u.create2.rd_data, ds4_usb_rdesc, sizeof(ds4_usb_rdesc));
	ev.u.create2.bus = bus;
	ev.u.create2.vendor = VID_SONY;
	ev.u.create2.product = PID_DS4_V1;
	return uhid_write(fd, &ev);
}

// Feature reads: report id first, then a pattern of 0xA5 so a test can tell
// real data from a zero-filled buffer.
static int answer_get_report(int fd, const struct uhid_get_report_req *req)
{
	struct uhid_event ev;
	size_t size = 0;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_GET_REPORT_REPLY;
	ev.u.get_report_reply.id = req->id;
	if (req->rnum == FEATURE_CAL_USB)
		size = FEATURE_CAL_USB_SIZE;
	else if (req->rnum == FEATURE_CAL_BT)
		size = FEATURE_CAL_BT_SIZE;

	if (size) {
		ev.u.get_report_reply.size = size;
		memset(ev.u.get_report_reply.data, 0xA5, size);
		ev.u.get_report_reply.data[0] = req->rnum;
	} else {
		ev.u.get_report_reply.err = EIO;
	}
	return uhid_write(fd, &ev);
}

static int send_input(int fd, int bus, uint8_t counter)
{
	struct uhid_event ev;
	size_t size = bus == BUS_BLUETOOTH ? REPORT_SIZE_BT : REPORT_SIZE_USB;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT2;
	ev.u.input2.size = size;
	ev.u.input2.data[0] = bus == BUS_BLUETOOTH ? 0x11 : 0x01;
	ev.u.input2.data[1] = counter;
	return uhid_write(fd, &ev);
}

// Drains whatever the kernel sent us; returns -1 on a fatal error.
static int handle_events(int fd)
{
	struct uhid_event ev;
	ssize_t n;

	while ((n = read(fd, &ev, sizeof(ev))) == (ssize_t)sizeof(ev)) {
		if (ev.type == UHID_GET_REPORT &&
		    answer_get_report(fd, &ev.u.get_report))
			return -1;
		else if (ev.type == UHID_SET_REPORT) {
			struct uhid_event reply;

			memset(&reply, 0, sizeof(reply));
			reply.type = UHID_SET_REPORT_REPLY;
			reply.u.set_report_reply.id = ev.u.set_report.id;
			if (uhid_write(fd, &reply))
				return -1;
		}
	}
	return (n < 0 && errno != EAGAIN) ? -1 : 0;
}

static void sleep_ms(long ms)
{
	struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };

	nanosleep(&ts, NULL);
}

static int usage(void)
{
	fprintf(stderr,
		"usage: fake-pad --bus usb|bt --mac XX:XX:XX:XX:XX:XX "
		"[--count N] [--hold SECONDS]\n");
	return 2;
}

int main(int argc, char **argv)
{
	int bus = 0, count = 20, hold = 3600, fd, i;
	const char *mac = NULL;
	struct uhid_event ev;

	for (i = 1; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "--bus"))
			bus = !strcmp(argv[i + 1], "usb") ? BUS_USB :
			      !strcmp(argv[i + 1], "bt") ? BUS_BLUETOOTH : 0;
		else if (!strcmp(argv[i], "--mac"))
			mac = argv[i + 1];
		else if (!strcmp(argv[i], "--count"))
			count = atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "--hold"))
			hold = atoi(argv[i + 1]);
		else
			return usage();
	}
	if (!bus || !mac || i != argc)
		return usage();

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);

	fd = open("/dev/uhid", O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		perror("fake-pad: open /dev/uhid");
		return 1;
	}
	if (create_device(fd, bus, mac))
		return 1;

	// Give the kernel a moment to probe and issue its GET_REPORTs.
	for (i = 0; i < 50 && !g_stop; i++) {
		sleep_ms(10);
		if (handle_events(fd))
			return 1;
	}
	for (i = 0; i < count && !g_stop; i++) {
		if (send_input(fd, bus, (uint8_t)i) || handle_events(fd))
			return 1;
		sleep_ms(10);
	}
	puts("fake-pad: ready");
	fflush(stdout);

	for (i = 0; i < hold * 100 && !g_stop; i++) {
		sleep_ms(10);
		if (handle_events(fd))
			break;
	}

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_DESTROY;
	uhid_write(fd, &ev);
	close(fd);
	return 0;
}
