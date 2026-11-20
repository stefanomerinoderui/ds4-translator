// libFuzzer target for the uevent reader: every input is delivered as one
// datagram through a socketpair, exactly as the kernel would deliver it.
#include <cstddef>
#include <cstdint>
#include <string>

#include <sys/socket.h>
#include <unistd.h>

#include "hid-unbind-detect.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) != 0) return 0;
    if (size > 0 && send(fds[0], data, size, 0) == static_cast<ssize_t>(size)) {
        std::string id;
        hid_uevent_monitor_read(fds[1], id);
    }
    close(fds[0]);
    close(fds[1]);
    return 0;
}
