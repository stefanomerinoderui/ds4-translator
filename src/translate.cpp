#include "translate.h"

#include <stdio.h>
#include <string.h>

// The Xbox types are vendor-class USB devices bound by xpad rather than HID
// devices, so they only exist as functionfs gadgets (no uhid, no custom name,
// no MAC squat).
bool controller_type_is_xbox(ControllerType type) {
    return type == TYPE_XBOX360 || type == TYPE_XBOXONE || type == TYPE_XBOXSERIES;
}

// Xbox One S / Series X|S: GIP instead of the 360's fixed packet format.
bool controller_type_is_gip(ControllerType type) {
    return type == TYPE_XBOXONE || type == TYPE_XBOXSERIES;
}

// TYPE_DS4/TYPE_DUALSENSE/the Xbox types are the only types that actually
// drive a virtual device; TYPE_NONE (fully hands-off passthrough) and TYPE_HIDDEN (physical
// hidden from other apps, but no translation) never create one.
bool controller_type_emulates(ControllerType type) {
    return type == TYPE_DS4 || type == TYPE_DUALSENSE || controller_type_is_xbox(type);
}

// Every type except TYPE_NONE hides the physical controller from other apps.
bool controller_type_hides_physical(ControllerType type) {
    return type != TYPE_NONE;
}

const char* controller_type_name(ControllerType type) {
    switch (type) {
        case TYPE_DS4: return "DualShock 4";
        case TYPE_DUALSENSE: return "DualSense";
        case TYPE_XBOX360: return "Xbox 360";
        case TYPE_XBOXONE: return "Xbox One S";
        case TYPE_XBOXSERIES: return "Xbox Series X|S";
        case TYPE_HIDDEN: return "Hidden";
        default: return "None";
    }
}

const char* controller_type_config_str(ControllerType type) {
    switch (type) {
        case TYPE_DS4: return "ds4";
        case TYPE_DUALSENSE: return "dualsense";
        case TYPE_XBOX360: return "xbox360";
        case TYPE_XBOXONE: return "xboxone";
        case TYPE_XBOXSERIES: return "xboxseries";
        case TYPE_HIDDEN: return "hidden";
        default: return "none";
    }
}

// Config / command-line spelling -> type. False (out untouched) if unknown.
bool parse_controller_type(const std::string& val, ControllerType& out) {
    static const ControllerType all[] = { TYPE_DS4, TYPE_DUALSENSE, TYPE_XBOX360,
                                         TYPE_XBOXONE, TYPE_XBOXSERIES, TYPE_NONE, TYPE_HIDDEN };
    for (ControllerType t : all) {
        if (val == controller_type_config_str(t)) {
            out = t;
            return true;
        }
    }
    return false;
}

// Like parse_controller_type(), but only the types that drive a virtual
// device -- what set-type (besides none/hidden) and create-virtual accept.
bool parse_emulated_type(const std::string& val, ControllerType& out) {
    ControllerType parsed;
    if (!parse_controller_type(val, parsed) || !controller_type_emulates(parsed)) return false;
    out = parsed;
    return true;
}

// CRC32 calculation matching Linux kernel (IEEE 802.3 Ethernet polynomial 0xEDB88320)
uint32_t calculate_crc32(uint8_t seed, const uint8_t* data, size_t len) {
    static uint32_t table[256];
    static bool table_initialized = false;
    if (!table_initialized) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int j = 0; j < 8; j++) {
                if (c & 1) {
                    c = 0xEDB88320L ^ (c >> 1);
                } else {
                    c = c >> 1;
                }
            }
            table[i] = c;
        }
        table_initialized = true;
    }

    uint32_t crc = 0xFFFFFFFF;
    crc = table[(crc ^ seed) & 0xFF] ^ (crc >> 8);
    for (size_t i = 0; i < len; i++) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

// Battery passthrough: the physical DS4's own status byte (common.status[0])
// already carries genuine charge/cable-state data from hardware, in the
// DS4 wire format (bit4 = cable connected, bits0-3 = level, with an
// out-of-range value of 11 used as a "fully charged" sentinel while wired).
// When emulating a DS4 target this can be forwarded byte-for-byte, but
// DualSense uses a different layout (bits0-3 = level, bits4-6 = charging
// state: 0=discharging, 1=charging, 2=full), so cross-emulating requires
// decoding to a state both formats can express and re-encoding.

void decode_ds4_battery(uint8_t ds4_status0, int& percent, BatteryState& state) {
    uint8_t level = ds4_status0 & 0x0F;
    bool usb = (ds4_status0 & 0x10) != 0;
    if (usb) {
        if (level > 10) { percent = 100; state = BATTERY_FULL; }
        else { percent = level * 10; state = BATTERY_CHARGING; }
    } else {
        percent = (level > 8) ? 100 : (level + 1) * 10;
        state = BATTERY_DISCHARGING;
    }
    if (percent > 100) percent = 100;
}

uint8_t encode_dualsense_battery(int percent, BatteryState state) {
    uint8_t status_bits;
    switch (state) {
        case BATTERY_FULL:       status_bits = 0x2; break;
        case BATTERY_CHARGING:   status_bits = 0x1; break;
        default:                 status_bits = 0x0; break;
    }
    int level = (percent - 5) / 10;
    if (level < 0) level = 0;
    if (level > 10) level = 10;
    return (uint8_t)((status_bits << 4) | (level & 0x0F));
}

// Shared by the 360 and GIP builders below: both are fed the DS4-format
// "common" struct, so physical passthrough and the standalone synthetic input
// use the same translation. The d-pad bits (up 0x01, down 0x02, left 0x04,
// right 0x08) are identical on every Xbox wire format.
int16_t xbox_stick_axis(uint8_t value, bool invert) {
    int32_t v = (int32_t(value) - 128) * 257;
    if (invert) v = -v;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return (int16_t)v;
}

uint8_t xbox_dpad_bits(uint8_t hat) {
    // Hat: 0=up, 1=up-right ... 7=up-left clockwise, 8+ = neutral.
    static const uint8_t hat_to_dpad[8] = {
        0x01, 0x01 | 0x08, 0x08, 0x02 | 0x08, 0x02, 0x02 | 0x04, 0x04, 0x01 | 0x04
    };
    return (hat < 8) ? hat_to_dpad[hat] : 0;
}

bool parse_bt_basic_input_report(const uint8_t* data, size_t len,
                                 dualshock4_input_report_common& out) {
    if (len != BT_BASIC_REPORT_SIZE || data[0] != 0x01) {
        return false;
    }
    memset(&out, 0, sizeof(out));
    // x, y, rx, ry, buttons[3], z, rz: bytes 1-9, as in the common block.
    memcpy(&out, data + 1, BT_BASIC_REPORT_SIZE - 1);
    return true;
}

size_t select_usb_touch_reports(uint8_t num_touch, const dualshock4_touch_report in[4],
                                dualshock4_touch_report out[3]) {
    const size_t available = num_touch > 4 ? 4 : num_touch;
    const size_t count = available > 3 ? 3 : available;
    const size_t first = available - count;
    for (size_t i = 0; i < count; ++i) {
        out[i] = in[first + i];
    }
    return count;
}

namespace {
uint32_t read_le_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
} // namespace

size_t build_ds4_output_report(bool bluetooth, const Ds4Output& o, uint8_t out[DS4_BT_OUTPUT_SIZE]) {
    memset(out, 0, DS4_BT_OUTPUT_SIZE);
    if (bluetooth) {
        out[0] = 0x11; // report ID
        out[1] = 0xC0; // hw_control: HID | CRC32
        out[3] = 0x07; // valid_flag0: motor | led | led_blink
        out[6] = o.motor_right;
        out[7] = o.motor_left;
        out[8] = o.r;
        out[9] = o.g;
        out[10] = o.b;
        out[11] = o.flash_on;
        out[12] = o.flash_off;
        const uint32_t crc = calculate_crc32(0xA2, out, 74);
        out[74] = crc & 0xFF;
        out[75] = (crc >> 8) & 0xFF;
        out[76] = (crc >> 16) & 0xFF;
        out[77] = (crc >> 24) & 0xFF;
        return DS4_BT_OUTPUT_SIZE;
    }
    out[0] = 0x05; // report ID
    out[1] = 0x07; // valid_flag0: motor | led | led_blink
    out[4] = o.motor_right;
    out[5] = o.motor_left;
    out[6] = o.r;
    out[7] = o.g;
    out[8] = o.b;
    out[9] = o.flash_on;
    out[10] = o.flash_off;
    return DS4_USB_OUTPUT_SIZE;
}

bool is_valid_ds4_imu_calibration_usb(const uint8_t* data, size_t len) {
    return len == DS4_IMU_CAL_USB_SIZE && data[0] == DS4_IMU_CAL_USB_ID;
}

bool convert_ds4_imu_calibration_bt(const uint8_t* bt, size_t len, uint8_t usb[DS4_IMU_CAL_USB_SIZE]) {
    if (len != DS4_IMU_CAL_BT_SIZE || bt[0] != DS4_IMU_CAL_BT_ID) return false;
    const uint32_t received = read_le_u32(bt + DS4_IMU_CAL_USB_SIZE);
    if (received != calculate_crc32(0xA3, bt, DS4_IMU_CAL_USB_SIZE)) return false;

    memcpy(usb, bt, DS4_IMU_CAL_USB_SIZE);
    usb[0] = DS4_IMU_CAL_USB_ID;
    // Gyro pitch/yaw/roll gains: Bluetooth lists the three plus values then
    // the three minus values, USB interleaves plus/minus per axis. Every
    // other field sits at the same offset in both.
    const size_t pitch_plus = 7, yaw_plus = 9, roll_plus = 11;
    const size_t pitch_minus = 13, yaw_minus = 15, roll_minus = 17;
    memcpy(usb + 7, bt + pitch_plus, 2);
    memcpy(usb + 9, bt + pitch_minus, 2);
    memcpy(usb + 11, bt + yaw_plus, 2);
    memcpy(usb + 13, bt + yaw_minus, 2);
    memcpy(usb + 15, bt + roll_plus, 2);
    memcpy(usb + 17, bt + roll_minus, 2);
    return true;
}

std::string ds4_pairing_info_mac(const uint8_t* data, size_t len) {
    if (len != DS4_PAIRING_INFO_SIZE || data[0] != DS4_PAIRING_INFO_ID) return "";
    bool any = false;
    char out[18];
    for (int i = 0; i < 6; ++i) {
        const uint8_t byte = data[6 - i]; // stored least significant byte first
        any = any || byte != 0;
        snprintf(out + i * 3, 4, i < 5 ? "%02x:" : "%02x", byte);
    }
    return any ? std::string(out) : "";
}

bool is_valid_ds4_firmware_report(const uint8_t* data, size_t len) {
    return len == DS4_FIRMWARE_REPORT_SIZE && data[0] == DS4_FIRMWARE_REPORT_ID;
}

namespace {
uint32_t read_le(const uint8_t* p, int bytes) {
    uint32_t v = 0;
    for (int i = bytes - 1; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

// A zero-padded ASCII field of `max` bytes, without the padding.
std::string ascii_field(const uint8_t* p, size_t max) {
    size_t n = 0;
    while (n < max && p[n] != 0) ++n;
    return std::string(reinterpret_cast<const char*>(p), n);
}
} // namespace

std::string describe_ds4_firmware(const uint8_t* data, size_t len) {
    if (!is_valid_ds4_firmware_report(data, len)) return "";
    // Offsets are +1 from the documented payload layout: byte 0 is the report ID.
    char versions[96];
    snprintf(versions, sizeof(versions), "hw 0x%04x.%04x, fw 0x%08x.%04x",
             read_le(data + 33, 2), read_le(data + 35, 2), read_le(data + 37, 4), read_le(data + 41, 2));
    return ascii_field(data + 1, 16) + " " + ascii_field(data + 17, 16) + ", " + versions;
}

void put_le16(uint8_t* out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFF);
    out[1] = (uint8_t)(value >> 8);
}

// Xbox 360 wire format (xpad's xpad360_process_packet): 20-byte input report,
// byte 0 = 0x00 (data), byte 1 = 0x14 (length), then two button bytes, two
// 8-bit triggers and four little-endian int16 stick axes (Y up = positive).

void build_xbox360_report(const dualshock4_input_report_common& common, uint8_t out[XBOX360_REPORT_SIZE]) {
    memset(out, 0, XBOX360_REPORT_SIZE);
    out[1] = 0x14;

    uint8_t b2 = xbox_dpad_bits(common.buttons[0] & 0x0F);
    if (common.buttons[1] & 0x20) b2 |= 0x10; // Options -> Start
    if (common.buttons[1] & 0x10) b2 |= 0x20; // Share -> Back
    if (common.buttons[2] & 0x02) b2 |= 0x20; // Touchpad click -> Back
    if (common.buttons[1] & 0x40) b2 |= 0x40; // L3
    if (common.buttons[1] & 0x80) b2 |= 0x80; // R3

    uint8_t b3 = 0;
    if (common.buttons[1] & 0x01) b3 |= 0x01; // L1 -> LB
    if (common.buttons[1] & 0x02) b3 |= 0x02; // R1 -> RB
    if (common.buttons[2] & 0x01) b3 |= 0x04; // PS -> Guide
    if (common.buttons[0] & 0x20) b3 |= 0x10; // Cross -> A
    if (common.buttons[0] & 0x40) b3 |= 0x20; // Circle -> B
    if (common.buttons[0] & 0x10) b3 |= 0x40; // Square -> X
    if (common.buttons[0] & 0x80) b3 |= 0x80; // Triangle -> Y
    out[2] = b2;
    out[3] = b3;
    out[4] = common.z;
    out[5] = common.rz;

    const int16_t axes[4] = {
        xbox_stick_axis(common.x, false), xbox_stick_axis(common.y, true),
        xbox_stick_axis(common.rx, false), xbox_stick_axis(common.ry, true)
    };
    for (int i = 0; i < 4; ++i) {
        put_le16(&out[6 + i * 2], (uint16_t)axes[i]);
    }
}

// Xbox One / Series wire format (GIP, xpad's xpadone_process_packet): a 4-byte
// header 0x20 0x00 <seq> <payload length>, then button bytes at 4 and 5,
// 16-bit triggers (0-1023) at 6-9 and four int16 sticks at 10-17. The Series
// pad's Share button is bit 0 of byte (packet length - 26), which xpad only
// reads when the packet is at least that long -- hence the larger Series
// packet. Guide is not part of this report; see send_gip_guide_event().

size_t build_gip_input_report(const dualshock4_input_report_common& common, bool series,
                              uint8_t seq, uint8_t out[FFS_PACKET_MAX]) {
    const size_t size = series ? GIP_SERIES_REPORT_SIZE : GIP_ONE_REPORT_SIZE;
    memset(out, 0, size);
    out[0] = 0x20;
    out[2] = seq;
    out[3] = (uint8_t)(size - 4);

    const bool touchpad = (common.buttons[2] & 0x02) != 0;
    uint8_t b4 = 0;
    if (common.buttons[1] & 0x20) b4 |= 0x04;                   // Options -> Menu
    if ((common.buttons[1] & 0x10) || (touchpad && !series)) b4 |= 0x08; // Share -> View
    if (common.buttons[0] & 0x20) b4 |= 0x10;                   // Cross -> A
    if (common.buttons[0] & 0x40) b4 |= 0x20;                   // Circle -> B
    if (common.buttons[0] & 0x10) b4 |= 0x40;                   // Square -> X
    if (common.buttons[0] & 0x80) b4 |= 0x80;                   // Triangle -> Y

    uint8_t b5 = xbox_dpad_bits(common.buttons[0] & 0x0F);
    if (common.buttons[1] & 0x01) b5 |= 0x10; // L1 -> LB
    if (common.buttons[1] & 0x02) b5 |= 0x20; // R1 -> RB
    if (common.buttons[1] & 0x40) b5 |= 0x40; // L3
    if (common.buttons[1] & 0x80) b5 |= 0x80; // R3
    out[4] = b4;
    out[5] = b5;

    put_le16(&out[6], (uint16_t)(common.z * 1023 / 255));
    put_le16(&out[8], (uint16_t)(common.rz * 1023 / 255));
    const int16_t axes[4] = {
        xbox_stick_axis(common.x, false), xbox_stick_axis(common.y, true),
        xbox_stick_axis(common.rx, false), xbox_stick_axis(common.ry, true)
    };
    for (int i = 0; i < 4; ++i) {
        put_le16(&out[10 + i * 2], (uint16_t)axes[i]);
    }

    if (series && touchpad) out[GIP_SERIES_SHARE_OFFSET] |= 0x01; // Touchpad click -> Share
    return size;
}
