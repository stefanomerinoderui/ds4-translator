// A fake libusb with one DualShock 4 on the bus, enough for
// src/usb-hid-transport.cpp to run without hardware. Linked into the test
// binary in place of -lusb-1.0 (the same trick as libudev_stub.c).
//
// Its job is to notice the thread-safety mistakes real libusb leaves
// undefined: any call on a closed handle, a double close, or a close while a
// transfer is still in flight is counted in fakeusb::violations.
#include "libusb_stub.h"

#include <libusb-1.0/libusb.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

struct libusb_context {};
struct libusb_device {};
struct libusb_device_handle {
    int pending = 0; // transfers submitted and not yet completed
};

namespace fakeusb {

Controls controls;
std::atomic<int> violations{0};
std::atomic<int> release_calls{0};
std::atomic<int> attach_calls{0};

namespace {

std::mutex g_mutex;
std::set<libusb_device_handle *> g_live;
std::vector<libusb_transfer *> g_pending;
std::set<libusb_transfer *> g_cancelled;
libusb_device g_device;

using Clock = std::chrono::steady_clock;
std::vector<Clock::time_point> g_deadlines; // parallel to g_pending

constexpr uint16_t kVendorSony = 0x054c;
constexpr uint16_t kProductDs4 = 0x05c4;

// True if `handle` may be used. Caller holds g_mutex.
bool live_locked(libusb_device_handle *handle) {
    if (g_live.count(handle) != 0) return true;
    violations++;
    return false;
}

void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

} // namespace

int open_handles() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return (int)g_live.size();
}

void reset() {
    controls.gone = false;
    controls.reclaimed = false;
    controls.wedge = false;
    controls.feed = false;
    violations = 0;
    release_calls = 0;
    attach_calls = 0;
}

} // namespace fakeusb

using namespace fakeusb;

extern "C" {

int libusb_init(libusb_context **ctx) {
    *ctx = new libusb_context();
    return 0;
}

void libusb_exit(libusb_context *ctx) { delete ctx; }

ssize_t libusb_get_device_list(libusb_context *, libusb_device ***list) {
    const bool present = !controls.gone.load();
    auto **array = (libusb_device **)calloc(2, sizeof(libusb_device *));
    if (present) array[0] = &g_device;
    *list = array;
    return present ? 1 : 0;
}

void libusb_free_device_list(libusb_device **list, int) { free(list); }

uint8_t libusb_get_bus_number(libusb_device *) { return 1; }
uint8_t libusb_get_device_address(libusb_device *) { return 5; }
// No port path: the transport then skips its "is this our own gadget" sysfs lookup.
int libusb_get_port_numbers(libusb_device *, uint8_t *, int) { return 0; }

int libusb_get_device_descriptor(libusb_device *, libusb_device_descriptor *desc) {
    memset(desc, 0, sizeof(*desc));
    desc->idVendor = kVendorSony;
    desc->idProduct = kProductDs4;
    return 0;
}

int libusb_get_active_config_descriptor(libusb_device *, libusb_config_descriptor **config) {
    static libusb_endpoint_descriptor endpoints[2];
    static libusb_interface_descriptor altsetting;
    static libusb_interface interface;
    static libusb_config_descriptor descriptor;
    endpoints[0].bEndpointAddress = LIBUSB_ENDPOINT_IN | 1;
    endpoints[0].bmAttributes = LIBUSB_TRANSFER_TYPE_INTERRUPT;
    endpoints[1].bEndpointAddress = LIBUSB_ENDPOINT_OUT | 1;
    endpoints[1].bmAttributes = LIBUSB_TRANSFER_TYPE_INTERRUPT;
    altsetting.bInterfaceNumber = 0;
    altsetting.bInterfaceClass = LIBUSB_CLASS_HID;
    altsetting.bNumEndpoints = 2;
    altsetting.endpoint = endpoints;
    interface.altsetting = &altsetting;
    interface.num_altsetting = 1;
    descriptor.bNumInterfaces = 1;
    descriptor.interface = &interface;
    *config = &descriptor;
    return 0;
}

void libusb_free_config_descriptor(libusb_config_descriptor *) {}

int libusb_open(libusb_device *, libusb_device_handle **handle) {
    auto *opened = new libusb_device_handle();
    std::lock_guard<std::mutex> lock(g_mutex);
    g_live.insert(opened);
    *handle = opened;
    return 0;
}

void libusb_close(libusb_device_handle *handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_live.count(handle) == 0) {
        violations++; // double close
        return;
    }
    if (handle->pending > 0) {
        violations++; // closed under an in-flight transfer
    }
    g_live.erase(handle);
    // Deliberately freed: a later use of the pointer is a real heap
    // use-after-free that ASan reports on top of the counter.
    delete handle;
}

int libusb_detach_kernel_driver(libusb_device_handle *handle, int) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return live_locked(handle) ? LIBUSB_ERROR_NOT_FOUND : LIBUSB_ERROR_NO_DEVICE;
}

int libusb_claim_interface(libusb_device_handle *handle, int) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return live_locked(handle) ? 0 : LIBUSB_ERROR_NO_DEVICE;
}

int libusb_release_interface(libusb_device_handle *handle, int) {
    std::lock_guard<std::mutex> lock(g_mutex);
    release_calls++;
    return live_locked(handle) ? 0 : LIBUSB_ERROR_NO_DEVICE;
}

int libusb_kernel_driver_active(libusb_device_handle *handle, int) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!live_locked(handle)) return LIBUSB_ERROR_NO_DEVICE;
    return controls.reclaimed.load() ? 1 : 0;
}

int libusb_attach_kernel_driver(libusb_device_handle *handle, int) {
    std::lock_guard<std::mutex> lock(g_mutex);
    attach_calls++;
    return live_locked(handle) ? 0 : LIBUSB_ERROR_NO_DEVICE;
}

const char *libusb_error_name(int code) {
    switch (code) {
    case LIBUSB_ERROR_IO: return "LIBUSB_ERROR_IO";
    case LIBUSB_ERROR_NO_DEVICE: return "LIBUSB_ERROR_NO_DEVICE";
    case LIBUSB_ERROR_TIMEOUT: return "LIBUSB_ERROR_TIMEOUT";
    default: return "LIBUSB_ERROR_OTHER";
    }
}

// ---- asynchronous transfers (what the transport uses)

libusb_transfer *libusb_alloc_transfer(int) {
    return (libusb_transfer *)calloc(1, sizeof(libusb_transfer));
}

void libusb_free_transfer(libusb_transfer *transfer) { free(transfer); }

int libusb_submit_transfer(libusb_transfer *transfer) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!live_locked(transfer->dev_handle)) return LIBUSB_ERROR_NO_DEVICE;
    if (controls.gone.load() && !controls.wedge.load()) return LIBUSB_ERROR_NO_DEVICE;
    transfer->dev_handle->pending++;
    g_pending.push_back(transfer);
    g_deadlines.push_back(Clock::now() + std::chrono::milliseconds(transfer->timeout));
    return 0;
}

int libusb_cancel_transfer(libusb_transfer *transfer) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (libusb_transfer *pending : g_pending) {
        if (pending == transfer) {
            g_cancelled.insert(transfer);
            return 0;
        }
    }
    return LIBUSB_ERROR_NOT_FOUND;
}

int libusb_handle_events_timeout_completed(libusb_context *, struct timeval *tv, int *completed) {
    const auto give_up = Clock::now() + std::chrono::seconds(tv->tv_sec) + std::chrono::microseconds(tv->tv_usec);
    do {
        libusb_transfer *done = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            const auto now = Clock::now();
            for (size_t i = 0; i < g_pending.size() && !done; ++i) {
                libusb_transfer *transfer = g_pending[i];
                const bool is_in = (transfer->endpoint & LIBUSB_ENDPOINT_IN) != 0;
                if (g_cancelled.count(transfer) != 0) {
                    transfer->status = LIBUSB_TRANSFER_CANCELLED;
                } else if (controls.gone.load() && !controls.wedge.load()) {
                    transfer->status = LIBUSB_TRANSFER_NO_DEVICE;
                } else if (is_in && controls.feed.load()) {
                    memset(transfer->buffer, 0x11, 64);
                    transfer->actual_length = 64;
                    transfer->status = LIBUSB_TRANSFER_COMPLETED;
                } else if (!controls.wedge.load() && now >= g_deadlines[i]) {
                    transfer->status = LIBUSB_TRANSFER_TIMED_OUT;
                } else {
                    continue;
                }
                done = transfer;
                transfer->dev_handle->pending--;
                g_cancelled.erase(transfer);
                g_pending.erase(g_pending.begin() + (long)i);
                g_deadlines.erase(g_deadlines.begin() + (long)i);
            }
        }
        if (done) {
            done->callback(done);
            return 0;
        }
        sleep_ms(1);
    } while (Clock::now() < give_up && !(completed && *completed));
    return 0;
}

// ---- synchronous transfer (what the transport used before the fix)

int libusb_interrupt_transfer(libusb_device_handle *handle, unsigned char endpoint, unsigned char *data,
                              int, int *transferred, unsigned int timeout) {
    std::unique_lock<std::mutex> lock(g_mutex);
    if (!live_locked(handle)) return LIBUSB_ERROR_NO_DEVICE;
    handle->pending++;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout);
    for (;;) {
        if (g_live.count(handle) == 0) {
            // Closed under us: real libusb makes the call return, and the
            // caller is left holding a dangling handle (no pending-- here:
            // the handle is gone).
            return LIBUSB_ERROR_NO_DEVICE;
        }
        int rc = 0;
        bool finished = true;
        if (controls.gone.load() && !controls.wedge.load()) {
            rc = LIBUSB_ERROR_NO_DEVICE;
        } else if ((endpoint & LIBUSB_ENDPOINT_IN) && controls.feed.load()) {
            memset(data, 0x11, 64);
            *transferred = 64;
            rc = LIBUSB_SUCCESS;
        } else if (!controls.wedge.load() && Clock::now() >= deadline) {
            rc = LIBUSB_ERROR_TIMEOUT;
        } else {
            finished = false;
        }
        if (finished) {
            handle->pending--;
            return rc;
        }
        lock.unlock();
        sleep_ms(1);
        lock.lock();
    }
}

} // extern "C"
