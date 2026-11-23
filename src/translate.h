#ifndef DS4_TRANSLATE_H
#define DS4_TRANSLATE_H

// Hardware-independent core of the translator: controller types, the
// DS4/DualSense/Xbox report layouts, CRC32, battery re-encoding and the Xbox
// report builders. Kept free of I/O (no fds, sysfs, uhid, FunctionFS state)
// so it can be unit tested without a controller; main.cpp does the
// plumbing around it.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "functionfs-backend.h" // FFS_PACKET_MAX

enum ControllerType {
    TYPE_DS4,
    TYPE_DUALSENSE,
    TYPE_XBOX360,
    TYPE_XBOXONE,
    TYPE_XBOXSERIES,
    TYPE_NONE,
    TYPE_HIDDEN
};

bool controller_type_is_xbox(ControllerType type);
bool controller_type_is_gip(ControllerType type);
bool controller_type_emulates(ControllerType type);
bool controller_type_hides_physical(ControllerType type);
const char* controller_type_name(ControllerType type);
const char* controller_type_config_str(ControllerType type);
bool parse_controller_type(const std::string& val, ControllerType& out);
bool parse_emulated_type(const std::string& val, ControllerType& out);

// CRC32 (IEEE 802.3, as the kernel's hid-playstation computes it) over
// `seed` followed by `data`. Bluetooth output reports use seed 0xA2, input
// reports 0xA1.
uint32_t calculate_crc32(uint8_t seed, const uint8_t* data, size_t len);

// Structures matching hid-playstation.c layout
struct __attribute__((packed)) dualshock4_input_report_common {
    uint8_t x, y;
    uint8_t rx, ry;
    uint8_t buttons[3];
    uint8_t z, rz;
    uint16_t sensor_timestamp;
    uint8_t sensor_temperature;
    int16_t gyro[3];
    int16_t accel[3];
    uint8_t reserved2[5];
    uint8_t status[2];
    uint8_t reserved3;
};

struct __attribute__((packed)) dualshock4_touch_point {
    uint8_t contact;
    uint8_t x_lo;
    uint8_t x_hi:4, y_lo:4;
    uint8_t y_hi;
};

struct __attribute__((packed)) dualshock4_touch_report {
    uint8_t timestamp;
    struct dualshock4_touch_point points[2];
};

struct __attribute__((packed)) dualshock4_input_report_bt {
    uint8_t report_id; // 0x11
    uint8_t reserved[2];
    struct dualshock4_input_report_common common;
    uint8_t num_touch_reports;
    struct dualshock4_touch_report touch_reports[4];
    uint8_t reserved2[2];
    uint32_t crc32;
};

struct __attribute__((packed)) dualshock4_input_report_usb {
    uint8_t report_id; // 0x01
    struct dualshock4_input_report_common common;
    uint8_t num_touch_reports;
    struct dualshock4_touch_report touch_reports[3];
    uint8_t reserved[3];
};

struct __attribute__((packed)) dualsense_touch_point {
    uint8_t contact;
    uint8_t x_lo;
    uint8_t x_hi:4, y_lo:4;
    uint8_t y_hi;
};

struct __attribute__((packed)) dualsense_input_report {
    uint8_t x, y;
    uint8_t rx, ry;
    uint8_t z, rz;
    uint8_t seq_number;
    uint8_t buttons[4];
    uint8_t reserved[4];
    int16_t gyro[3];
    int16_t accel[3];
    uint32_t sensor_timestamp;
    uint8_t reserved2;
    struct dualsense_touch_point points[2];
    uint8_t reserved3[12];
    uint8_t status[3];
    uint8_t reserved4[8];
};

// These mirror the kernel's hid-playstation layouts byte for byte; a changed
// size means the struct no longer matches what the hardware sends.
static_assert(sizeof(dualshock4_input_report_common) == 32, "DS4 common block is 32 bytes");
static_assert(sizeof(dualshock4_touch_report) == 9, "DS4 touch report is 9 bytes");
static_assert(sizeof(dualshock4_input_report_usb) == 64, "DS4 USB report (0x01) is 64 bytes");
static_assert(sizeof(dualshock4_input_report_bt) == 78, "DS4 Bluetooth report (0x11) is 78 bytes");
static_assert(sizeof(dualsense_input_report) == 63, "DualSense input report is 63 bytes");

// The short Bluetooth input report (id 0x01, 10 bytes) a pad sends until it is
// switched to extended mode. Its sticks, buttons and triggers sit in the same
// bytes as the start of the common block; motion, touch and status stay zero.
constexpr size_t BT_BASIC_REPORT_SIZE = 10;
bool parse_bt_basic_input_report(const uint8_t* data, size_t len,
                                 dualshock4_input_report_common& out);

// The USB report holds three touchpad packets, a Bluetooth one up to four.
// Copies the newest min(num_touch, 3) of the first min(num_touch, 4) packets
// to `out` (oldest first) and returns how many were copied.
size_t select_usb_touch_reports(uint8_t num_touch, const dualshock4_touch_report in[4],
                                dualshock4_touch_report out[3]);

// What the daemon asks the physical pad to do: rumble motors, light bar colour
// and light bar flash (bright/dark durations, 0/0 = steady).
struct Ds4Output {
    uint8_t motor_left = 0;
    uint8_t motor_right = 0;
    uint8_t r = 0, g = 0, b = 0;
    uint8_t flash_on = 0;
    uint8_t flash_off = 0;
};

constexpr size_t DS4_USB_OUTPUT_SIZE = 32;
constexpr size_t DS4_BT_OUTPUT_SIZE = 78;
// Fills `out` (DS4_BT_OUTPUT_SIZE with CRC32 seed 0xA2 for Bluetooth,
// DS4_USB_OUTPUT_SIZE otherwise) with the output report for `o` and returns
// its size. The valid flags always claim motor, light bar and flash, so
// every report is the full state.
size_t build_ds4_output_report(bool bluetooth, const Ds4Output& o, uint8_t out[DS4_BT_OUTPUT_SIZE]);

// Feature report 0xA3: build date/time, hardware and firmware versions. Same
// 49-byte layout (report ID first) over USB and Bluetooth.
constexpr uint8_t DS4_FIRMWARE_REPORT_ID = 0xA3;
constexpr size_t DS4_FIRMWARE_REPORT_SIZE = 49;
bool is_valid_ds4_firmware_report(const uint8_t* data, size_t len);

// Prints the "<build date> <build time>, hw 0xHHHH.MMMM, fw 0xHHHHHHHH.MMMM"
// summary of a valid firmware report; empty string for an invalid one.
std::string describe_ds4_firmware(const uint8_t* data, size_t len);

enum BatteryState { BATTERY_DISCHARGING, BATTERY_CHARGING, BATTERY_FULL };

void decode_ds4_battery(uint8_t ds4_status0, int& percent, BatteryState& state);
uint8_t encode_dualsense_battery(int percent, BatteryState state);

int16_t xbox_stick_axis(uint8_t value, bool invert);
uint8_t xbox_dpad_bits(uint8_t hat);
void put_le16(uint8_t* out, uint16_t value);

constexpr size_t XBOX360_REPORT_SIZE = 20;
constexpr size_t GIP_ONE_REPORT_SIZE = 18;
constexpr size_t GIP_SERIES_REPORT_SIZE = 48;
constexpr size_t GIP_SERIES_SHARE_OFFSET = GIP_SERIES_REPORT_SIZE - 26;

void build_xbox360_report(const dualshock4_input_report_common& common, uint8_t out[XBOX360_REPORT_SIZE]);
size_t build_gip_input_report(const dualshock4_input_report_common& common, bool series,
                              uint8_t seq, uint8_t out[FFS_PACKET_MAX]);

#endif // DS4_TRANSLATE_H
