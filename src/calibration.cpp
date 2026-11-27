#include "calibration.h"

#include <string.h>

#include <string>

namespace {

constexpr uint8_t kCalibrationControl = 0x90;
constexpr uint8_t kCalibrationState = 0x91;
constexpr uint8_t kCalibrationResult = 0x92;
constexpr uint8_t kFlashSelect = 0x08;
constexpr uint8_t kFlashRead = 0x11;
constexpr uint8_t kSystem = 0xA0;

constexpr uint8_t kDeviceSticks = 1;
constexpr uint8_t kDeviceTriggers = 3;
constexpr uint8_t kStateRunning = 1;
constexpr uint8_t kStateStored = 2;
constexpr uint8_t kResultPending = 0xFF;
constexpr uint8_t kResultSuccess = 1;

// Flash-mirror offset of the temporary (1) / permanent (0) flag.
constexpr uint16_t kModeFlagOffset = 12;

uint8_t stick_target(CalibrationTarget target) { return target == CAL_STICK_CENTER ? 1 : 2; }

std::string error(const std::string& what) { return "Error: " + what; }

// 0x91/0x92 echo `device, target` followed by a state/result byte.
bool read_status(FeatureIo& io, uint8_t id, CalibrationTarget target, uint8_t& value) {
    uint8_t buf[4] = {};
    if (io.get(id, buf, sizeof(buf)) < (int)sizeof(buf) || buf[0] != id) return false;
    if (buf[1] != kDeviceSticks || buf[2] != stick_target(target)) return false;
    value = buf[3];
    return true;
}

bool send(FeatureIo& io, const uint8_t* report, size_t len) { return len > 0 && io.set(report, len); }

// Reads the flash-mirror mode flag into `temporary` (true = changes are lost
// on reset); returns false if the pad did not answer.
bool read_temporary_mode(FeatureIo& io, bool& temporary) {
    uint8_t select[FLASH_REPORT_MAX];
    if (!send(io, select, build_flash_select_report(kModeFlagOffset, select))) return false;
    uint8_t word[3] = {};
    if (io.get(kFlashRead, word, sizeof(word)) < (int)sizeof(word) || word[0] != kFlashRead) return false;
    temporary = word[1] == 1;
    return true;
}

bool set_mode(FeatureIo& io, bool permanent) {
    uint8_t report[FLASH_REPORT_MAX];
    return send(io, report, build_flash_mode_report(permanent, report));
}

// Why a stick calibration did not start. The usual cause is a calibration
// that was started and never stored (a cancelled trigger run, say): the pad
// keeps it open until it resets, and it has a battery, so unplugging it does
// not count. Only a reset clears it.
std::string start_refused(FeatureIo& io) {
    const std::string fix = " Reset the controller (ds4-ctl reset-controller, or Reset controller on the Calibrate tab) "
                            "and start again.";
    uint8_t raw[4] = {};
    if (io.get(kCalibrationState, raw, sizeof(raw)) < (int)sizeof(raw) || raw[0] != kCalibrationState) {
        return error("the pad did not report calibration as running." + fix);
    }
    if (raw[1] == kDeviceTriggers) {
        return error("a trigger calibration is still open on the controller (it was started and not stored)." + fix);
    }
    return error("the pad did not report calibration as running (it reports device " + std::to_string(raw[1]) +
                 ", target " + std::to_string(raw[2]) + ", state " + std::to_string(raw[3]) + ")." + fix);
}

std::string start_step(FeatureIo& io, CalibrationTarget target) {
    // Never start from a pad left in permanent mode: that would make a
    // half-finished or wrong calibration stick.
    if (!set_mode(io, false)) return error("could not switch the flash mirror to temporary mode");
    bool temporary = false;
    if (!read_temporary_mode(io, temporary) || !temporary) {
        return error("the pad did not confirm temporary flash-mirror mode; refusing to calibrate");
    }
    uint8_t command[CAL_COMMAND_MAX];
    if (!send(io, command, build_calibration_command(target, CAL_STEP_START, 0, 0, command))) {
        return error("the pad rejected the calibration start command");
    }
    if (target != CAL_TRIGGERS) {
        uint8_t state = 0;
        if (!read_status(io, kCalibrationState, target, state) || state != kStateRunning) {
            return start_refused(io);
        }
    }
    return std::string("OK: ") + calibration_target_name(target) + " calibration started (temporary mode).";
}

std::string sample_step(FeatureIo& io, CalibrationTarget target, uint8_t position, uint8_t side) {
    uint8_t command[CAL_COMMAND_MAX];
    const size_t len = build_calibration_command(target, CAL_STEP_SAMPLE, position, side, command);
    if (len == 0) return error("this calibration target has no sample step, or the trigger position/side is invalid");
    if (!io.set(command, len)) return error("the pad rejected the sample command");
    return "OK: sample taken.";
}

std::string store_step(FeatureIo& io, CalibrationTarget target, bool permanent) {
    if (target != CAL_TRIGGERS) {
        uint8_t state = 0;
        uint8_t result = 0;
        if (!read_status(io, kCalibrationState, target, state) || state != kStateRunning ||
            !read_status(io, kCalibrationResult, target, result) || result != kResultPending) {
            return error("calibration is not running on the pad; start it first");
        }
    }
    if (permanent && !set_mode(io, true)) return error("could not switch the flash mirror to permanent mode");

    uint8_t command[CAL_COMMAND_MAX];
    const bool stored = send(io, command, build_calibration_command(target, CAL_STEP_STORE, 0, 0, command));
    // Back to temporary whether or not the store worked, so a failure can not
    // leave the pad in the mode where writes persist.
    const bool restored = !permanent || set_mode(io, false);
    if (!stored) return error("the pad rejected the store command");
    if (!restored) return error("stored, but the pad could not be switched back to temporary mode");

    if (target != CAL_TRIGGERS) {
        uint8_t state = 0;
        uint8_t result = 0;
        if (!read_status(io, kCalibrationState, target, state) || state != kStateStored ||
            !read_status(io, kCalibrationResult, target, result) || result != kResultSuccess) {
            return error("the pad did not confirm the calibration was stored successfully");
        }
    }
    return std::string("OK: ") + calibration_target_name(target) + " calibration stored" +
           (permanent ? " permanently." : " (temporary: lost on reset or standby).");
}

} // namespace

bool parse_calibration_target(const std::string& name, CalibrationTarget& out) {
    if (name == "stick-center") out = CAL_STICK_CENTER;
    else if (name == "stick-range") out = CAL_STICK_RANGE;
    else if (name == "triggers") out = CAL_TRIGGERS;
    else return false;
    return true;
}

const char* calibration_target_name(CalibrationTarget target) {
    switch (target) {
    case CAL_STICK_CENTER: return "stick-center";
    case CAL_STICK_RANGE: return "stick-range";
    case CAL_TRIGGERS: return "triggers";
    }
    return "";
}

size_t build_calibration_command(CalibrationTarget target, CalibrationStep step,
                                 uint8_t trigger_position, uint8_t trigger_side,
                                 uint8_t out[CAL_COMMAND_MAX]) {
    memset(out, 0, CAL_COMMAND_MAX);
    const uint8_t cmd = step == CAL_STEP_START ? 1 : step == CAL_STEP_STORE ? 2 : 3;
    out[0] = kCalibrationControl;
    out[1] = cmd;
    if (target == CAL_TRIGGERS) {
        out[2] = kDeviceTriggers;
        if (step == CAL_STEP_SAMPLE) {
            if (trigger_position < 1 || trigger_position > 3 || trigger_side < 1 || trigger_side > 2) return 0;
            out[3] = trigger_position;
            out[4] = trigger_side;
        } else {
            out[3] = 0;
            out[4] = 3;
        }
        return 5;
    }
    if (target == CAL_STICK_RANGE && step == CAL_STEP_SAMPLE) return 0;
    out[2] = kDeviceSticks;
    out[3] = stick_target(target);
    return 4;
}

size_t build_flash_select_report(uint16_t offset, uint8_t out[FLASH_REPORT_MAX]) {
    memset(out, 0, FLASH_REPORT_MAX);
    out[0] = kFlashSelect;
    out[1] = 0xFF;
    out[2] = (uint8_t)(offset >> 8);
    out[3] = (uint8_t)(offset & 0xFF);
    return 4;
}

size_t build_flash_mode_report(bool permanent, uint8_t out[FLASH_REPORT_MAX]) {
    memset(out, 0, FLASH_REPORT_MAX);
    out[0] = kSystem;
    out[1] = 0x0A;
    if (!permanent) {
        out[2] = 0x01;
        out[3] = 0x00;
        return 4;
    }
    static const uint8_t kPermanent[] = {0x02, 0x3E, 0x71, 0x7F, 0x89};
    memcpy(out + 2, kPermanent, sizeof(kPermanent));
    return 2 + sizeof(kPermanent);
}

size_t build_reset_report(uint8_t out[FLASH_REPORT_MAX]) {
    memset(out, 0, FLASH_REPORT_MAX);
    out[0] = kSystem;
    out[1] = 0x04;
    out[2] = 0x01;
    out[3] = 0x00;
    return 4;
}

std::string calibration_run_step(FeatureIo& io, CalibrationTarget target, CalibrationStep step,
                                 uint8_t trigger_position, uint8_t trigger_side, bool permanent) {
    switch (step) {
    case CAL_STEP_START: return start_step(io, target);
    case CAL_STEP_SAMPLE: return sample_step(io, target, trigger_position, trigger_side);
    case CAL_STEP_STORE: return store_step(io, target, permanent);
    }
    return error("unknown calibration step");
}

std::string controller_reset(FeatureIo& io) {
    uint8_t report[FLASH_REPORT_MAX];
    if (!send(io, report, build_reset_report(report))) return error("the pad rejected the reset command");
    return "OK: reset sent; the controller will disconnect and reconnect.";
}
