#include "pairing.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "fs-util.h"
#include "translate.h" // ds4_pairing_info_mac, DS4_PAIRING_INFO_*

namespace fs = std::filesystem;

bool parse_bt_mac(const std::string& text, uint8_t out[6]) {
    if (text.size() != 17) return false;
    for (size_t i = 0; i < 6; ++i) {
        const size_t pos = i * 3;
        const char digits[3] = {text[pos], text[pos + 1], '\0'};
        if (!isxdigit((unsigned char)digits[0]) || !isxdigit((unsigned char)digits[1])) return false;
        if (i < 5 && text[pos + 2] != ':') return false;
        out[i] = (uint8_t)strtoul(digits, nullptr, 16);
    }
    return true;
}

void build_ds4_pairing_report(const uint8_t host_mac[6], const uint8_t key[BT_LINK_KEY_SIZE],
                              uint8_t out[DS4_PAIRING_SET_SIZE]) {
    out[0] = DS4_PAIRING_SET_ID;
    for (int i = 0; i < 6; ++i) out[1 + i] = host_mac[5 - i];
    memcpy(out + 7, key, BT_LINK_KEY_SIZE);
}

namespace {

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)toupper(c); });
    return s;
}

std::string key_hex(const uint8_t key[BT_LINK_KEY_SIZE]) {
    static const char kDigits[] = "0123456789ABCDEF";
    std::string out;
    for (size_t i = 0; i < BT_LINK_KEY_SIZE; ++i) {
        out += kDigits[(unsigned)key[i] / 16u];
        out += kDigits[(unsigned)key[i] % 16u];
    }
    return out;
}

// The adapter directories under the BlueZ state directory: those named like a
// MAC address. Sorted, so the choice among several is stable.
std::vector<std::string> list_adapters(const std::string& bluez_dir) {
    std::vector<std::string> adapters;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(bluez_dir, ec)) {
        uint8_t mac[6];
        if (entry.is_directory(ec) && parse_bt_mac(entry.path().filename().string(), mac)) {
            adapters.push_back(entry.path().filename().string());
        }
    }
    std::sort(adapters.begin(), adapters.end());
    return adapters;
}

// The adapter to pair with, as BlueZ names its directory (upper case), or ""
// with `error` set.
std::string choose_adapter(const std::string& bluez_dir, const std::string& requested,
                           std::string& error) {
    const std::vector<std::string> adapters = list_adapters(bluez_dir);
    if (adapters.empty()) {
        error = "Error: No Bluetooth adapter found under " + bluez_dir + ".";
        return "";
    }
    if (requested.empty()) {
        if (adapters.size() == 1) return adapters[0];
        error = "Error: Several Bluetooth adapters found; name one: pair <adapter MAC>.";
        return "";
    }
    uint8_t mac[6];
    if (!parse_bt_mac(requested, mac)) {
        error = "Error: '" + requested + "' is not a Bluetooth MAC address (aa:bb:cc:dd:ee:ff).";
        return "";
    }
    const std::string wanted = upper(requested);
    if (std::find(adapters.begin(), adapters.end(), wanted) == adapters.end()) {
        error = "Error: No Bluetooth adapter " + wanted + " under " + bluez_dir + ".";
        return "";
    }
    return wanted;
}

} // namespace

std::string bluez_ds4_device_info(const uint8_t key[BT_LINK_KEY_SIZE]) {
    // Name, class and UUIDs are what BlueZ records for a DualShock 4 it has
    // paired itself; Source=2 is the USB vendor-ID namespace. Trusted lets the
    // pad reconnect without an authorisation prompt.
    return "[General]\n"
           "Name=Wireless Controller\n"
           "Class=0x002508\n"
           "SupportedTechnologies=BR/EDR;\n"
           "Trusted=true\n"
           "Blocked=false\n"
           "Services=00001124-0000-1000-8000-00805f9b34fb;00001200-0000-1000-8000-00805f9b34fb;\n"
           "\n"
           "[DeviceID]\n"
           "Source=2\n"
           "Vendor=1356\n"
           "Product=1476\n"
           "Version=1\n"
           "\n"
           "[LinkKey]\n"
           "Key=" + key_hex(key) + "\n"
           "Type=0\n"
           "PINLength=0\n";
}

std::string cable_pair(FeatureIo& io, const std::string& bluez_dir, const std::string& adapter_arg,
                       const uint8_t key[BT_LINK_KEY_SIZE]) {
    uint8_t info[DS4_PAIRING_INFO_SIZE] = {};
    const int got = io.get(DS4_PAIRING_INFO_ID, info, sizeof(info));
    const std::string pad_mac = got < 0 ? "" : ds4_pairing_info_mac(info, (size_t)got);
    if (pad_mac.empty()) return "Error: Could not read the controller's Bluetooth MAC.";

    std::string error;
    const std::string adapter = choose_adapter(bluez_dir, adapter_arg, error);
    if (adapter.empty()) return error;

    const std::string pad_name = upper(pad_mac);
    const fs::path device_dir = fs::path(bluez_dir) / adapter / pad_name;
    const fs::path info_path = device_dir / "info";
    std::error_code ec;
    if (fs::exists(info_path, ec)) {
        return "Error: BlueZ already has " + pad_name + ". To pair again run: bluetoothctl remove " +
               pad_name;
    }

    const bool created_dir = fs::create_directory(device_dir, ec);
    if (ec) return "Error: Could not create " + device_dir.string() + ": " + ec.message();
    if (created_dir && chmod(device_dir.c_str(), 0700) != 0) {
        fs::remove(device_dir, ec);
        return "Error: Could not restrict " + device_dir.string() + ": " + strerror(errno);
    }

    std::string write_error;
    if (!ds4fs::write_file_atomic(info_path.string(), bluez_ds4_device_info(key), 0600, &write_error)) {
        if (created_dir) fs::remove(device_dir, ec);
        return "Error: Could not write " + info_path.string() + ": " + write_error;
    }

    uint8_t host_mac[6];
    parse_bt_mac(adapter, host_mac); // list_adapters only returns valid MACs
    uint8_t report[DS4_PAIRING_SET_SIZE];
    build_ds4_pairing_report(host_mac, key, report);
    if (!io.set(report, sizeof(report))) {
        fs::remove(info_path, ec);
        if (created_dir) fs::remove(device_dir, ec);
        return "Error: The controller rejected the pairing report; nothing was changed in BlueZ.";
    }

    return "OK: paired " + pad_name + " with adapter " + adapter +
           ". Restart bluetoothd so it loads the new entry (sudo systemctl restart bluetooth), then "
           "unplug the controller and press its PS button.";
}
