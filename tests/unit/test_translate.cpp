// Unit tests for src/translate.cpp: controller types, CRC32, battery
// re-encoding and the Xbox 360 / GIP report builders.
#include <gtest/gtest.h>

#include <cstring>
#include <string>

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

// ------------------------------------------------------ Bluetooth basic report

TEST(BtBasicReport, MapsSticksButtonsAndTriggers) {
    const uint8_t report[BT_BASIC_REPORT_SIZE] = {0x01, 10, 20, 30, 40, 0x58, 0xA1, 0x03, 200, 100};
    dualshock4_input_report_common c;
    ASSERT_TRUE(parse_bt_basic_input_report(report, sizeof(report), c));
    EXPECT_EQ(c.x, 10);
    EXPECT_EQ(c.y, 20);
    EXPECT_EQ(c.rx, 30);
    EXPECT_EQ(c.ry, 40);
    EXPECT_EQ(c.buttons[0], 0x58);
    EXPECT_EQ(c.buttons[1], 0xA1);
    EXPECT_EQ(c.buttons[2], 0x03);
    EXPECT_EQ(c.z, 200);
    EXPECT_EQ(c.rz, 100);
    EXPECT_EQ(c.gyro[0], 0);
    EXPECT_EQ(c.status[0], 0);
}

TEST(BtBasicReport, RejectsWrongIdOrLength) {
    uint8_t report[BT_BASIC_REPORT_SIZE] = {0x01};
    dualshock4_input_report_common c;
    EXPECT_FALSE(parse_bt_basic_input_report(report, BT_BASIC_REPORT_SIZE - 1, c));
    EXPECT_FALSE(parse_bt_basic_input_report(report, BT_BASIC_REPORT_SIZE + 1, c));
    report[0] = 0x11;
    EXPECT_FALSE(parse_bt_basic_input_report(report, BT_BASIC_REPORT_SIZE, c));
}

// --------------------------------------------------------------- touch packets

namespace {
void fill_touch(dualshock4_touch_report in[4]) {
    std::memset(in, 0, 4 * sizeof(in[0]));
    for (int i = 0; i < 4; ++i) {
        in[i].timestamp = static_cast<uint8_t>(i + 1);
    }
}
} // namespace

TEST(SelectUsbTouchReports, FourPacketsKeepsNewestThree) {
    dualshock4_touch_report in[4], out[3];
    fill_touch(in);
    ASSERT_EQ(select_usb_touch_reports(4, in, out), 3u);
    EXPECT_EQ(out[0].timestamp, 2);
    EXPECT_EQ(out[1].timestamp, 3);
    EXPECT_EQ(out[2].timestamp, 4);
}

TEST(SelectUsbTouchReports, FewerThanThreeAreCopiedInOrder) {
    dualshock4_touch_report in[4], out[3];
    fill_touch(in);
    ASSERT_EQ(select_usb_touch_reports(2, in, out), 2u);
    EXPECT_EQ(out[0].timestamp, 1);
    EXPECT_EQ(out[1].timestamp, 2);
    EXPECT_EQ(select_usb_touch_reports(0, in, out), 0u);
}

TEST(SelectUsbTouchReports, OversizedCountIsClampedToFour) {
    dualshock4_touch_report in[4], out[3];
    fill_touch(in);
    ASSERT_EQ(select_usb_touch_reports(200, in, out), 3u);
    EXPECT_EQ(out[2].timestamp, 4);
}

// -------------------------------------------------------------- firmware info

namespace {
// What a real pad answers feature 0xA3 with: ID, date, time, hw, fw, series, size.
void make_firmware_report(uint8_t out[DS4_FIRMWARE_REPORT_SIZE]) {
    std::memset(out, 0, DS4_FIRMWARE_REPORT_SIZE);
    out[0] = DS4_FIRMWARE_REPORT_ID;
    std::memcpy(out + 1, "Sep  2 2020", 11);
    std::memcpy(out + 17, "14:45:10", 8);
    out[33] = 0x00; out[34] = 0x01;                         // hw major 0x0100
    out[35] = 0x38; out[36] = 0x54;                         // hw minor 0x5438
    out[37] = 0x04; out[38] = 0x00; out[39] = 0x00; out[40] = 0x00; // fw major 4
    out[41] = 0x33; out[42] = 0x20;                         // fw minor 0x2033
}
} // namespace

TEST(Firmware, ValidReportIsRecognised) {
    uint8_t report[DS4_FIRMWARE_REPORT_SIZE];
    make_firmware_report(report);
    EXPECT_TRUE(is_valid_ds4_firmware_report(report, sizeof(report)));
}

TEST(Firmware, WrongIdOrLengthIsRejected) {
    uint8_t report[DS4_FIRMWARE_REPORT_SIZE];
    make_firmware_report(report);
    EXPECT_FALSE(is_valid_ds4_firmware_report(report, sizeof(report) - 1));
    report[0] = 0x31;
    EXPECT_FALSE(is_valid_ds4_firmware_report(report, sizeof(report)));
    EXPECT_EQ(describe_ds4_firmware(report, sizeof(report)), "");
}

TEST(Firmware, DescribesDateTimeAndVersions) {
    uint8_t report[DS4_FIRMWARE_REPORT_SIZE];
    make_firmware_report(report);
    EXPECT_EQ(describe_ds4_firmware(report, sizeof(report)),
              "Sep  2 2020 14:45:10, hw 0x0100.5438, fw 0x00000004.2033");
}

// ------------------------------------------------------------- output reports

namespace {
Ds4Output sample_output() {
    Ds4Output o;
    o.motor_left = 11;
    o.motor_right = 22;
    o.r = 1;
    o.g = 2;
    o.b = 3;
    o.flash_on = 40;
    o.flash_off = 50;
    return o;
}
} // namespace

TEST(Ds4OutputReport, UsbLayoutCarriesMotorsColourAndFlash) {
    uint8_t buf[DS4_BT_OUTPUT_SIZE];
    ASSERT_EQ(build_ds4_output_report(false, sample_output(), buf), DS4_USB_OUTPUT_SIZE);
    EXPECT_EQ(buf[0], 0x05);
    EXPECT_EQ(buf[1], 0x07);
    EXPECT_EQ(buf[4], 22); // right
    EXPECT_EQ(buf[5], 11); // left
    EXPECT_EQ(buf[6], 1);
    EXPECT_EQ(buf[7], 2);
    EXPECT_EQ(buf[8], 3);
    EXPECT_EQ(buf[9], 40);
    EXPECT_EQ(buf[10], 50);
}

TEST(Ds4OutputReport, BluetoothLayoutCarriesFlashAndValidCrc) {
    uint8_t buf[DS4_BT_OUTPUT_SIZE];
    ASSERT_EQ(build_ds4_output_report(true, sample_output(), buf), DS4_BT_OUTPUT_SIZE);
    EXPECT_EQ(buf[0], 0x11);
    EXPECT_EQ(buf[1], 0xC0);
    EXPECT_EQ(buf[3], 0x07);
    EXPECT_EQ(buf[6], 22);
    EXPECT_EQ(buf[7], 11);
    EXPECT_EQ(buf[8], 1);
    EXPECT_EQ(buf[10], 3);
    EXPECT_EQ(buf[11], 40);
    EXPECT_EQ(buf[12], 50);
    const uint32_t crc = buf[74] | (buf[75] << 8) | (buf[76] << 16) | (static_cast<uint32_t>(buf[77]) << 24);
    EXPECT_EQ(crc, calculate_crc32(0xA2, buf, 74));
}

TEST(Ds4OutputReport, DefaultIsSteadyLight) {
    uint8_t buf[DS4_BT_OUTPUT_SIZE];
    build_ds4_output_report(true, Ds4Output{}, buf);
    EXPECT_EQ(buf[11], 0);
    EXPECT_EQ(buf[12], 0);
}

// ------------------------------------------------------ IMU calibration (BT)

namespace {
// A Bluetooth 0x05 report whose every 16-bit field holds a distinct value
// (field index + 1), CRC filled in.
void make_bt_calibration(uint8_t out[DS4_IMU_CAL_BT_SIZE]) {
    std::memset(out, 0, DS4_IMU_CAL_BT_SIZE);
    out[0] = DS4_IMU_CAL_BT_ID;
    for (int field = 0; field < 17; ++field) { // fields at bytes 1,3,...,33
        out[1 + field * 2] = static_cast<uint8_t>(field + 1);
    }
    const uint32_t crc = calculate_crc32(0xA3, out, DS4_IMU_CAL_USB_SIZE);
    for (int i = 0; i < 4; ++i) out[DS4_IMU_CAL_USB_SIZE + i] = static_cast<uint8_t>(crc >> (8 * i));
}
uint8_t field_at(const uint8_t* report, size_t byte) { return report[byte]; }
} // namespace

TEST(ImuCalibration, BluetoothGyroGainsAreReorderedToUsbLayout) {
    uint8_t bt[DS4_IMU_CAL_BT_SIZE], usb[DS4_IMU_CAL_USB_SIZE];
    make_bt_calibration(bt);
    ASSERT_TRUE(convert_ds4_imu_calibration_bt(bt, sizeof(bt), usb));
    EXPECT_EQ(usb[0], DS4_IMU_CAL_USB_ID);
    // BT order: pitch+ yaw+ roll+ pitch- yaw- roll- (bytes 7..17); USB: pitch+ pitch- yaw+ yaw- roll+ roll-.
    EXPECT_EQ(field_at(usb, 7), field_at(bt, 7));
    EXPECT_EQ(field_at(usb, 9), field_at(bt, 13));
    EXPECT_EQ(field_at(usb, 11), field_at(bt, 9));
    EXPECT_EQ(field_at(usb, 13), field_at(bt, 15));
    EXPECT_EQ(field_at(usb, 15), field_at(bt, 11));
    EXPECT_EQ(field_at(usb, 17), field_at(bt, 17));
}

TEST(ImuCalibration, BiasesSpeedAndAccelFieldsKeepTheirOffsets) {
    uint8_t bt[DS4_IMU_CAL_BT_SIZE], usb[DS4_IMU_CAL_USB_SIZE];
    make_bt_calibration(bt);
    ASSERT_TRUE(convert_ds4_imu_calibration_bt(bt, sizeof(bt), usb));
    EXPECT_EQ(0, std::memcmp(usb + 1, bt + 1, 6));   // gyro biases
    EXPECT_EQ(0, std::memcmp(usb + 19, bt + 19, 18)); // speed, accelerometer, trailer
}

TEST(ImuCalibration, CorruptedOrMislabelledReportIsRejected) {
    uint8_t bt[DS4_IMU_CAL_BT_SIZE], usb[DS4_IMU_CAL_USB_SIZE];
    make_bt_calibration(bt);
    bt[20] ^= 0xFF; // payload no longer matches the CRC
    EXPECT_FALSE(convert_ds4_imu_calibration_bt(bt, sizeof(bt), usb));
    make_bt_calibration(bt);
    EXPECT_FALSE(convert_ds4_imu_calibration_bt(bt, sizeof(bt) - 1, usb));
    bt[0] = DS4_IMU_CAL_USB_ID;
    EXPECT_FALSE(convert_ds4_imu_calibration_bt(bt, sizeof(bt), usb));
}

TEST(ImuCalibration, UsbReportValidation) {
    uint8_t usb[DS4_IMU_CAL_USB_SIZE] = {DS4_IMU_CAL_USB_ID};
    EXPECT_TRUE(is_valid_ds4_imu_calibration_usb(usb, sizeof(usb)));
    EXPECT_FALSE(is_valid_ds4_imu_calibration_usb(usb, sizeof(usb) - 1));
    usb[0] = 0x05;
    EXPECT_FALSE(is_valid_ds4_imu_calibration_usb(usb, sizeof(usb)));
}
