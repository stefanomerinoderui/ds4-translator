// SPDX-License-Identifier: GPL-2.0-only
//
// Stands in for the daemon's "kernel" backend in the VM: attaches to the
// module, creates the virtual controller (following a fake pad, or none) and
// reports what hid-playstation made of it, as facts on stdout for the test
// script to check.
//
//   virtual-check --type 1|2 --mac XX:.. [--pad-mac XX:..] [--inject]
//                 [--rumble] [--hidraw-rumble usb|bt] [--features ID,..]
//                 [--usb] [--fail-nth N] [--hold SECONDS]
//   virtual-check --observe --name NAME [--hold SECONDS] [--watch]
//
// With --observe it touches nothing: it only reads the state of the gamepad
// called NAME (a virtual pad someone else made, such as the daemon). With
// --watch it prints every change of the left stick's X ("abs_x=N") instead of
// sampling the state, so a value that lasts a single report is seen too.
//
// --hidraw-rumble writes a rumble report straight to the virtual pad's hidraw
// node, the way Steam and SDL do, in the USB or the Bluetooth layout.
// --features reads each feature report (hex IDs) from the virtual pad's hidraw
// node, the way a game's pad library does, and prints what came back.
// --usb makes it a USB gadget (the kernel-usb backend; ds4t-gadget loaded, a
// free UDC): usbhid then names the gamepad "<manufacturer> <product>".
// --fail-nth makes the Nth fault-injection point the create ioctl reaches fail
// (failslab, a debug kernel's /proc/thread-self/fail-nth), and prints how many
// were left: 0 means the failure was injected.
// --pad-mac names the fake pad to follow (its HID id is found through sysfs);
// without it nothing is followed and --inject sets the input instead. Dies, and
// with it the virtual controller, on SIGTERM or when --hold runs out.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "../../include/uapi/ds4t.h"

#define DEVICE_NAME "ds4t virtual test pad"

static const char *g_event_name = DEVICE_NAME;
#define TEST_BIT(bit, arr) ((arr)[(bit) / 8] & (1 << ((bit) % 8)))

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

static void sleep_ms(long ms)
{
	struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };

	nanosleep(&ts, NULL);
}

// The sysfs name ("0005:054C:05C4.000A") of the HID device whose uniq is mac.
static int hid_id_by_mac(const char *mac, char *out, size_t size)
{
	DIR *dir = opendir("/sys/bus/hid/devices");
	struct dirent *de;
	int found = -1;

	while (dir && (de = readdir(dir))) {
		char path[512], line[256];
		FILE *f;

		if (de->d_name[0] == '.')
			continue;
		snprintf(path, sizeof(path), "/sys/bus/hid/devices/%s/uevent", de->d_name);
		f = fopen(path, "r");
		while (f && fgets(line, sizeof(line), f)) {
			char want[64];

			snprintf(want, sizeof(want), "HID_UNIQ=%s\n", mac);
			if (!strcasecmp(line, want)) {
				if (strlen(de->d_name) < size) {
					strcpy(out, de->d_name);
					found = 0;
				}
			}
		}
		if (f)
			fclose(f);
	}
	if (dir)
		closedir(dir);
	return found;
}

// The event node of the virtual pad's gamepad, found by its device name.
static int open_event_node(char *path, size_t size)
{
	for (int attempt = 0; attempt < 100; attempt++) {
		for (int n = 0; n < 64; n++) {
			char name[128] = "";
			int fd;

			snprintf(path, size, "/dev/input/event%d", n);
			fd = open(path, O_RDWR | O_NONBLOCK);
			if (fd < 0)
				continue;
			if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) > 0 &&
			    !strcmp(name, g_event_name))
				return fd;
			close(fd);
		}
		sleep_ms(100);
	}
	return -1;
}

static int abs_value(int fd, int code)
{
	struct input_absinfo info;

	if (ioctl(fd, EVIOCGABS(code), &info))
		return -1;
	return info.value;
}

static void print_state(int fd, const char *tag)
{
	unsigned char keys[KEY_MAX / 8 + 1] = { 0 };

	ioctl(fd, EVIOCGKEY(sizeof(keys)), keys);
	printf("%s x=%d y=%d rx=%d ry=%d z=%d rz=%d hat_x=%d hat_y=%d south=%d\n",
	       tag, abs_value(fd, ABS_X), abs_value(fd, ABS_Y), abs_value(fd, ABS_RX),
	       abs_value(fd, ABS_RY), abs_value(fd, ABS_Z), abs_value(fd, ABS_RZ),
	       abs_value(fd, ABS_HAT0X), abs_value(fd, ABS_HAT0Y),
	       TEST_BIT(BTN_SOUTH, keys) ? 1 : 0);
	fflush(stdout);
}

// Prints each ABS_X event for `seconds`, starting with the current value.
static int watch_x(int fd, int seconds)
{
	struct timespec start, now;

	printf("abs_x=%d\n", abs_value(fd, ABS_X));
	clock_gettime(CLOCK_MONOTONIC, &start);
	do {
		struct pollfd p = { .fd = fd, .events = POLLIN };
		struct input_event e;

		if (poll(&p, 1, 100) > 0) {
			while (read(fd, &e, sizeof(e)) == sizeof(e)) {
				if (e.type == EV_ABS && e.code == ABS_X)
					printf("abs_x=%d\n", e.value);
			}
			fflush(stdout);
		}
		clock_gettime(CLOCK_MONOTONIC, &now);
	} while (!g_stop && now.tv_sec - start.tv_sec < seconds);
	return 0;
}

static int rumble(int fd)
{
	struct ff_effect effect = { .type = FF_RUMBLE, .id = -1 };
	struct input_event play = { .type = EV_FF, .value = 1 };

	effect.u.rumble.strong_magnitude = 0xffff;
	if (ioctl(fd, EVIOCSFF, &effect))
		return -1;
	play.code = effect.id;
	return write(fd, &play, sizeof(play)) == sizeof(play) ? 0 : -1;
}

// The hidraw node of the HID device whose uniq is mac.
static int open_hidraw_by_mac(const char *mac)
{
	char hid_id[64], path[512];
	DIR *dir;
	struct dirent *de;
	int fd = -1;

	if (hid_id_by_mac(mac, hid_id, sizeof(hid_id)))
		return -1;
	snprintf(path, sizeof(path), "/sys/bus/hid/devices/%s/hidraw", hid_id);
	dir = opendir(path);
	while (dir && (de = readdir(dir)) && fd < 0) {
		if (strncmp(de->d_name, "hidraw", 6))
			continue;
		snprintf(path, sizeof(path), "/dev/%s", de->d_name);
		fd = open(path, O_RDWR);
	}
	if (dir)
		closedir(dir);
	return fd;
}

// A rumble report for the emulated pad (strong motor 0x40, weak 0x20) in the
// layout a game uses for a USB or a Bluetooth pad.
static ssize_t hidraw_rumble(int fd, int type, int bluetooth)
{
	unsigned char r[78] = { 0 };
	size_t c = bluetooth ? 3 : 1, len;

	if (type == DS4T_VIRTUAL_DS4) {
		r[0] = bluetooth ? 0x11 : 0x05;
		r[c] = 0x01;		// motors valid
		r[c + 3] = 0x20;	// right, weak
		r[c + 4] = 0x40;	// left, strong
		len = bluetooth ? 78 : 32;
	} else {
		r[0] = bluetooth ? 0x31 : 0x02;
		r[c + 38] = 0x04;	// "compatible vibration 2", newer firmware
		r[c + 2] = 0x20;
		r[c + 3] = 0x40;
		len = bluetooth ? 78 : 48;
	}
	return write(fd, r, len);
}

// Arms /proc/thread-self/fail-nth; reads back what is left of it (0 once the
// failure has been injected, the count otherwise; it disarms itself).
static int set_fail_nth(int n)
{
	FILE *f = fopen("/proc/thread-self/fail-nth", "w");

	if (!f || fprintf(f, "%d", n) < 0 || fclose(f)) {
		printf("fail-nth unavailable\n");
		return -1;
	}
	return 0;
}

static int get_fail_nth(void)
{
	FILE *f = fopen("/proc/thread-self/fail-nth", "r");
	int n = -1;

	if (f) {
		if (fscanf(f, "%d", &n) != 1)
			n = -1;
		fclose(f);
	}
	return n;
}

// Prints "feature XX len=N data=HEX" or "feature XX failed: ERROR" per ID in
// the comma-separated hex list ids.
static void read_features(int fd, const char *ids)
{
	char *list = strdup(ids), *save = NULL;

	for (char *tok = strtok_r(list, ",", &save); tok;
	     tok = strtok_r(NULL, ",", &save)) {
		unsigned char buf[64] = { (unsigned char)strtoul(tok, NULL, 16) };
		int n = ioctl(fd, HIDIOCGFEATURE(sizeof(buf)), buf);

		if (n < 0) {
			printf("feature %02x failed: %s\n", buf[0], strerror(errno));
			continue;
		}
		printf("feature %02x len=%d data=", buf[0], n);
		for (int i = 0; i < n; i++)
			printf("%02x", buf[i]);
		printf("\n");
	}
	fflush(stdout);
	free(list);
}

// A DualShock 4 input report with a known state, for DS4T_IOC_CTL_INJECT.
static void fill_injected(struct ds4t_feature *report)
{
	memset(report, 0, sizeof(*report));
	report->len = 64;
	report->data[0] = 0x01;
	report->data[1] = 0x77;	// x
	report->data[2] = 0x88;	// y
	report->data[3] = 0x99;	// rx
	report->data[4] = 0xAA;	// ry
	report->data[5] = 0x28;	// cross, d-pad released
	report->data[8] = 0xBB;	// z
	report->data[9] = 0xCC;	// rz
}

int main(int argc, char **argv)
{
	struct ds4t_virtual_cfg cfg;
	struct ds4t_mac ignore;
	const char *mac = NULL, *pad_mac = NULL;
	int type = 0, inject = 0, do_rumble = 0, observe = 0, watch = 0, hold = 3, fd, ev;
	const char *hidraw_layout = NULL;
	const char *features = NULL;
	char usb_name[160];
	int usb = 0;
	int fail_nth = 0, ret, err;
	char path[64];

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--type") && i + 1 < argc)
			type = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--mac") && i + 1 < argc)
			mac = argv[++i];
		else if (!strcmp(argv[i], "--pad-mac") && i + 1 < argc)
			pad_mac = argv[++i];
		else if (!strcmp(argv[i], "--hold") && i + 1 < argc)
			hold = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--name") && i + 1 < argc)
			g_event_name = argv[++i];
		else if (!strcmp(argv[i], "--hidraw-rumble") && i + 1 < argc)
			hidraw_layout = argv[++i];
		else if (!strcmp(argv[i], "--features") && i + 1 < argc)
			features = argv[++i];
		else if (!strcmp(argv[i], "--fail-nth") && i + 1 < argc)
			fail_nth = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--usb"))
			usb = 1;
		else if (!strcmp(argv[i], "--watch"))
			watch = 1;
		else if (!strcmp(argv[i], "--observe"))
			observe = 1;
		else if (!strcmp(argv[i], "--inject"))
			inject = 1;
		else if (!strcmp(argv[i], "--rumble"))
			do_rumble = 1;
		else
			goto usage;
	}
	if (!observe && (!type || !mac))
		goto usage;

	signal(SIGTERM, on_signal);

	if (observe) {
		ev = open_event_node(path, sizeof(path));
		if (ev < 0) {
			printf("no event node called %s\n", g_event_name);
			return 1;
		}
		printf("evdev found\n");
		fflush(stdout);
		if (watch)
			return watch_x(ev, hold);
		for (int i = 0; i < hold * 10 && !g_stop; i++) {
			print_state(ev, "state");
			sleep_ms(100);
		}
		return 0;
	}

	// A killed predecessor may still be releasing the node (closing it waits
	// for its virtual controller to go): open is single, so retry a while.
	for (int i = 0; i < 50; i++) {
		fd = open("/dev/ds4-translator/control", O_RDWR);
		if (fd >= 0 || errno != EBUSY)
			break;
		sleep_ms(100);
	}
	if (fd < 0) {
		perror("open control");
		return 1;
	}
	memset(&ignore, 0, sizeof(ignore));
	snprintf(ignore.str, sizeof(ignore.str), "%s", mac);
	if (ioctl(fd, DS4T_IOC_CTL_SET_IGNORE_MAC, &ignore) ||
	    ioctl(fd, DS4T_IOC_CTL_ATTACH)) {
		perror("attach");
		return 1;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.type = type;
	if (usb) {
		cfg.flags = DS4T_VIRTUAL_F_USB;
		snprintf(usb_name, sizeof(usb_name), "%s %s",
			 type == DS4T_VIRTUAL_DS4 ? "Sony Computer Entertainment" :
			 "Sony Interactive Entertainment", DEVICE_NAME);
		g_event_name = usb_name;
	}
	snprintf(cfg.name, sizeof(cfg.name), "%s", DEVICE_NAME);
	snprintf(cfg.mac, sizeof(cfg.mac), "%s", mac);
	if (pad_mac && hid_id_by_mac(pad_mac, cfg.hid_id, sizeof(cfg.hid_id))) {
		fprintf(stderr, "no HID device with uniq %s\n", pad_mac);
		return 1;
	}
	if (fail_nth && set_fail_nth(fail_nth))
		return 1;
	ret = ioctl(fd, DS4T_IOC_CTL_CREATE_VIRTUAL, &cfg);
	err = errno;
	if (fail_nth)
		printf("fail-nth left=%d\n", get_fail_nth());
	if (ret) {
		printf("create failed: %s\n", strerror(err));
		return 1;
	}
	printf("created following=%s\n", cfg.hid_id[0] ? cfg.hid_id : "-");

	ev = open_event_node(path, sizeof(path));
	if (ev < 0) {
		printf("no event node for the virtual pad\n");
		return 1;
	}
	printf("evdev found\n");

	if (inject) {
		struct ds4t_feature report;

		fill_injected(&report);
		if (ioctl(fd, DS4T_IOC_CTL_INJECT, &report)) {
			printf("inject failed: %s\n", strerror(errno));
			return 1;
		}
	}
	sleep_ms(500);	// the fake pad streams at 100 Hz
	print_state(ev, "state");

	if (do_rumble) {
		printf("rumble %s\n", rumble(ev) ? "failed" : "sent");
		fflush(stdout);
	}
	if (hidraw_layout) {
		int raw = open_hidraw_by_mac(mac);

		printf("hidraw rumble (%s) %s\n", hidraw_layout,
		       raw >= 0 && hidraw_rumble(raw, type, !strcmp(hidraw_layout, "bt")) > 0 ?
		       "sent" : "failed");
		fflush(stdout);
		if (raw >= 0)
			close(raw);
	}

	if (features) {
		int raw = open_hidraw_by_mac(mac);

		if (raw < 0)
			printf("no hidraw node for the virtual pad\n");
		else
			read_features(raw, features);
		if (raw >= 0)
			close(raw);
	}

	for (int i = 0; i < hold * 10 && !g_stop; i++)
		sleep_ms(100);
	// What the game still sees at the end, e.g. after the pad was unplugged.
	print_state(ev, "final");
	return 0;

usage:
	fprintf(stderr, "usage: virtual-check --type 1|2 --mac MAC [--pad-mac MAC] "
			"[--inject] [--rumble] [--hidraw-rumble usb|bt] [--features ID,..] [--usb] "
			"[--hold S]\n"
			"       virtual-check --observe --name NAME [--hold S] [--watch]\n");
	return 2;
}
