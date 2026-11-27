// SPDX-License-Identifier: GPL-2.0-only
//
// Drives src/driver-transport.cpp (the daemon's side) against the real kernel
// module, the way main.cpp does: attach (naming the emulated pad's MAC before
// claiming anything), open by hid_id, poll, read reports, write an output
// report, and notice the pad going away. Used by test-binder.sh.
//
//   transport-check HID_ID [IGNORE_MAC]

#include <chrono>
#include <cstdio>
#include <iostream>

#include <poll.h>
#include <unistd.h>

#include "driver-transport.h"

namespace {

constexpr int kReports = 5;
constexpr int kPollMs = 10000;

int read_reports(int fd) {
    for (int i = 0; i < kReports; i++) {
        pollfd pfd{fd, POLLIN, 0};
        if (poll(&pfd, 1, kPollMs) <= 0 || !(pfd.revents & POLLIN)) {
            std::cout << "no report " << i << std::endl;
            return 1;
        }
        unsigned char buf[128];
        const ssize_t n = read(fd, buf, sizeof(buf));
        if (n != 64 || buf[0] != 0x01) {
            std::cout << "unexpected report: " << n << " bytes, id " << int(buf[0]) << std::endl;
            return 1;
        }
    }
    std::cout << "reports ok" << std::endl;
    return 0;
}

// What the pacer writes: report 0x05, 32 bytes.
int write_output(int fd) {
    unsigned char out[32] = {0x05, 0xff};
    if (write(fd, out, sizeof(out)) != static_cast<ssize_t>(sizeof(out))) {
        std::perror("write");
        return 1;
    }
    std::cout << "write ok" << std::endl;
    return 0;
}

// main.cpp treats a failed read() or POLLHUP/POLLERR as the pad being gone.
int wait_for_disconnect(int fd) {
    std::cout << "waiting for disconnect" << std::endl;
    for (;;) {
        pollfd pfd{fd, POLLIN, 0};
        if (poll(&pfd, 1, kPollMs) <= 0) {
            std::cout << "timed out" << std::endl;
            return 1;
        }
        unsigned char buf[128];
        if ((pfd.revents & POLLIN) && read(fd, buf, sizeof(buf)) < 0) {
            std::cout << "disconnect detected (read)" << std::endl;
            return 0;
        }
        if (pfd.revents & (POLLHUP | POLLERR)) {
            std::cout << "disconnect detected (poll)" << std::endl;
            return 0;
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: transport-check HID_ID [IGNORE_MAC]" << std::endl;
        return 2;
    }
    const int control_fd = driver_transport_attach();
    if (control_fd < 0) {
        std::cout << "attach failed" << std::endl;
        return 1;
    }
    if ((argc == 3 && !driver_transport_set_ignore_mac(control_fd, argv[2])) ||
        !driver_transport_claim(control_fd)) {
        std::cout << "claim failed" << std::endl;
        return 1;
    }
    const int fd = driver_transport_open(argv[1]);
    if (fd < 0) {
        std::cout << "open failed" << std::endl;
        return 1;
    }
    const int rc = read_reports(fd) || write_output(fd) || wait_for_disconnect(fd);
    close(fd);
    driver_transport_detach(control_fd);
    return rc;
}
