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

void report_abi_mismatch(unsigned module_abi) {
    std::cerr << "driver: kernel module speaks ABI " << module_abi
              << ", this daemon speaks " << DS4T_ABI_VERSION
              << " -- rebuild/reinstall the hid-ds4-translator module." << std::endl;
}

bool abi_matches(int fd, const std::string& node) {
    struct ds4t_info info;
    memset(&info, 0, sizeof(info));
    if (ioctl(fd, DS4T_IOC_GET_INFO, &info) != 0) {
        std::cerr << "driver: " << node << " is not a ds4-translator node: "
                  << strerror(errno) << std::endl;
        return false;
    }
    if (info.abi_version != DS4T_ABI_VERSION) {
        report_abi_mismatch(info.abi_version);
        return false;
    }
    return true;
}

} // namespace

int driver_transport_attach() {
    if (!driver_transport_available()) {
        std::cerr << "driver: hid_ds4_translator is not loaded." << std::endl;
        return -1;
    }
    const std::string node = ds4paths::driver_dev_dir() + "/control";
    const int fd = open(node.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        std::cerr << "driver: cannot attach through " << node << ": "
                  << (errno == EBUSY ? "another daemon is already attached" : strerror(errno))
                  << std::endl;
        return -1;
    }
    __u32 abi = 0;
    if (ioctl(fd, DS4T_IOC_CTL_GET_ABI, &abi) != 0) {
        std::cerr << "driver: " << node << " is not a ds4-translator control node: "
                  << strerror(errno) << std::endl;
        close(fd);
        return -1;
    }
    if (abi != DS4T_ABI_VERSION) {
        report_abi_mismatch(abi);
        close(fd);
        return -1;
    }
    return fd;
}

bool driver_transport_claim(int control_fd) {
    if (ioctl(control_fd, DS4T_IOC_CTL_ATTACH) != 0) {
        std::cerr << "driver: the module refused to claim pads: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

void driver_transport_detach(int control_fd) {
    if (control_fd >= 0) {
        close(control_fd);
    }
}

bool driver_transport_set_ignore_mac(int control_fd, const std::string& mac) {
    struct ds4t_mac arg;
    memset(&arg, 0, sizeof(arg));
    if (mac.size() >= sizeof(arg.str)) {
        std::cerr << "driver: MAC '" << mac << "' is too long" << std::endl;
        return false;
    }
    memcpy(arg.str, mac.c_str(), mac.size());
    if (ioctl(control_fd, DS4T_IOC_CTL_SET_IGNORE_MAC, &arg) != 0) {
        std::cerr << "driver: cannot tell the module which MAC to ignore: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool driver_transport_get_calibration(int pad_fd, std::vector<uint8_t>& report) {
    struct ds4t_feature feat;
    memset(&feat, 0, sizeof(feat));
    if (ioctl(pad_fd, DS4T_IOC_GET_CALIBRATION, &feat) != 0) {
        if (errno != ENODATA) {
            std::cerr << "driver: cannot read the calibration report: " << strerror(errno) << std::endl;
        }
        return false;
    }
    if (feat.len == 0 || feat.len > sizeof(feat.data)) {
        return false;
    }
    report.assign(feat.data, feat.data + feat.len);
    return true;
}

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
