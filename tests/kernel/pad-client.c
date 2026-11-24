// SPDX-License-Identifier: GPL-2.0-only
//
// Minimal user of /dev/ds4-translator/<hid_id>, standing in for the daemon in
// tests/kernel/test-binder.sh. One command per run:
//
//   pad-client NODE info          print the DS4T_IOC_GET_INFO fields
//   pad-client NODE read N        read N input reports, print id/len/seq
//   pad-client NODE write HEX     write one output report (e.g. 05ff00...)
//   pad-client NODE hold          read until the pad goes away, print how
//   pad-client NODE feature-get ID LEN   GET_FEATURE; prints "len=N hex"
//   pad-client NODE feature-set HEX      SET_FEATURE (first byte is the ID)
//   pad-client NODE calibration          GET_CALIBRATION; prints "len=N hex"
//   pad-client HIDRAW hidraw-feature ID LEN
//                                 HIDIOCGFEATURE on an ordinary hidraw node
//   pad-client NODE stress        hammer the node (info, write, feature get,
//                                 read) until the pad goes away; print how
//   pad-client CONTROL control [MAC]
//                                 attach through the control node (optionally
//                                 naming the emulated pad's MAC), then stay
//                                 attached until killed

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../../include/uapi/ds4t.h"

static int do_info(int fd)
{
	struct ds4t_info info;

	if (ioctl(fd, DS4T_IOC_GET_INFO, &info)) {
		perror("pad-client: GET_INFO");
		return 1;
	}
	printf("abi=%u bus=%u vendor=%04x product=%04x uniq=%s hid_id=%s\n",
	       info.abi_version, info.bus, info.vendor, info.product, info.uniq,
	       info.hid_id);
	return 0;
}

static int do_read(int fd, int count)
{
	unsigned char buf[DS4T_MAX_REPORT];

	for (int i = 0; i < count; i++) {
		ssize_t n = read(fd, buf, sizeof(buf));

		if (n < 0) {
			perror("pad-client: read");
			return 1;
		}
		printf("report id=%02x len=%zd seq=%u\n", buf[0], n, buf[1]);
	}
	return 0;
}

static int do_write(int fd, const char *hex)
{
	unsigned char buf[DS4T_MAX_REPORT];
	size_t len = strlen(hex) / 2;

	if (!len || len > sizeof(buf))
		return 2;
	for (size_t i = 0; i < len; i++) {
		unsigned int byte;

		if (sscanf(hex + 2 * i, "%2x", &byte) != 1)
			return 2;
		buf[i] = byte;
	}
	ssize_t n = write(fd, buf, len);

	if (n != (ssize_t)len) {
		perror("pad-client: write");
		return 1;
	}
	printf("wrote %zd\n", n);
	return 0;
}

// Blocks like the daemon's main loop: poll, read, until the pad goes away.
static int do_hold(int fd)
{
	unsigned char buf[DS4T_MAX_REPORT];
	struct pollfd pfd = { .fd = fd, .events = POLLIN };

	puts("holding");
	fflush(stdout);
	for (;;) {
		if (poll(&pfd, 1, -1) < 0)
			return 1;
		if (pfd.revents & POLLIN) {
			if (read(fd, buf, sizeof(buf)) < 0) {
				printf("read error: %s\n", strerror(errno));
				return 0;
			}
		} else if (pfd.revents & (POLLHUP | POLLERR)) {
			printf("poll hangup revents=%#x\n", pfd.revents);
			return 0;
		}
	}
}

static void print_feature(const struct ds4t_feature *feat)
{
	printf("len=%u ", feat->len);
	for (unsigned int i = 0; i < feat->len && i < sizeof(feat->data); i++)
		printf("%02x", feat->data[i]);
	putchar('\n');
}

static int do_feature_get(int fd, const char *id, const char *len)
{
	struct ds4t_feature feat = { .len = (__u16)atoi(len) };

	feat.data[0] = (__u8)strtoul(id, NULL, 16);
	if (ioctl(fd, DS4T_IOC_GET_FEATURE, &feat)) {
		printf("GET_FEATURE: %s\n", strerror(errno));
		return 1;
	}
	print_feature(&feat);
	return 0;
}

static int do_feature_set(int fd, const char *hex)
{
	struct ds4t_feature feat = { .len = (__u16)(strlen(hex) / 2) };

	for (unsigned int i = 0; i < feat.len && i < sizeof(feat.data); i++) {
		unsigned int byte;

		if (sscanf(hex + 2 * i, "%2x", &byte) != 1)
			return 2;
		feat.data[i] = byte;
	}
	if (ioctl(fd, DS4T_IOC_SET_FEATURE, &feat)) {
		printf("SET_FEATURE: %s\n", strerror(errno));
		return 1;
	}
	puts("set ok");
	return 0;
}

static int do_calibration(int fd)
{
	struct ds4t_feature feat;

	if (ioctl(fd, DS4T_IOC_GET_CALIBRATION, &feat)) {
		printf("GET_CALIBRATION: %s\n", strerror(errno));
		return 1;
	}
	print_feature(&feat);
	return 0;
}

static int do_hidraw_feature(int fd, const char *id, const char *len)
{
	unsigned char buf[DS4T_MAX_REPORT] = { (unsigned char)strtoul(id, NULL, 16) };
	int n = ioctl(fd, HIDIOCGFEATURE(atoi(len)), buf);

	if (n < 0) {
		printf("HIDIOCGFEATURE: %s\n", strerror(errno));
		return 1;
	}
	printf("len=%d ", n);
	for (int i = 0; i < n; i++)
		printf("%02x", buf[i]);
	putchar('\n');
	return 0;
}

// What a daemon that is unlucky with timing does: keep using every operation
// of the node while the pad is being unplugged.
static int do_stress(int fd)
{
	unsigned char buf[DS4T_MAX_REPORT];
	unsigned char out[32] = { 0x05, 0xff };
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	unsigned long ops = 0;

	puts("stressing");
	fflush(stdout);
	for (;;) {
		struct ds4t_feature feat = { .len = 37 };
		struct ds4t_info info;
		int err = 0;

		feat.data[0] = 0x02;
		if (ioctl(fd, DS4T_IOC_GET_INFO, &info))
			err = errno;
		else if (write(fd, out, sizeof(out)) < 0)
			err = errno;
		else if (ioctl(fd, DS4T_IOC_GET_FEATURE, &feat))
			err = errno;
		else if (poll(&pfd, 1, 5) > 0 && (pfd.revents & POLLIN) &&
			 read(fd, buf, sizeof(buf)) < 0 && errno != EAGAIN)
			err = errno;
		else if (pfd.revents & (POLLHUP | POLLERR))
			err = ENODEV;
		if (err) {
			printf("stress: %lu ops, ended: %s\n", ops, strerror(err));
			return 0;
		}
		ops++;
	}
}

static int do_control(int fd, const char *mac)
{
	struct ds4t_mac arg = { { 0 } };
	__u32 abi = 0;

	if (ioctl(fd, DS4T_IOC_CTL_GET_ABI, &abi)) {
		perror("pad-client: CTL_GET_ABI");
		return 1;
	}
	if (mac) {
		snprintf(arg.str, sizeof(arg.str), "%s", mac);
		if (ioctl(fd, DS4T_IOC_CTL_SET_IGNORE_MAC, &arg)) {
			perror("pad-client: CTL_SET_IGNORE_MAC");
			return 1;
		}
	}
	if (ioctl(fd, DS4T_IOC_CTL_ATTACH)) {
		perror("pad-client: CTL_ATTACH");
		return 1;
	}
	printf("attached abi=%u\n", abi);
	fflush(stdout);
	for (;;)
		pause();
}

int main(int argc, char **argv)
{
	int fd, ret;

	if (argc < 3) {
		fprintf(stderr, "usage: pad-client NODE info|read N|write HEX|hold|feature-get ID LEN|"
			"feature-set HEX|calibration|hidraw-feature ID LEN|stress|control [MAC]\n");
		return 2;
	}
	fd = open(argv[1], O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("pad-client: open");
		return 1;
	}

	if (!strcmp(argv[2], "info"))
		ret = do_info(fd);
	else if (!strcmp(argv[2], "read") && argc == 4)
		ret = do_read(fd, atoi(argv[3]));
	else if (!strcmp(argv[2], "write") && argc == 4)
		ret = do_write(fd, argv[3]);
	else if (!strcmp(argv[2], "hold"))
		ret = do_hold(fd);
	else if (!strcmp(argv[2], "feature-get") && argc == 5)
		ret = do_feature_get(fd, argv[3], argv[4]);
	else if (!strcmp(argv[2], "feature-set") && argc == 4)
		ret = do_feature_set(fd, argv[3]);
	else if (!strcmp(argv[2], "hidraw-feature") && argc == 5)
		ret = do_hidraw_feature(fd, argv[3], argv[4]);
	else if (!strcmp(argv[2], "calibration"))
		ret = do_calibration(fd);
	else if (!strcmp(argv[2], "stress"))
		ret = do_stress(fd);
	else if (!strcmp(argv[2], "control") && argc <= 4)
		ret = do_control(fd, argc == 4 ? argv[3] : NULL);
	else
		ret = 2;
	close(fd);
	return ret;
}
