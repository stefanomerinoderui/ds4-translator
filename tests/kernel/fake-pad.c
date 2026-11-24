// SPDX-License-Identifier: GPL-2.0-only
//
// Fake DualShock 4 on top of /dev/uhid, for testing hid-ds4-translator
// without hardware. To the kernel it is a HID device with the DS4 IDs on a
// chosen bus, so it binds (or doesn't) exactly like a real pad.
//
//   fake-pad --bus usb|bt --mac AA:BB:CC:DD:EE:FF [--count N] [--hold SECONDS]
//            [--stream 1] [--fixed 1] [--slow-feature MS]
//
// Sends N input reports (default 20, 100 Hz), answers feature-report reads
// with a recognisable pattern, then stays alive for --hold seconds (default
// 3600) so the test can poke at it; with --stream 1 it keeps sending input
// reports during that time. --slow-feature delays the answer to feature report
// 0x02 by MS milliseconds once the pad is ready, so a test can unplug it while
// the kernel is blocked waiting for it. Output reports the kernel sends are
// logged as "output rnum=XX len=N". With --fixed 1 every input report carries
// the known state of the FIXED_* values below instead of a counter, so a test can
// check what a translating consumer makes of it. SIGTERM destroys the device.

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
// The fake's descriptor is the USB one, which declares only input report 0x01
// (64 bytes); the HID core drops any other report ID, so both buses send that.
#define REPORT_SIZE 64
#define FEATURE_CAL_USB 0x02
#define FEATURE_CAL_USB_SIZE 37
#define FEATURE_CAL_BT 0x05
#define FEATURE_CAL_BT_SIZE 41
// hid-playstation reads these in probe and gives up if they fail.
#define FEATURE_PAIRING 0x12
#define FEATURE_PAIRING_SIZE 16
#define FEATURE_FIRMWARE 0xA3
#define FEATURE_FIRMWARE_SIZE 49

// CRC32 as hid-playstation computes it for Bluetooth feature reports: the seed
// byte first, then the report including its ID.
static uint32_t crc32_seeded(uint8_t seed, const uint8_t *data, size_t len)
{
	uint32_t crc = 0xFFFFFFFF;

	for (size_t i = 0; i <= len; i++) {
		crc ^= i == 0 ? seed : data[i - 1];
		for (int bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ (0xEDB88320U & -(crc & 1));
	}
	return ~crc;
}

// The 36 calibration bytes (little-endian s16): three gyro biases, gyro
// pitch+, pitch-, yaw+, yaw-, roll+, roll- (the Bluetooth order), speed +/-,
// then accelerometer x+, x-, y+, y-, z+, z-. Read in the USB order instead
// (pitch+, yaw+, roll+, pitch-, yaw-, roll-) the three gyro denominators come
// out zero, which is exactly what makes this payload a layout oracle.
static const uint8_t g_bt_calibration[36] = {
	0, 0, 0, 0, 0, 0,
	100, 0, 200, 0, 44, 1, 100, 0, 200, 0, 44, 1,
	0x1c, 0x02, 0x1c, 0x02,
	0x00, 0x20, 0x00, 0xe0, 0x00, 0x20, 0x00, 0xe0, 0x00, 0x20, 0x00, 0xe0,
	0, 0,
};

// A known input state (offsets are into the 0x01 report): sticks and triggers
// all different, cross pressed, d-pad right.
#define FIXED_X 0x10
#define FIXED_Y 0x20
#define FIXED_RX 0x30
#define FIXED_RY 0x40
#define FIXED_BUTTONS0 0x22
#define FIXED_Z 0x50
#define FIXED_RZ 0x60
static int g_fixed;

static volatile sig_atomic_t g_stop;
static int g_slow_feature_ms;	// delay for report 0x02; 0 until the pad is ready
static int g_slow_feature_arg;
static uint8_t g_mac[6];	// as sent in the pairing report: least significant byte first

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
// real data from a zero-filled buffer. The pairing report carries the MAC.
static int answer_get_report(int fd, const struct uhid_get_report_req *req)
{
	struct uhid_event ev;
	size_t size = 0;

	printf("get_report rnum=%02x id=%u\n", req->rnum, req->id);
	fflush(stdout);
	if (req->rnum == FEATURE_CAL_USB && g_slow_feature_ms) {
		struct timespec ts = { g_slow_feature_ms / 1000,
				       (g_slow_feature_ms % 1000) * 1000000L };

		nanosleep(&ts, NULL);
	}

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_GET_REPORT_REPLY;
	ev.u.get_report_reply.id = req->id;
	if (req->rnum == FEATURE_CAL_USB)
		size = FEATURE_CAL_USB_SIZE;
	else if (req->rnum == FEATURE_CAL_BT)
		size = FEATURE_CAL_BT_SIZE;

	else if (req->rnum == FEATURE_PAIRING)
		size = FEATURE_PAIRING_SIZE;
	else if (req->rnum == FEATURE_FIRMWARE)
		size = FEATURE_FIRMWARE_SIZE;

	if (size) {
		ev.u.get_report_reply.size = size;
		memset(ev.u.get_report_reply.data, 0xA5, size);
		ev.u.get_report_reply.data[0] = req->rnum;
		if (req->rnum == FEATURE_PAIRING)
			memcpy(&ev.u.get_report_reply.data[1], g_mac, sizeof(g_mac));
		if (req->rnum == FEATURE_CAL_BT) {
			// A Bluetooth calibration report with a valid CRC whose gyro
			// words only make sense in the Bluetooth order (see below);
			// hid-playstation rejects it for a wrong order, so a pad
			// that binds proves both the CRC seed and the layout.
			uint32_t crc;

			memset(ev.u.get_report_reply.data, 0, size);
			ev.u.get_report_reply.data[0] = req->rnum;
			memcpy(&ev.u.get_report_reply.data[1], g_bt_calibration,
			       sizeof(g_bt_calibration));
			crc = crc32_seeded(0xA3, ev.u.get_report_reply.data, size - 4);
			for (int i = 0; i < 4; i++)
				ev.u.get_report_reply.data[size - 4 + i] = crc >> (8 * i);
		}
	} else {
		ev.u.get_report_reply.err = EIO;
	}
	return uhid_write(fd, &ev);
}

static int send_input(int fd, uint8_t counter)
{
	struct uhid_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT2;
	ev.u.input2.size = REPORT_SIZE;
	ev.u.input2.data[0] = 0x01;
	ev.u.input2.data[1] = counter;
	if (g_fixed) {
		ev.u.input2.data[1] = FIXED_X;
		ev.u.input2.data[2] = FIXED_Y;
		ev.u.input2.data[3] = FIXED_RX;
		ev.u.input2.data[4] = FIXED_RY;
		ev.u.input2.data[5] = FIXED_BUTTONS0;
		ev.u.input2.data[8] = FIXED_Z;
		ev.u.input2.data[9] = FIXED_RZ;
	}
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
		else if (ev.type == UHID_OUTPUT) {
			const uint8_t *d = ev.u.output.data;

			printf("output rnum=%02x len=%u", d[0], ev.u.output.size);
			if (d[0] == 0x11 && ev.u.output.size == 78) {
				// A Bluetooth output report: check the CRC the way the
				// pad does (seed 0xA2), and show the rumble bytes.
				uint32_t want = crc32_seeded(0xA2, d, 74);
				uint32_t got = d[74] | d[75] << 8 | d[76] << 16 |
					       (uint32_t)d[77] << 24;

				printf(" crc=%s motor_right=%u motor_left=%u rgb=%u,%u,%u",
				       want == got ? "ok" : "bad", d[6], d[7],
				       d[8], d[9], d[10]);
			}
			printf("\n");
			fflush(stdout);
		} else if (ev.type == UHID_SET_REPORT) {
			struct uhid_event reply;

			printf("set_report rnum=%02x len=%u\n",
			       ev.u.set_report.rnum, ev.u.set_report.size);
			fflush(stdout);
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
		"[--count N] [--hold SECONDS] [--stream 1] [--fixed 1] [--slow-feature MS]\n");
	return 2;
}

int main(int argc, char **argv)
{
	int bus = 0, count = 20, hold = 3600, stream = 0, fd, i;
	uint8_t counter = 0;
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
		else if (!strcmp(argv[i], "--stream"))
			stream = atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "--fixed"))
			g_fixed = atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "--slow-feature"))
			g_slow_feature_arg = atoi(argv[i + 1]);
		else
			return usage();
	}
	if (!bus || !mac || i != argc)
		return usage();
	if (sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &g_mac[5], &g_mac[4],
		   &g_mac[3], &g_mac[2], &g_mac[1], &g_mac[0]) != 6) {
		fprintf(stderr, "fake-pad: bad MAC '%s'\n", mac);
		return 2;
	}

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
		if (send_input(fd, (uint8_t)i) || handle_events(fd))
			return 1;
		sleep_ms(10);
	}
	g_slow_feature_ms = g_slow_feature_arg;
	puts("fake-pad: ready");
	fflush(stdout);

	for (i = 0; i < hold * 100 && !g_stop; i++) {
		sleep_ms(10);
		if ((stream && send_input(fd, counter++)) ||
		    handle_events(fd))
			break;
	}

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_DESTROY;
	uhid_write(fd, &ev);
	close(fd);
	return 0;
}
