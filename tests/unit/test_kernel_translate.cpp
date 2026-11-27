// The kernel module's translation (kernel/ds4t-translate.c) against the
// daemon's own (src/translate.cpp): same input, byte-identical output. The
// "kernel" backend must present exactly what the uhid/functionfs ones do.
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

extern "C" {
#include "ds4t-translate.h"
}
#include "translate.h"

namespace {

std::vector<uint8_t> usb_report(uint8_t seed, uint8_t num_touch) {
    std::vector<uint8_t> r(64, 0);
    r[0] = 0x01;
    for (size_t i = 1; i < 33; ++i) r[i] = uint8_t(seed * 7 + i * 13);
    r[1 + 4] &= 0xF0;      // keep the d-pad nibble sane; irrelevant to translation
    r[33] = num_touch;
    for (size_t i = 0; i < 27; ++i) r[34 + i] = uint8_t(seed * 3 + i * 5);
    return r;
}

std::vector<uint8_t> bt_report(const std::vector<uint8_t>& usb, uint8_t num_touch, bool good_crc) {
    std::vector<uint8_t> r(78, 0);
    r[0] = 0x11;
    std::memcpy(&r[3], &usb[1], 32);
    r[35] = num_touch;
    for (size_t i = 0; i < 36; ++i) r[36 + i] = uint8_t(i * 11 + usb[2]);
    uint32_t crc = calculate_crc32(0xA1, r.data(), 74);
    if (!good_crc) crc ^= 1;
    for (int i = 0; i < 4; ++i) r[74 + i] = uint8_t(crc >> (8 * i));
    return r;
}

// What main()'s parse block hands emit_input_report().
struct DaemonView {
    dualshock4_input_report_common common;
    uint8_t num_touch;
    dualshock4_touch_report touch[4];
};

DaemonView daemon_parse_usb(const std::vector<uint8_t>& r) {
    DaemonView v{};
    const auto* in = reinterpret_cast<const dualshock4_input_report_usb*>(r.data());
    v.common = in->common;
    v.num_touch = in->num_touch_reports > 3 ? 3 : in->num_touch_reports;
    for (int i = 0; i < v.num_touch; ++i) v.touch[i] = in->touch_reports[i];
    return v;
}

DaemonView daemon_parse_bt(const std::vector<uint8_t>& r) {
    DaemonView v{};
    const auto* in = reinterpret_cast<const dualshock4_input_report_bt*>(r.data());
    v.common = in->common;
    v.num_touch = in->num_touch_reports > 4 ? 4 : in->num_touch_reports;
    for (int i = 0; i < v.num_touch; ++i) v.touch[i] = in->touch_reports[i];
    return v;
}

void expect_same(const DaemonView& d, const ds4t_input& k) {
    ASSERT_EQ(0, std::memcmp(&d.common, k.common, DS4T_COMMON_SIZE));
    ASSERT_EQ(d.num_touch, k.num_touch);
    for (int i = 0; i < d.num_touch; ++i) {
        ASSERT_EQ(0, std::memcmp(&d.touch[i], k.touch[i], DS4T_TOUCH_SIZE)) << "touch " << i;
    }
}

} // namespace

TEST(KernelTranslate, ParseMatchesDaemonOnUsbAndBluetooth) {
    for (uint8_t seed = 0; seed < 8; ++seed) {
        for (uint8_t touches : {0, 1, 2, 3, 4, 9}) {
            const auto usb = usb_report(seed, touches);
            ds4t_input k;
            ASSERT_EQ(0, ds4t_parse_input(usb.data(), usb.size(), &k));
            expect_same(daemon_parse_usb(usb), k);

            const auto bt = bt_report(usb, touches, true);
            ASSERT_EQ(0, ds4t_parse_input(bt.data(), bt.size(), &k));
            expect_same(daemon_parse_bt(bt), k);
        }
    }
}

TEST(KernelTranslate, RejectsBadCrcShortReportsAndUnknownIds) {
    const auto usb = usb_report(1, 1);
    ds4t_input k;
    const auto bad = bt_report(usb, 1, false);
    EXPECT_EQ(-1, ds4t_parse_input(bad.data(), bad.size(), &k));
    EXPECT_EQ(-1, ds4t_parse_input(usb.data(), 63, &k));
    auto bt = bt_report(usb, 1, true);
    EXPECT_EQ(-1, ds4t_parse_input(bt.data(), 77, &k));
    bt[0] = 0x05;
    EXPECT_EQ(-1, ds4t_parse_input(bt.data(), bt.size(), &k));
}

TEST(KernelTranslate, BuildMatchesDaemonForBothTargets) {
    for (uint8_t seed = 0; seed < 8; ++seed) {
        for (uint8_t touches : {0, 1, 2, 3, 4}) {
            for (bool battery : {true, false}) {
                const auto bt = bt_report(usb_report(seed, touches), touches, true);
                ds4t_input k;
                ASSERT_EQ(0, ds4t_parse_input(bt.data(), bt.size(), &k));
                const DaemonView d = daemon_parse_bt(bt);

                uint8_t want[DS_VIRTUAL_REPORT_SIZE], got[DS4T_VIRT_REPORT_SIZE];
                build_ds4_usb_report(d.common, d.num_touch, d.touch, battery, want);
                ds4t_build_ds4(&k, battery, got);
                EXPECT_EQ(0, std::memcmp(want, got, sizeof(want)))
                    << "ds4 seed=" << int(seed) << " touches=" << int(touches) << " battery=" << battery;

                build_dualsense_report(d.common, d.num_touch, d.touch, seed, battery, want);
                ds4t_build_dualsense(&k, seed, battery, got);
                EXPECT_EQ(0, std::memcmp(want, got, sizeof(want)))
                    << "dualsense seed=" << int(seed) << " touches=" << int(touches) << " battery=" << battery;
            }
        }
    }
}

TEST(KernelTranslate, BatteryAndCrcMatchDaemon) {
    for (int status = 0; status < 256; ++status) {
        int percent_d, percent_k;
        BatteryState state_d;
        ds4t_battery_state state_k;
        decode_ds4_battery(uint8_t(status), percent_d, state_d);
        ds4t_decode_ds4_battery(uint8_t(status), &percent_k, &state_k);
        ASSERT_EQ(percent_d, percent_k);
        ASSERT_EQ(int(state_d), int(state_k));
        ASSERT_EQ(encode_dualsense_battery(percent_d, state_d),
                  ds4t_encode_dualsense_battery(percent_k, state_k));
    }
    std::vector<uint8_t> data(100);
    for (size_t i = 0; i < data.size(); ++i) data[i] = uint8_t(i * 31 + 7);
    for (uint8_t seed : {0xA1, 0xA2, 0xA3}) {
        for (size_t len : {size_t(0), size_t(1), size_t(74), data.size()}) {
            EXPECT_EQ(calculate_crc32(seed, data.data(), len), ds4t_crc32(seed, data.data(), len));
        }
    }
}

TEST(KernelTranslate, CalibrationLayoutMatchesDaemon) {
    std::vector<uint8_t> usb(37);
    usb[0] = 0x02;
    for (size_t i = 1; i < usb.size(); ++i) usb[i] = uint8_t(i * 3);
    std::vector<uint8_t> bt(41);
    bt[0] = 0x05;
    for (size_t i = 1; i < 37; ++i) bt[i] = uint8_t(i * 5 + 1);
    const uint32_t crc = calculate_crc32(0xA3, bt.data(), 37);
    for (int i = 0; i < 4; ++i) bt[37 + i] = uint8_t(crc >> (8 * i));

    for (const auto* report : {&usb, &bt}) {
        uint8_t want[DS4_CALIBRATION_SIZE], got[DS4T_CALIBRATION_SIZE];
        ASSERT_TRUE(ds4_calibration_to_usb_layout(report->data(), report->size(), want));
        ASSERT_EQ(0, ds4t_calibration_to_usb_layout(report->data(), report->size(), got));
        EXPECT_EQ(0, std::memcmp(want, got, sizeof(want)));
    }
    bt[5] ^= 1; // corrupt: CRC no longer matches
    uint8_t out[DS4T_CALIBRATION_SIZE];
    EXPECT_EQ(-1, ds4t_calibration_to_usb_layout(bt.data(), bt.size(), out));
}
