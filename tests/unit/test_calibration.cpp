// Unit tests for src/calibration.cpp against a fake pad that follows the
// ds4-tools protocol: flash-mirror mode, 0x90 control, 0x91/0x92 status.
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "calibration.h"

namespace {

using Report = std::vector<uint8_t>;

class FakePad : public FeatureIo {
public:
    bool temporary = true;       // flash-mirror mode
    uint8_t state = 0;           // 0x91 state byte
    uint8_t result = 0;          // 0x92 result byte
    bool reject_sets = false;
    bool answer_mode_read = true;
    bool store_succeeds = true;
    std::vector<Report> sent;    // every report written, in order
    uint16_t selected_offset = 0;
    bool mode_was_permanent_during_store = false;
    uint8_t open_device = 0;     // nonzero: 0x91 reports this device instead

    int get(uint8_t id, uint8_t* buf, size_t len) override {
        Report r;
        if (id == 0x11 && answer_mode_read) {
            r = {0x11, (uint8_t)(selected_offset == 12 && temporary ? 1 : 0), 0};
        } else if (id == 0x91) {
            r = {0x91, (uint8_t)(open_device ? open_device : 1), last_target, state};
        } else if (id == 0x92) {
            r = {0x92, 1, last_target, result};
        } else {
            return -1;
        }
        const size_t n = std::min(len, r.size());
        std::memcpy(buf, r.data(), n);
        return (int)n;
    }

    bool set(const uint8_t* buf, size_t len) override {
        if (reject_sets) return false;
        sent.emplace_back(buf, buf + len);
        if (buf[0] == 0x08) selected_offset = (uint16_t)((buf[2] << 8) | buf[3]);
        if (buf[0] == 0xA0 && buf[1] == 0x0A) temporary = (buf[2] == 1);
        if (buf[0] == 0x90) {
            last_target = buf[3];
            if (buf[1] == 1) { state = 1; result = 0xFF; }
            if (buf[1] == 2) {
                mode_was_permanent_during_store = !temporary;
                state = 2;
                result = store_succeeds ? 1 : 0;
            }
        }
        return true;
    }

private:
    uint8_t last_target = 1;
};

} // namespace

TEST(CalibrationCommand, StickCenterReports) {
    uint8_t out[CAL_COMMAND_MAX];
    ASSERT_EQ(build_calibration_command(CAL_STICK_CENTER, CAL_STEP_START, 0, 0, out), 4u);
    EXPECT_EQ(Report(out, out + 4), (Report{0x90, 1, 1, 1}));
    ASSERT_EQ(build_calibration_command(CAL_STICK_CENTER, CAL_STEP_SAMPLE, 0, 0, out), 4u);
    EXPECT_EQ(Report(out, out + 4), (Report{0x90, 3, 1, 1}));
    ASSERT_EQ(build_calibration_command(CAL_STICK_CENTER, CAL_STEP_STORE, 0, 0, out), 4u);
    EXPECT_EQ(Report(out, out + 4), (Report{0x90, 2, 1, 1}));
}

TEST(CalibrationCommand, StickRangeHasNoSampleStep) {
    uint8_t out[CAL_COMMAND_MAX];
    ASSERT_EQ(build_calibration_command(CAL_STICK_RANGE, CAL_STEP_START, 0, 0, out), 4u);
    EXPECT_EQ(Report(out, out + 4), (Report{0x90, 1, 1, 2}));
    EXPECT_EQ(build_calibration_command(CAL_STICK_RANGE, CAL_STEP_SAMPLE, 0, 0, out), 0u);
}

TEST(CalibrationCommand, TriggerReports) {
    uint8_t out[CAL_COMMAND_MAX];
    ASSERT_EQ(build_calibration_command(CAL_TRIGGERS, CAL_STEP_START, 0, 0, out), 5u);
    EXPECT_EQ(Report(out, out + 5), (Report{0x90, 1, 3, 0, 3}));
    ASSERT_EQ(build_calibration_command(CAL_TRIGGERS, CAL_STEP_SAMPLE, 2, 1, out), 5u);
    EXPECT_EQ(Report(out, out + 5), (Report{0x90, 3, 3, 2, 1}));
    ASSERT_EQ(build_calibration_command(CAL_TRIGGERS, CAL_STEP_STORE, 0, 0, out), 5u);
    EXPECT_EQ(Report(out, out + 5), (Report{0x90, 2, 3, 0, 3}));
    EXPECT_EQ(build_calibration_command(CAL_TRIGGERS, CAL_STEP_SAMPLE, 0, 1, out), 0u);
    EXPECT_EQ(build_calibration_command(CAL_TRIGGERS, CAL_STEP_SAMPLE, 4, 1, out), 0u);
    EXPECT_EQ(build_calibration_command(CAL_TRIGGERS, CAL_STEP_SAMPLE, 1, 3, out), 0u);
}

TEST(CalibrationCommand, FlashMirrorAndResetReports) {
    uint8_t out[FLASH_REPORT_MAX];
    ASSERT_EQ(build_flash_select_report(12, out), 4u);
    EXPECT_EQ(Report(out, out + 4), (Report{0x08, 0xFF, 0x00, 0x0C}));
    ASSERT_EQ(build_flash_mode_report(false, out), 4u);
    EXPECT_EQ(Report(out, out + 4), (Report{0xA0, 0x0A, 0x01, 0x00}));
    ASSERT_EQ(build_flash_mode_report(true, out), 7u);
    EXPECT_EQ(Report(out, out + 7), (Report{0xA0, 0x0A, 0x02, 0x3E, 0x71, 0x7F, 0x89}));
    ASSERT_EQ(build_reset_report(out), 4u);
    EXPECT_EQ(Report(out, out + 4), (Report{0xA0, 0x04, 0x01, 0x00}));
}

TEST(CalibrationTarget, ParsesNames) {
    CalibrationTarget t;
    EXPECT_TRUE(parse_calibration_target("stick-range", t));
    EXPECT_EQ(t, CAL_STICK_RANGE);
    EXPECT_FALSE(parse_calibration_target("gyro", t));
}

TEST(CalibrationRun, StartForcesTemporaryModeThenStarts) {
    FakePad pad;
    pad.temporary = false; // left permanent by something else
    const std::string reply = calibration_run_step(pad, CAL_STICK_CENTER, CAL_STEP_START, 0, 0, false);
    EXPECT_EQ(reply.rfind("OK:", 0), 0u) << reply;
    EXPECT_TRUE(pad.temporary);
    EXPECT_EQ(pad.sent.back(), (Report{0x90, 1, 1, 1}));
}

TEST(CalibrationRun, StartRefusesWhenTemporaryModeIsNotConfirmed) {
    FakePad pad;
    pad.answer_mode_read = false;
    const std::string reply = calibration_run_step(pad, CAL_STICK_CENTER, CAL_STEP_START, 0, 0, false);
    EXPECT_EQ(reply.rfind("Error:", 0), 0u) << reply;
    for (const Report& r : pad.sent) EXPECT_NE(r[0], 0x90) << "calibration must not start";
}

TEST(CalibrationRun, StartExplainsAnUnfinishedTriggerCalibration) {
    FakePad pad;
    pad.open_device = 3; // triggers, left open by a cancelled run
    const std::string reply = calibration_run_step(pad, CAL_STICK_RANGE, CAL_STEP_START, 0, 0, false);
    EXPECT_EQ(reply.rfind("Error:", 0), 0u);
    EXPECT_NE(reply.find("trigger calibration is still open"), std::string::npos);
    EXPECT_NE(reply.find("reset-controller"), std::string::npos);
}

TEST(CalibrationRun, StartFailsWhenPadRejectsReports) {
    FakePad pad;
    pad.reject_sets = true;
    EXPECT_EQ(calibration_run_step(pad, CAL_STICK_RANGE, CAL_STEP_START, 0, 0, false).rfind("Error:", 0), 0u);
}

TEST(CalibrationRun, TemporaryStoreNeverEntersPermanentMode) {
    FakePad pad;
    ASSERT_EQ(calibration_run_step(pad, CAL_STICK_CENTER, CAL_STEP_START, 0, 0, false).rfind("OK:", 0), 0u);
    const std::string reply = calibration_run_step(pad, CAL_STICK_CENTER, CAL_STEP_STORE, 0, 0, false);
    EXPECT_EQ(reply.rfind("OK:", 0), 0u) << reply;
    EXPECT_FALSE(pad.mode_was_permanent_during_store);
    EXPECT_TRUE(pad.temporary);
}

TEST(CalibrationRun, PermanentStoreSwitchesModeOnlyAroundTheStore) {
    FakePad pad;
    ASSERT_EQ(calibration_run_step(pad, CAL_STICK_RANGE, CAL_STEP_START, 0, 0, false).rfind("OK:", 0), 0u);
    const std::string reply = calibration_run_step(pad, CAL_STICK_RANGE, CAL_STEP_STORE, 0, 0, true);
    EXPECT_EQ(reply.rfind("OK:", 0), 0u) << reply;
    EXPECT_TRUE(pad.mode_was_permanent_during_store);
    EXPECT_TRUE(pad.temporary) << "pad must be back in temporary mode afterwards";
}

TEST(CalibrationRun, FailedStoreIsReportedAndStillRestoresTemporaryMode) {
    FakePad pad;
    pad.store_succeeds = false;
    ASSERT_EQ(calibration_run_step(pad, CAL_STICK_CENTER, CAL_STEP_START, 0, 0, false).rfind("OK:", 0), 0u);
    const std::string reply = calibration_run_step(pad, CAL_STICK_CENTER, CAL_STEP_STORE, 0, 0, true);
    EXPECT_EQ(reply.rfind("Error:", 0), 0u) << reply;
    EXPECT_TRUE(pad.temporary);
}

TEST(CalibrationRun, StoreWithoutStartIsRefused) {
    FakePad pad;
    const std::string reply = calibration_run_step(pad, CAL_STICK_CENTER, CAL_STEP_STORE, 0, 0, true);
    EXPECT_EQ(reply.rfind("Error:", 0), 0u) << reply;
    EXPECT_TRUE(pad.sent.empty()) << "nothing may be written to a pad that is not calibrating";
}

TEST(CalibrationRun, SampleChecksItsArguments) {
    FakePad pad;
    EXPECT_EQ(calibration_run_step(pad, CAL_STICK_RANGE, CAL_STEP_SAMPLE, 0, 0, false).rfind("Error:", 0), 0u);
    EXPECT_EQ(calibration_run_step(pad, CAL_TRIGGERS, CAL_STEP_SAMPLE, 9, 1, false).rfind("Error:", 0), 0u);
    EXPECT_EQ(calibration_run_step(pad, CAL_TRIGGERS, CAL_STEP_SAMPLE, 3, 2, false).rfind("OK:", 0), 0u);
    EXPECT_EQ(pad.sent.back(), (Report{0x90, 3, 3, 3, 2}));
}

TEST(ControllerReset, SendsTheResetReport) {
    FakePad pad;
    EXPECT_EQ(controller_reset(pad).rfind("OK:", 0), 0u);
    EXPECT_EQ(pad.sent.back(), (Report{0xA0, 0x04, 0x01, 0x00}));
    pad.reject_sets = true;
    EXPECT_EQ(controller_reset(pad).rfind("Error:", 0), 0u);
}
