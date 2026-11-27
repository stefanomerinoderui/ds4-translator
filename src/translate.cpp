#include "translate.h"

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

bool ds4_calibration_to_usb_layout(const uint8_t* report, size_t len,
                                   uint8_t out[DS4_CALIBRATION_SIZE]) {
    constexpr uint8_t kUsbReportId = 0x02;
    constexpr size_t kUsbReportSize = 1 + DS4_CALIBRATION_SIZE;
    constexpr uint8_t kBtReportId = 0x05;
    constexpr size_t kBtReportSize = kUsbReportSize + 4; // plus CRC32
    constexpr uint8_t kBtFeatureCrcSeed = 0xA3;
    constexpr size_t kGyroWordsOffset = 6; // after the three gyro biases
    constexpr size_t kWord = 2;

    if (report == nullptr) return false;
    if (len == kUsbReportSize && report[0] == kUsbReportId) {
        memcpy(out, report + 1, DS4_CALIBRATION_SIZE);
        return true;
    }
    if (len != kBtReportSize || report[0] != kBtReportId) return false;

    const uint32_t want = (uint32_t)report[len - 4] | ((uint32_t)report[len - 3] << 8) |
                          ((uint32_t)report[len - 2] << 16) | ((uint32_t)report[len - 1] << 24);
    if (calculate_crc32(kBtFeatureCrcSeed, report, len - 4) != want) return false;

    memcpy(out, report + 1, DS4_CALIBRATION_SIZE);
    // Word i of the Bluetooth gyro block lands where USB keeps it.
    static const size_t kBtWordForUsbWord[6] = {0, 2, 4, 1, 3, 5};
    for (size_t usb_word = 0; usb_word < 6; ++usb_word) {
        memcpy(out + kGyroWordsOffset + usb_word * kWord,
               report + 1 + kGyroWordsOffset + kBtWordForUsbWord[usb_word] * kWord, kWord);
    }
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

void build_ds4_usb_report(const dualshock4_input_report_common& common, uint8_t num_touch,
                          const dualshock4_touch_report touch_reps[4], bool has_real_battery,
                          uint8_t out[DS_VIRTUAL_REPORT_SIZE]) {
    memset(out, 0, DS_VIRTUAL_REPORT_SIZE);
    auto* report = reinterpret_cast<dualshock4_input_report_usb*>(out);
    report->report_id = 0x01;
    report->common = common;
    if (!has_real_battery) {
        // No hardware behind this device: report a plausible fixed state
        // (USB-wired, fully charged) instead of whatever zeroed or synthetic
        // value common.status happens to hold. status[0] bit 4 is the cable,
        // bits 3:0 the level (0-10, 0x0B = full).
        report->common.status[0] = 0x1B;
        report->common.status[1] = 0x00;
    }
    report->num_touch_reports = (num_touch > 3) ? 3 : num_touch;
    for (int i = 0; i < report->num_touch_reports; ++i) {
        report->touch_reports[i] = touch_reps[i];
    }
    // Real hardware fills every slot; leaving unused ones zeroed (contact 0,
    // x 0, y 0) reads as a touch at (0,0) to any consumer that does not gate
    // on num_touch_reports, so mark both points "not touching" (bit 7).
    for (int i = report->num_touch_reports; i < 3; ++i) {
        report->touch_reports[i].points[0].contact = 0x80;
        report->touch_reports[i].points[1].contact = 0x80;
    }
}

void build_dualsense_report(const dualshock4_input_report_common& common, uint8_t num_touch,
                            const dualshock4_touch_report touch_reps[4], uint8_t seq,
                            bool has_real_battery, uint8_t out[DS_VIRTUAL_REPORT_SIZE]) {
    memset(out, 0, DS_VIRTUAL_REPORT_SIZE);
    out[0] = 0x01;
    auto* report = reinterpret_cast<dualsense_input_report*>(&out[1]);

    report->x = common.x;
    report->y = common.y;
    report->rx = common.rx;
    report->ry = common.ry;
    report->z = common.z;
    report->rz = common.rz;
    report->buttons[0] = common.buttons[0];
    report->buttons[1] = common.buttons[1];
    report->buttons[2] = common.buttons[2] & 0x03;
    report->buttons[3] = 0;
    report->seq_number = seq;
    memcpy(report->gyro, common.gyro, sizeof(report->gyro));
    memcpy(report->accel, common.accel, sizeof(report->accel));
    report->sensor_timestamp = (uint32_t)common.sensor_timestamp;

    if (num_touch > 0) {
        // The newest report of the batch: the older ones are the same
        // fingers a few milliseconds earlier.
        const int latest = (num_touch > 4 ? 4 : num_touch) - 1;
        for (int i = 0; i < 2; ++i) {
            const dualshock4_touch_point& from = touch_reps[latest].points[i];
            report->points[i].contact = from.contact;
            report->points[i].x_lo = from.x_lo;
            report->points[i].x_hi = from.x_hi;
            report->points[i].y_lo = from.y_lo;
            report->points[i].y_hi = from.y_hi;
        }
    } else {
        report->points[0].contact = 0x80;
        report->points[1].contact = 0x80;
    }

    if (has_real_battery) {
        int percent;
        BatteryState state;
        decode_ds4_battery(common.status[0], percent, state);
        report->status[0] = encode_dualsense_battery(percent, state);
    } else {
        report->status[0] = 0x2B; // fully charged
    }
}
