// Controls for the fake libusb in libusb_stub.cpp. One fake DualShock 4 is
// present on the fake bus; tests flip these flags to make it misbehave.
#ifndef TESTS_STUBS_LIBUSB_STUB_H
#define TESTS_STUBS_LIBUSB_STUB_H

#include <atomic>

namespace fakeusb {

struct Controls {
    // The device is unplugged: enumeration no longer lists it and every
    // transfer fails with NO_DEVICE.
    std::atomic<bool> gone{false};
    // Another driver took the interface back (kernel_driver_active == 1).
    std::atomic<bool> reclaimed{false};
    // Transfers never complete on their own, not even when the device is
    // unplugged: they only end by cancellation or (synchronous API) by the
    // handle being closed. This is the hang the transport's watchdog exists
    // for; enumeration still reflects `gone`.
    std::atomic<bool> wedge{false};
    // Complete IN transfers with a 64-byte report instead of timing out.
    std::atomic<bool> feed{false};
};

extern Controls controls;

// Contract violations the real libusb would turn into undefined behaviour:
// using a handle after libusb_close(), closing twice, or closing a handle
// that still has a transfer in flight.
extern std::atomic<int> violations;

// Calls to the teardown helpers, to check the normal path still hands the
// interface back to the kernel driver.
extern std::atomic<int> release_calls;
extern std::atomic<int> attach_calls;

// Number of handles opened and not yet closed.
int open_handles();

void reset();

} // namespace fakeusb

#endif // TESTS_STUBS_LIBUSB_STUB_H
