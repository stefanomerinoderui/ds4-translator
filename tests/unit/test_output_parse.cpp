// The kernel backend's reading of the output reports a game sends the
// emulated pad (kernel/ds4t-translate.c: ds4t_parse_output), in the layouts
// hid-playstation, SDL and Steam produce.
#include <gtest/gtest.h>

#include <vector>

extern "C" {
#include "ds4t-translate.h"
}

namespace {

ds4t_output parse(bool dualsense, const std::vector<uint8_t>& r, unsigned int& found) {
    ds4t_output out{1, 2, 3, 4, 5}; // untouched fields stay as they were
    found = ds4t_parse_output(dualsense, r.data(), r.size(), &out);
    return out;
}

// DualShock 4: report id, then (Bluetooth only) hw control and audio bytes,
// then flags, flags, reserved, right motor, left motor, r, g, b.
std::vector<uint8_t> ds4_report(bool bluetooth, uint8_t flags) {
    std::vector<uint8_t> r(bluetooth ? 78 : 32, 0);
    const size_t c = bluetooth ? 3 : 1;
    r[0] = bluetooth ? 0x11 : 0x05;
    r[c] = flags;
    r[c + 3] = 0xAA; // right (weak)
    r[c + 4] = 0x55; // left (strong)
    r[c + 5] = 10;
    r[c + 6] = 20;
    r[c + 7] = 30;
    return r;
}

// DualSense: report id, then (Bluetooth only) sequence tag and tag, then the
// common block: flag0, flag1, right, left, ..., flag2 at 38, r g b at 44.
std::vector<uint8_t> ds5_report(bool bluetooth, uint8_t flag0, uint8_t flag1, uint8_t flag2) {
    std::vector<uint8_t> r(bluetooth ? 78 : 48, 0);
    const size_t c = bluetooth ? 3 : 1;
    r[0] = bluetooth ? 0x31 : 0x02;
    r[c] = flag0;
    r[c + 1] = flag1;
    r[c + 2] = 0xAA;
    r[c + 3] = 0x55;
    r[c + 38] = flag2;
    r[c + 44] = 10;
    r[c + 45] = 20;
    r[c + 46] = 30;
    return r;
}

} // namespace

TEST(OutputParse, Ds4UsbAndBluetoothLayoutsGiveTheSameState) {
    for (bool bt : {false, true}) {
        unsigned int found = 0;
        const ds4t_output out = parse(false, ds4_report(bt, 0x03), found);
        EXPECT_EQ(found, unsigned(DS4T_OUT_RUMBLE | DS4T_OUT_LED)) << "bt=" << bt;
        EXPECT_EQ(out.motor_right, 0xAA);
        EXPECT_EQ(out.motor_left, 0x55);
        EXPECT_EQ(out.red, 10);
        EXPECT_EQ(out.green, 20);
        EXPECT_EQ(out.blue, 30);
    }
}

TEST(OutputParse, Ds4FlagsSelectWhatIsRead) {
    unsigned int found = 0;
    ds4t_output out = parse(false, ds4_report(false, 0x01), found);
    EXPECT_EQ(found, unsigned(DS4T_OUT_RUMBLE));
    EXPECT_EQ(out.red, 3); // LED untouched
    out = parse(false, ds4_report(false, 0x02), found);
    EXPECT_EQ(found, unsigned(DS4T_OUT_LED));
    EXPECT_EQ(out.motor_right, 2); // motors untouched
}

// Firmware 2.21 and later: hid-playstation and SDL ask for rumble through
// flag2 bit 2 ("compatible vibration 2") and leave flag0 bit 0 clear.
TEST(OutputParse, DualSenseVibrationEitherFlagInEitherLayout) {
    for (bool bt : {false, true}) {
        for (auto flags : {std::pair<uint8_t, uint8_t>{0x01, 0x00}, {0x00, 0x04}}) {
            unsigned int found = 0;
            const ds4t_output out = parse(true, ds5_report(bt, flags.first, 0x04, flags.second), found);
            EXPECT_EQ(found, unsigned(DS4T_OUT_RUMBLE | DS4T_OUT_LED)) << "bt=" << bt;
            EXPECT_EQ(out.motor_right, 0xAA);
            EXPECT_EQ(out.motor_left, 0x55);
            EXPECT_EQ(out.blue, 30);
        }
    }
}

TEST(OutputParse, DualSenseHapticsSelectKeepsOnlyTheWeakMotor) {
    unsigned int found = 0;
    const ds4t_output out = parse(true, ds5_report(false, 0x03, 0x00, 0x00), found);
    EXPECT_EQ(out.motor_right, 0xAA);
    EXPECT_EQ(out.motor_left, 0);
}

TEST(OutputParse, OtherReportsAndShortOnesAreIgnored) {
    unsigned int found = 1;
    auto r = ds4_report(false, 0x03);
    r[0] = 0x02; // a DualSense id sent to a DS4
    parse(false, r, found);
    EXPECT_EQ(found, 0u);
    r = ds4_report(true, 0x03);
    r.resize(10); // Bluetooth layout cut short of the light bar
    parse(false, r, found);
    EXPECT_EQ(found, 0u);
    auto s = ds5_report(false, 0x01, 0x04, 0x00);
    s.resize(47);
    parse(true, s, found);
    EXPECT_EQ(found, 0u);
    parse(true, {}, found);
    EXPECT_EQ(found, 0u);
}
