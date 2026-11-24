#ifndef DRIVER_TRANSPORT_H
#define DRIVER_TRANSPORT_H
// Transport for the "driver" hide method: the hid-ds4-translator kernel
// module (kernel/, see docs/kernel-driver.md) claims the physical pad before
// any other driver, so nothing else ever creates a hidraw, input, battery or
// LED device for it, and hands its reports to this daemon through
// /dev/ds4-translator/<hid_id>.
//
// Unlike usb-hid-transport.h and hid-bpf-transport.h there is nothing to pump:
// the node already has the contract the rest of main.cpp expects of a
// physical-pad fd -- pollable, one input report per read(), one output report
// per write(), and an error/POLLHUP once the pad is gone -- so the fd is
// returned as is and closed with a plain close().
#include <chrono>
#include <string>

// True while the kernel module is loaded. Callers use it to fall back to the
// unbind method without paying driver_transport_open()'s wait for a node
// that will never appear.
bool driver_transport_available();

// Opens the node for hid_id (the HID device's sysfs name, as found by the
// uevent monitor), waiting up to `wait` for it: the HID-bus "add" uevent
// arrives before the module's probe has created the node. Checks that the
// module speaks this build's ABI. Returns a non-blocking fd, or -1 on
// failure (module not bound to this pad, node busy, ABI mismatch).
int driver_transport_open(const std::string& hid_id,
                          std::chrono::milliseconds wait = std::chrono::milliseconds(2000));

#endif // DRIVER_TRANSPORT_H
