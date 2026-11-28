#ifndef DS4_PAIRING_H
#define DS4_PAIRING_H

// Cable pairing of a physical DualShock 4: what BlueZ's sixaxis plugin does
// when a pad is plugged in, but done by the daemon, because under the
// unbind/unlink hide methods the plugin never sees the pad's hidraw node.
//
// Two halves, both needed:
//   * feature report 0x13 tells the pad which host MAC and link key to use;
//   * a BlueZ device entry, /var/lib/bluetooth/<adapter>/<pad MAC>/info, holds
//     the same link key so bluetoothd accepts the pad when it connects.
// bluetoothd reads those files only at startup, so it has to be restarted
// afterwards (the daemon does not do that itself: it would drop every
// Bluetooth device).
//
// Free of real I/O except the BlueZ state directory, which tests point at a
// temporary one; the pad is a FeatureIo.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "calibration.h" // FeatureIo

constexpr uint8_t DS4_PAIRING_SET_ID = 0x13;
constexpr size_t DS4_PAIRING_SET_SIZE = 23;
constexpr size_t BT_LINK_KEY_SIZE = 16;
constexpr const char* BLUEZ_STATE_DIR = "/var/lib/bluetooth";

// "aa:bb:cc:dd:ee:ff" (either case) to bytes in written order; false if it is
// not exactly that shape.
bool parse_bt_mac(const std::string& text, uint8_t out[6]);

// Feature report 0x13: ID, the host MAC least significant byte first (as in
// report 0x12), then the link key. `host_mac` is in written order.
void build_ds4_pairing_report(const uint8_t host_mac[6], const uint8_t key[BT_LINK_KEY_SIZE],
                              uint8_t out[DS4_PAIRING_SET_SIZE]);

// Contents of the BlueZ `info` file for a DualShock 4 paired by cable.
std::string bluez_ds4_device_info(const uint8_t key[BT_LINK_KEY_SIZE]);

// Reads the pad's MAC (feature 0x12), records the link key in BlueZ's state
// under `bluez_dir` for the adapter `adapter_arg` (an adapter MAC, or empty to
// use the only one there) and writes the host MAC and key to the pad. Leaves
// no BlueZ entry behind if the pad write fails, and refuses to replace an
// entry BlueZ already has. Returns one line starting "OK:" or "Error:".
std::string cable_pair(FeatureIo& io, const std::string& bluez_dir, const std::string& adapter_arg,
                       const uint8_t key[BT_LINK_KEY_SIZE]);

#endif // DS4_PAIRING_H
