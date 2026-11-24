#include "driver-transport.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <thread>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "paths.h"
#include "../include/uapi/ds4t.h"

namespace {

constexpr auto kRetryInterval = std::chrono::milliseconds(50);

bool abi_matches(int fd, const std::string& node) {
    struct ds4t_info info;
    memset(&info, 0, sizeof(info));
    if (ioctl(fd, DS4T_IOC_GET_INFO, &info) != 0) {
        std::cerr << "driver: " << node << " is not a ds4-translator node: "
                  << strerror(errno) << std::endl;
        return false;
    }
    if (info.abi_version != DS4T_ABI_VERSION) {
        std::cerr << "driver: kernel module speaks ABI " << info.abi_version
                  << ", this daemon speaks " << DS4T_ABI_VERSION
                  << " -- rebuild/reinstall the hid-ds4-translator module." << std::endl;
        return false;
    }
    return true;
}

} // namespace

bool driver_transport_available() {
    struct stat st;
    return stat(ds4paths::driver_module_dir().c_str(), &st) == 0;
}

int driver_transport_open(const std::string& hid_id, std::chrono::milliseconds wait) {
    // hid_id becomes a path component: reject anything that could leave the
    // device directory rather than trusting the uevent parser upstream.
    if (hid_id.empty() || hid_id.find('/') != std::string::npos || hid_id[0] == '.') {
        std::cerr << "driver: refusing suspicious hid id '" << hid_id << "'" << std::endl;
        return -1;
    }

    const std::string node = ds4paths::driver_dev_dir() + "/" + hid_id;
    const auto give_up = std::chrono::steady_clock::now() + wait;
    for (;;) {
        const int fd = open(node.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            if (abi_matches(fd, node)) {
                return fd;
            }
            close(fd);
            return -1;
        }
        // Only a missing node is worth waiting for (probe not finished yet);
        // EBUSY or EACCES will not fix themselves.
        if (errno != ENOENT) {
            std::cerr << "driver: cannot open " << node << ": " << strerror(errno) << std::endl;
            return -1;
        }
        if (std::chrono::steady_clock::now() >= give_up) {
            std::cerr << "driver: " << node << " did not appear -- the module is not bound to this pad."
                      << std::endl;
            return -1;
        }
        std::this_thread::sleep_for(kRetryInterval);
    }
}
