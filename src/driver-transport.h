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
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

// The module is inert until a daemon attaches through its control node: it
// takes over the pads other drivers hold only once told to, and closing the
// control fd -- or this process dying, however it dies -- hands every pad
// back. That is why the daemon must keep the fd open exactly as long as it
// wants pads held.
//
// Opens the control node and checks the module's ABI. The module still claims
// nothing at this point. Returns the control fd, or -1 (module not loaded,
// another daemon attached, ABI mismatch).
int driver_transport_attach();

// Names the emulated controller's MAC so the module never claims it. Must be
// done before driver_transport_claim(), and kept current afterwards;
// replaces any earlier value.
bool driver_transport_set_ignore_mac(int control_fd, const std::string& mac);

// Makes the module start claiming pads, taking over the ones other drivers
// hold now. Blocks while that happens.
bool driver_transport_claim(int control_fd);

// Hands every pad back to its regular driver.
void driver_transport_detach(int control_fd);

// The module's own virtual controller (the "kernel" emulation backend): it
// translates the claimed pad's input itself, relays rumble/LED back, and goes
// away when the control fd closes. `dualsense` picks the pad type, `mac` is the
// pad's uniq (and what driver_transport_set_ignore_mac() was given), `hid_id`
// the claimed pad it follows, or empty for none. False with errno-style
// diagnostics logged (module too old, a controller already exists, no such
// pad).
bool driver_transport_create_virtual(int control_fd, bool dualsense, const std::string& name,
                                     const std::string& mac, const std::string& hid_id);
void driver_transport_destroy_virtual(int control_fd);
// Follows another claimed pad, or none when hid_id is empty.
bool driver_transport_link_virtual(int control_fd, const std::string& hid_id);
// Sets the virtual controller's input from a 64-byte DS4 USB input report
// (0x01), for when no physical pad drives it.
bool driver_transport_inject(int control_fd, const uint8_t* report, size_t len);

// True if the module has created the node for hid_id, i.e. it has claimed that
// pad and the daemon could open it now.
bool driver_transport_node_exists(const std::string& hid_id);

// Reads the calibration report the module cached when it claimed the pad
// (0x02, 37 bytes, over USB; 0x05, 41 bytes with CRC, over Bluetooth). False
// if the module has none or the fd is not a pad node.
bool driver_transport_get_calibration(int pad_fd, std::vector<uint8_t>& report);

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
