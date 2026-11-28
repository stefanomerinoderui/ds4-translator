#ifndef DS4_CALIBRATION_H
#define DS4_CALIBRATION_H

// Stick/trigger calibration and the flash-mirror mode of a physical
// DualShock 4, driven through its undocumented vendor feature reports (as
// documented by dualshock-tools/ds4-tools). Free of real I/O: the daemon
// passes in a FeatureIo wired to the pad, tests pass a fake.
//
// Everything here is USB only. The reports have not been verified over
// Bluetooth, and a bad write can brick the pad, so every operation starts in
// the temporary flash-mirror mode and only the explicit "store permanent"
// step ever makes a change survive a reset.

#include <stddef.h>
#include <stdint.h>

#include <string>

class FeatureIo {
public:
    virtual ~FeatureIo() = default;
    // Reads feature report `id` into `buf` (report ID first); bytes read or -1.
    virtual int get(uint8_t id, uint8_t* buf, size_t len) = 0;
    // Sends feature report buf[0] (report ID first, `len` includes it).
    virtual bool set(const uint8_t* buf, size_t len) = 0;
};

enum CalibrationTarget { CAL_STICK_CENTER, CAL_STICK_RANGE, CAL_TRIGGERS };
enum CalibrationStep { CAL_STEP_START, CAL_STEP_SAMPLE, CAL_STEP_STORE };

bool parse_calibration_target(const std::string& name, CalibrationTarget& out);
const char* calibration_target_name(CalibrationTarget target);

// The 0x90 calibration-control report, written to `out` (at least 8 bytes);
// returns its length, or 0 if the step makes no sense for the target (range
// has no sample step; a trigger sample needs position 1-3 and side 1 = L2 or
// 2 = R2).
constexpr size_t CAL_COMMAND_MAX = 8;
size_t build_calibration_command(CalibrationTarget target, CalibrationStep step,
                                 uint8_t trigger_position, uint8_t trigger_side,
                                 uint8_t out[CAL_COMMAND_MAX]);

// Flash-mirror reports (feature 0x08 selects a word, 0x11 reads it, 0xA0
// switches between temporary and permanent changes or resets the pad).
constexpr size_t FLASH_REPORT_MAX = 8;
size_t build_flash_select_report(uint16_t offset, uint8_t out[FLASH_REPORT_MAX]);
size_t build_flash_mode_report(bool permanent, uint8_t out[FLASH_REPORT_MAX]);
size_t build_reset_report(uint8_t out[FLASH_REPORT_MAX]);

// Each returns a one-line reply starting "OK:" or "Error:". `trigger_*` only
// matter for the trigger sample step; `permanent` only for the store step.
std::string calibration_run_step(FeatureIo& io, CalibrationTarget target, CalibrationStep step,
                                 uint8_t trigger_position, uint8_t trigger_side, bool permanent);
std::string controller_reset(FeatureIo& io);

#endif // DS4_CALIBRATION_H
