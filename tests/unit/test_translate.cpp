// Unit tests for src/translate.cpp: controller types, CRC32, battery
// re-encoding and the Xbox 360 / GIP report builders.
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "translate.h"

namespace {

// A report with sticks centred and nothing pressed (dpad neutral).
dualshock4_input_report_common neutral() {
    dualshock4_input_report_common c;
    std::memset(&c, 0, sizeof(c));
    c.x = c.y = c.rx = c.ry = 128;
    c.buttons[0] = 0x08;
    return c;
}

int16_t le16(const uint8_t *p) { return static_cast<int16_t>(p[0] | (p[1] << 8)); }

} // namespace

// ----------------------------------------------------------- controller type

TEST(ControllerType, NameAndConfigStringRoundTrip) {
    for (ControllerType t : {TYPE_DS4, TYPE_DUALSENSE, TYPE_XBOX360, TYPE_XBOXONE, TYPE_XBOXSERIES,
                             TYPE_NONE, TYPE_HIDDEN}) {
        ControllerType parsed = TYPE_DS4;
        ASSERT_TRUE(parse_controller_type(controller_type_config_str(t), parsed));
        EXPECT_EQ(parsed, t);
    }
}

TEST(ControllerType, ParseRejectsUnknownAndLeavesOutputUntouched) {
    ControllerType out = TYPE_DUALSENSE;
    EXPECT_FALSE(parse_controller_type("", out));
    EXPECT_FALSE(parse_controller_type("DS4", out));
    EXPECT_FALSE(parse_controller_type("xbox", out));
    EXPECT_EQ(out, TYPE_DUALSENSE);
}

TEST(ControllerType, EmulatedParseExcludesNoneAndHidden) {
    ControllerType out = TYPE_NONE;
    EXPECT_TRUE(parse_emulated_type("xbox360", out));
    EXPECT_EQ(out, TYPE_XBOX360);
    EXPECT_FALSE(parse_emulated_type("none", out));
    EXPECT_FALSE(parse_emulated_type("hidden", out));
    EXPECT_EQ(out, TYPE_XBOX360);
}

TEST(ControllerType, Predicates) {
    EXPECT_TRUE(controller_type_is_xbox(TYPE_XBOX360));
    EXPECT_TRUE(controller_type_is_xbox(TYPE_XBOXSERIES));
    EXPECT_FALSE(controller_type_is_xbox(TYPE_DS4));
    EXPECT_FALSE(controller_type_is_gip(TYPE_XBOX360));
    EXPECT_TRUE(controller_type_is_gip(TYPE_XBOXONE));
    EXPECT_TRUE(controller_type_emulates(TYPE_DUALSENSE));
    EXPECT_FALSE(controller_type_emulates(TYPE_NONE));
    EXPECT_FALSE(controller_type_emulates(TYPE_HIDDEN));
    EXPECT_FALSE(controller_type_hides_physical(TYPE_NONE));
    EXPECT_TRUE(controller_type_hides_physical(TYPE_HIDDEN));
}

TEST(ControllerType, DisplayNames) {
    EXPECT_STREQ(controller_type_name(TYPE_DS4), "DualShock 4");
    EXPECT_STREQ(controller_type_name(TYPE_XBOXSERIES), "Xbox Series X|S");
    EXPECT_STREQ(controller_type_name(TYPE_NONE), "None");
}

// --------------------------------------------------------------------- CRC32

TEST(Crc32, MatchesReferenceVectors) {
    // Reference values computed independently with zlib's crc32 over
    // {seed} + data.
    const uint8_t data[] = {1, 2, 3};
    EXPECT_EQ(calculate_crc32(0xA2, data, sizeof(data)), 0x6CDB579Du);
    EXPECT_EQ(calculate_crc32(0xA1, nullptr, 0), 0x73D37CF3u);
}

TEST(Crc32, SeedChangesResult) {
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_NE(calculate_crc32(0xA1, data, sizeof(data)), calculate_crc32(0xA2, data, sizeof(data)));
}

TEST(Crc32, SingleBitFlipChangesResult) {
    uint8_t data[8] = {};
    const uint32_t before = calculate_crc32(0xA2, data, sizeof(data));
    data[5] ^= 0x01;
    EXPECT_NE(calculate_crc32(0xA2, data, sizeof(data)), before);
}

// ------------------------------------------------------------------- battery

TEST(Battery, WiredFullSentinelAndCharging) {
    int percent = 0;
    BatteryState state = BATTERY_DISCHARGING;
    decode_ds4_battery(0x10 | 11, percent, state); // cable + out-of-range level
    EXPECT_EQ(percent, 100);
    EXPECT_EQ(state, BATTERY_FULL);

    decode_ds4_battery(0x10 | 4, percent, state);
    EXPECT_EQ(percent, 40);
    EXPECT_EQ(state, BATTERY_CHARGING);
}

TEST(Battery, WirelessLevelsAreCapped) {
    int percent = 0;
    BatteryState state = BATTERY_FULL;
    decode_ds4_battery(0, percent, state);
    EXPECT_EQ(percent, 10);
    EXPECT_EQ(state, BATTERY_DISCHARGING);
    decode_ds4_battery(8, percent, state);
    EXPECT_EQ(percent, 90);
    decode_ds4_battery(15, percent, state);
    EXPECT_EQ(percent, 100);
}

TEST(Battery, DualSenseEncoding) {
    EXPECT_EQ(encode_dualsense_battery(100, BATTERY_FULL), 0x29); // level = (percent - 5) / 10
    EXPECT_EQ(encode_dualsense_battery(50, BATTERY_CHARGING), 0x14);
    EXPECT_EQ(encode_dualsense_battery(0, BATTERY_DISCHARGING), 0x00); // clamped, not negative
    EXPECT_EQ(encode_dualsense_battery(500, BATTERY_DISCHARGING), 0x0A); // clamped to level 10
}

// --------------------------------------------------------------- Xbox helpers

TEST(XboxHelpers, StickAxisScalesAndClamps) {
    EXPECT_EQ(xbox_stick_axis(128, false), 0);
    EXPECT_EQ(xbox_stick_axis(255, false), 32639);
    EXPECT_EQ(xbox_stick_axis(0, false), -32768);   // -128*257 = -32896, clamped
    EXPECT_EQ(xbox_stick_axis(0, true), 32767);     // inverted and clamped
    EXPECT_EQ(xbox_stick_axis(255, true), -32639);
}

TEST(XboxHelpers, DpadBits) {
    EXPECT_EQ(xbox_dpad_bits(0), 0x01);        // up
    EXPECT_EQ(xbox_dpad_bits(2), 0x08);        // right
    EXPECT_EQ(xbox_dpad_bits(3), 0x02 | 0x08); // down-right
    EXPECT_EQ(xbox_dpad_bits(6), 0x04);        // left
    EXPECT_EQ(xbox_dpad_bits(8), 0);           // neutral
    EXPECT_EQ(xbox_dpad_bits(0xFF), 0);
}

TEST(XboxHelpers, PutLe16) {
    uint8_t out[2] = {};
    put_le16(out, 0xABCD);
    EXPECT_EQ(out[0], 0xCD);
    EXPECT_EQ(out[1], 0xAB);
}

// ------------------------------------------------------------- Xbox 360 report

TEST(Xbox360Report, NeutralInput) {
    uint8_t out[XBOX360_REPORT_SIZE];
    build_xbox360_report(neutral(), out);
    EXPECT_EQ(out[0], 0x00);
    EXPECT_EQ(out[1], 0x14);
    for (size_t i = 2; i < XBOX360_REPORT_SIZE; ++i) EXPECT_EQ(out[i], 0) << i;
}

TEST(Xbox360Report, ButtonMapping) {
    struct Case { int byte; uint8_t mask; int out_byte; uint8_t out_mask; const char *name; };
    const Case cases[] = {
        {0, 0x20, 3, 0x10, "Cross->A"},    {0, 0x40, 3, 0x20, "Circle->B"},
        {0, 0x10, 3, 0x40, "Square->X"},   {0, 0x80, 3, 0x80, "Triangle->Y"},
        {1, 0x01, 3, 0x01, "L1->LB"},      {1, 0x02, 3, 0x02, "R1->RB"},
        {2, 0x01, 3, 0x04, "PS->Guide"},   {1, 0x20, 2, 0x10, "Options->Start"},
        {1, 0x10, 2, 0x20, "Share->Back"}, {2, 0x02, 2, 0x20, "Touchpad->Back"},
        {1, 0x40, 2, 0x40, "L3"},          {1, 0x80, 2, 0x80, "R3"},
    };
    for (const Case &c : cases) {
        dualshock4_input_report_common in = neutral();
        in.buttons[c.byte] |= c.mask;
        uint8_t out[XBOX360_REPORT_SIZE];
        build_xbox360_report(in, out);
        EXPECT_EQ(out[c.out_byte] & c.out_mask, c.out_mask) << c.name;
    }
}

TEST(Xbox360Report, TriggersSticksAndYInversion) {
    dualshock4_input_report_common in = neutral();
    in.z = 200;
    in.rz = 17;
    in.x = 255;  // full right
    in.y = 0;    // full up on the DS4 (Y down is positive) -> positive on Xbox
    uint8_t out[XBOX360_REPORT_SIZE];
    build_xbox360_report(in, out);
    EXPECT_EQ(out[4], 200);
    EXPECT_EQ(out[5], 17);
    EXPECT_EQ(le16(&out[6]), 32639);
    EXPECT_EQ(le16(&out[8]), 32767);
    EXPECT_EQ(le16(&out[10]), 0);
    EXPECT_EQ(le16(&out[12]), 0);
}

TEST(Xbox360Report, DpadComesFromHatNibble) {
    dualshock4_input_report_common in = neutral();
    in.buttons[0] = 0x02; // right
    uint8_t out[XBOX360_REPORT_SIZE];
    build_xbox360_report(in, out);
    EXPECT_EQ(out[2] & 0x0F, 0x08);
}

// ------------------------------------------------------------------ GIP report

TEST(GipReport, HeaderAndSizes) {
    uint8_t out[FFS_PACKET_MAX];
    EXPECT_EQ(build_gip_input_report(neutral(), false, 7, out), GIP_ONE_REPORT_SIZE);
    EXPECT_EQ(out[0], 0x20);
    EXPECT_EQ(out[2], 7);
    EXPECT_EQ(out[3], GIP_ONE_REPORT_SIZE - 4);

    EXPECT_EQ(build_gip_input_report(neutral(), true, 9, out), GIP_SERIES_REPORT_SIZE);
    EXPECT_EQ(out[3], GIP_SERIES_REPORT_SIZE - 4);
}

TEST(GipReport, ButtonsTriggersAndSticks) {
    dualshock4_input_report_common in = neutral();
    in.buttons[0] |= 0x20 | 0x80; // Cross, Triangle
    in.buttons[1] |= 0x20 | 0x01; // Options, L1
    in.z = 255;
    in.rz = 0;
    in.x = 0;
    uint8_t out[FFS_PACKET_MAX];
    build_gip_input_report(in, false, 0, out);
    EXPECT_EQ(out[4] & 0x04, 0x04); // Menu
    EXPECT_EQ(out[4] & 0x10, 0x10); // A
    EXPECT_EQ(out[4] & 0x80, 0x80); // Y
    EXPECT_EQ(out[5] & 0x10, 0x10); // LB
    EXPECT_EQ(le16(&out[6]), 1023);
    EXPECT_EQ(le16(&out[8]), 0);
    EXPECT_EQ(le16(&out[10]), -32768);
}

TEST(GipReport, TouchpadMapsToShareOnSeriesAndViewOnOne) {
    dualshock4_input_report_common in = neutral();
    in.buttons[2] |= 0x02;
    uint8_t out[FFS_PACKET_MAX];

    build_gip_input_report(in, false, 0, out);
    EXPECT_EQ(out[4] & 0x08, 0x08);

    build_gip_input_report(in, true, 0, out);
    EXPECT_EQ(out[4] & 0x08, 0);
    EXPECT_EQ(out[GIP_SERIES_SHARE_OFFSET] & 0x01, 0x01);
}

TEST(GipReport, UnusedBytesAreZeroed) {
    uint8_t out[FFS_PACKET_MAX];
    std::memset(out, 0xFF, sizeof(out));
    const size_t size = build_gip_input_report(neutral(), false, 0, out);
    for (size_t i = 18; i < size; ++i) EXPECT_EQ(out[i], 0);
}

namespace {

// 36 calibration bytes where every byte is its own index, so a moved byte is
// visible in the output.
void fill_payload(uint8_t payload[DS4_CALIBRATION_SIZE]) {
    for (size_t i = 0; i < DS4_CALIBRATION_SIZE; ++i) payload[i] = (uint8_t)(0x10 + i);
}

std::vector<uint8_t> bluetooth_calibration_report(const uint8_t payload[DS4_CALIBRATION_SIZE]) {
    std::vector<uint8_t> report(41);
    report[0] = 0x05;
    std::memcpy(&report[1], payload, DS4_CALIBRATION_SIZE);
    const uint32_t crc = calculate_crc32(0xA3, report.data(), 37);
    for (int i = 0; i < 4; ++i) report[37 + i] = (uint8_t)(crc >> (8 * i));
    return report;
}

TEST(Ds4Calibration, UsbReportIsCopiedUnchanged) {
    uint8_t payload[DS4_CALIBRATION_SIZE];
    fill_payload(payload);
    std::vector<uint8_t> report(37);
    report[0] = 0x02;
    std::memcpy(&report[1], payload, sizeof(payload));

    uint8_t out[DS4_CALIBRATION_SIZE] = {};
    ASSERT_TRUE(ds4_calibration_to_usb_layout(report.data(), report.size(), out));
    EXPECT_EQ(0, std::memcmp(out, payload, sizeof(payload)));
}

TEST(Ds4Calibration, BluetoothGyroWordsAreReorderedToTheUsbLayout) {
    uint8_t payload[DS4_CALIBRATION_SIZE];
    fill_payload(payload);
    const auto report = bluetooth_calibration_report(payload);

    uint8_t out[DS4_CALIBRATION_SIZE] = {};
    ASSERT_TRUE(ds4_calibration_to_usb_layout(report.data(), report.size(), out));

    // The biases and everything after the gyro block do not move.
    EXPECT_EQ(0, std::memcmp(out, payload, 6));
    EXPECT_EQ(0, std::memcmp(out + 18, payload + 18, DS4_CALIBRATION_SIZE - 18));
    // Bluetooth words (p+, p-, y+, y-, r+, r-) become USB (p+, y+, r+, p-, y-, r-).
    const size_t bt_word_for_usb_word[6] = {0, 2, 4, 1, 3, 5};
    for (size_t usb_word = 0; usb_word < 6; ++usb_word) {
        EXPECT_EQ(0, std::memcmp(out + 6 + 2 * usb_word, payload + 6 + 2 * bt_word_for_usb_word[usb_word], 2))
            << "USB word " << usb_word;
    }
}

TEST(Ds4Calibration, BluetoothReportWithABadCrcIsRejected) {
    uint8_t payload[DS4_CALIBRATION_SIZE];
    fill_payload(payload);
    auto report = bluetooth_calibration_report(payload);
    report[10] ^= 0xFF;

    uint8_t out[DS4_CALIBRATION_SIZE] = {0xEE};
    EXPECT_FALSE(ds4_calibration_to_usb_layout(report.data(), report.size(), out));
    EXPECT_EQ(0xEE, out[0]); // untouched on failure
}

TEST(Ds4Calibration, WrongIdsAndSizesAreRejected) {
    std::vector<uint8_t> report(41, 0);
    uint8_t out[DS4_CALIBRATION_SIZE];
    EXPECT_FALSE(ds4_calibration_to_usb_layout(nullptr, 37, out));
    EXPECT_FALSE(ds4_calibration_to_usb_layout(report.data(), 37, out));  // id 0x00, not 0x02
    report[0] = 0x02;
    EXPECT_FALSE(ds4_calibration_to_usb_layout(report.data(), 41, out));  // USB id, BT size
    EXPECT_FALSE(ds4_calibration_to_usb_layout(report.data(), 36, out));  // short
    report[0] = 0x05;
    EXPECT_FALSE(ds4_calibration_to_usb_layout(report.data(), 37, out));  // BT id, USB size
}

} // namespace
