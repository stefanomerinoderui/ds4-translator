// Unit tests for the control-protocol rules in src/ipc-client.cpp: argument
// validators, command builders and the STATE/TIMING/OUTPUT/NOTE/status wire
// formats. No sockets, no hardware.
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "ipc-client.h"

using namespace ds4ipc;

// ---------------------------------------------------------------- validators

TEST(Validators, AcceptKnownValuesOnly) {
    for (const auto &v : type_values()) EXPECT_TRUE(is_valid_type(v)) << v;
    for (const auto &v : controller_values()) EXPECT_TRUE(is_valid_controller(v)) << v;
    for (const auto &v : backend_values()) EXPECT_TRUE(is_valid_backend(v)) << v;
    for (const auto &v : hide_method_values()) EXPECT_TRUE(is_valid_hide_method(v)) << v;
    for (const auto &v : toggle_values()) EXPECT_TRUE(is_valid_toggle(v)) << v;

    EXPECT_FALSE(is_valid_type(""));
    EXPECT_FALSE(is_valid_type("DS4"));          // case-sensitive
    EXPECT_FALSE(is_valid_type("ds4 "));         // no trimming
    EXPECT_FALSE(is_valid_controller("xbox360")); // set-backend/set-name only take ds4|dualsense
    EXPECT_FALSE(is_valid_toggle("true"));
}

TEST(Validators, JoinValues) {
    EXPECT_EQ(join_values({}), "");
    EXPECT_EQ(join_values({"a"}), "a");
    EXPECT_EQ(join_values({"ds4", "dualsense"}), "ds4 dualsense");
}

TEST(ValidateName, AcceptsOrdinaryNames) {
    EXPECT_EQ(validate_name("My Pad"), "");
    EXPECT_EQ(validate_name(std::string(kMaxNameBytes, 'x')), "");
}

TEST(ValidateName, RejectsEmptyOverlongAndLineBreaks) {
    EXPECT_NE(validate_name(""), "");
    EXPECT_NE(validate_name(std::string(kMaxNameBytes + 1, 'x')), "");
    EXPECT_NE(validate_name("a\nb"), "");
    EXPECT_NE(validate_name("a\rb"), "");
}

TEST(ValidateName, RejectsControlCharacters) {
    for (const char *bad : {"a\tb", "esc\x1b]0;pwn\x07", "del\x7f", "\x01"}) {
        EXPECT_NE(validate_name(bad), "") << bad;
    }
    EXPECT_NE(validate_name(std::string("a\0b", 3)), "");
}

TEST(ValidateName, EnforcesUtf8) {
    EXPECT_EQ(validate_name("Mando de Stef\xc3\xa1n \xe2\x9c\x93 \xf0\x9f\x8e\xae"), ""); // 2-, 3- and 4-byte sequences
    for (const char *bad : {"\xff\xfe", "\xc3", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "a\x80", "\xe2\x82"}) {
        EXPECT_NE(validate_name(bad), "") << bad; // stray/truncated/overlong/surrogate/out-of-range
    }
}

TEST(ReadOnlyCommands, OnlyObservingCommandsAreOpen) {
    EXPECT_TRUE(is_read_only_command("status"));
    EXPECT_TRUE(is_read_only_command(kTestCommand));
    EXPECT_TRUE(is_read_only_command(kTestAllCommand));
    for (const char *cmd : {"", "set-type ds4", "set-name ds4 x", "led 1 2 3 4 5", "identify",
                            "release-physical", "create-virtual ds4", "input 0", "forget-mac",
                            "status ", "STATUS", "test all ", "led --reset"}) {
        EXPECT_FALSE(is_read_only_command(cmd)) << cmd;
    }
}

// ------------------------------------------------------------------ builders

TEST(Builders, ProduceExactWireCommands) {
    std::string err;
    EXPECT_EQ(build_set_type("ds4", &err), "set-type ds4");
    EXPECT_EQ(build_set_backend("dualsense", "functionfs", &err), "set-backend dualsense functionfs");
    EXPECT_EQ(build_set_name("ds4", "Wireless Controller", &err), "set-name ds4 Wireless Controller");
    EXPECT_EQ(build_set_name("ds4", "--reset", &err), "set-name ds4 --reset");
    EXPECT_EQ(build_set_hide_method("unbind", &err), "set-hide-method unbind");
    EXPECT_EQ(build_set_mac_squat("on", &err), "set-mac-squat on");
    EXPECT_EQ(build_led(1, 2, 3, 4, 5, &err), "led 1 2 3 4 5");
    EXPECT_TRUE(err.empty());
}

TEST(Builders, RejectInvalidArgumentsWithMessage) {
    std::string err;
    EXPECT_EQ(build_set_type("nope", &err), "");
    EXPECT_NE(err.find("Invalid type 'nope'"), std::string::npos);
    EXPECT_NE(err.find("ds4 dualsense"), std::string::npos);

    err.clear();
    EXPECT_EQ(build_set_backend("ds4", "tcp", &err), "");
    EXPECT_NE(err.find("Invalid backend"), std::string::npos);

    err.clear();
    EXPECT_EQ(build_set_backend("xbox360", "uhid", &err), "");
    EXPECT_NE(err.find("Invalid controller type"), std::string::npos);

    err.clear();
    EXPECT_EQ(build_set_hide_method("hide", &err), "");
    err.clear();
    EXPECT_EQ(build_set_mac_squat("maybe", &err), "");
    EXPECT_FALSE(err.empty());
}

TEST(Builders, NullErrorPointerIsTolerated) {
    EXPECT_EQ(build_set_type("nope", nullptr), "");
    EXPECT_EQ(build_set_name("ds4", "", nullptr), "");
    EXPECT_EQ(build_led(256, 0, 0, 0, 0, nullptr), "");
}

TEST(Builders, LedRangeIsInclusive0To255) {
    std::string err;
    EXPECT_EQ(build_led(0, 0, 0, 0, 0, &err), "led 0 0 0 0 0");
    EXPECT_EQ(build_led(255, 255, 255, 255, 255, &err), "led 255 255 255 255 255");
    for (int bad : {-1, 256}) {
        EXPECT_EQ(build_led(0, 0, 0, bad, 0, &err), "") << bad;
        EXPECT_EQ(err, "led values must be 0-255");
    }
}

TEST(Builders, NameWithInjectedNewlineNeverReachesTheWire) {
    std::string err;
    EXPECT_EQ(build_set_name("ds4", "x\nset-type none", &err), "");
    EXPECT_FALSE(err.empty());
}

// ------------------------------------------------------------- button decode

TEST(DecodeButtons, DpadIsLowNibble) {
    const uint8_t bytes[3] = {0x03, 0, 0};
    uint16_t buttons = 0xFFFF;
    uint8_t dpad = 0xFF;
    decode_buttons(bytes, &buttons, &dpad);
    EXPECT_EQ(dpad, 3);
    EXPECT_EQ(buttons, 0);
}

TEST(DecodeButtons, EveryBitMapsToItsButton) {
    const struct { int byte; uint8_t mask; uint16_t button; } map[] = {
        {0, 0x10, kBtnSquare}, {0, 0x20, kBtnCross},   {0, 0x40, kBtnCircle}, {0, 0x80, kBtnTriangle},
        {1, 0x01, kBtnL1},     {1, 0x02, kBtnR1},      {1, 0x04, kBtnL2},     {1, 0x08, kBtnR2},
        {1, 0x10, kBtnShare},  {1, 0x20, kBtnOptions}, {1, 0x40, kBtnL3},     {1, 0x80, kBtnR3},
        {2, 0x01, kBtnPs},     {2, 0x02, kBtnTouchpad},
    };
    for (const auto &m : map) {
        uint8_t bytes[3] = {kDpadNeutral, 0, 0};
        bytes[m.byte] |= m.mask;
        uint16_t buttons = 0;
        uint8_t dpad = 0;
        decode_buttons(bytes, &buttons, &dpad);
        EXPECT_EQ(buttons, m.button) << "byte " << m.byte << " mask " << int(m.mask);
        EXPECT_EQ(dpad, kDpadNeutral);
    }
}

TEST(DecodeButtons, NullOutputsAreTolerated) {
    const uint8_t bytes[3] = {0xFF, 0xFF, 0xFF};
    decode_buttons(bytes, nullptr, nullptr);
    uint8_t dpad = 0;
    decode_buttons(bytes, nullptr, &dpad);
    EXPECT_EQ(dpad, 0x0F);
}

TEST(DpadName, CoversAllDirections) {
    EXPECT_STREQ(dpad_name(0), "Up");
    EXPECT_STREQ(dpad_name(2), "Right");
    EXPECT_STREQ(dpad_name(4), "Down");
    EXPECT_STREQ(dpad_name(6), "Left");
    EXPECT_STREQ(dpad_name(7), "Up-Left");
    EXPECT_STREQ(dpad_name(kDpadNeutral), "Neutral");
    EXPECT_STREQ(dpad_name(200), "Neutral");
}

TEST(DecodeTouchPoint, ActiveBitIsInverted) {
    const uint8_t touching[4] = {0x00, 0, 0, 0};
    const uint8_t released[4] = {0x80, 0, 0, 0};
    EXPECT_TRUE(decode_touch_point(touching).active);
    EXPECT_FALSE(decode_touch_point(released).active);
}

TEST(DecodeTouchPoint, Unpacks12BitCoordinates) {
    // x = 0x3A5 (low byte 0xA5, high nibble 0x3), y = 0x2B1 (low nibble 0x1, high byte 0x2B)
    const uint8_t bytes[4] = {0x00, 0xA5, static_cast<uint8_t>(0x03 | (0x1 << 4)), 0x2B};
    const TouchPoint p = decode_touch_point(bytes);
    EXPECT_EQ(p.x, 0x3A5);
    EXPECT_EQ(p.y, 0x2B1);
}

TEST(DecodeTouchPoint, ExtremesStayWithinPadBoundsOrFitIn12Bits) {
    const uint8_t max[4] = {0x00, 0xFF, 0xFF, 0xFF};
    const TouchPoint p = decode_touch_point(max);
    EXPECT_EQ(p.x, 0xFFF);
    EXPECT_EQ(p.y, 0xFFF);
}

// ------------------------------------------------------------------- STATE

namespace {

InputState sample_state() {
    InputState s;
    s.lx = 10; s.ly = 20; s.rx = 30; s.ry = 40; s.l2 = 50; s.r2 = 60;
    s.buttons = kBtnCross | kBtnL1 | kBtnPs | kBtnTouchpad;
    s.dpad = 5;
    s.gyro[0] = -100; s.gyro[1] = 200; s.gyro[2] = -32768;
    s.accel[0] = 32767; s.accel[1] = 0; s.accel[2] = -1;
    s.touch[0] = {true, 1919, 942};
    s.touch[1] = {false, 7, 8};
    s.has_motion = true;
    s.has_touch = true;
    return s;
}

} // namespace

TEST(StateLine, RoundTrip) {
    const InputState in = sample_state();
    const std::string line = format_state_line(kSourceVirtual, in);
    ASSERT_FALSE(line.empty());
    EXPECT_EQ(line.back(), '\n');

    InputState out;
    ASSERT_TRUE(parse_state(line, &out));
    EXPECT_EQ(out.source, kSourceVirtual);
    EXPECT_EQ(out.lx, in.lx); EXPECT_EQ(out.ly, in.ly);
    EXPECT_EQ(out.rx, in.rx); EXPECT_EQ(out.ry, in.ry);
    EXPECT_EQ(out.l2, in.l2); EXPECT_EQ(out.r2, in.r2);
    EXPECT_EQ(out.buttons, in.buttons);
    EXPECT_EQ(out.dpad, in.dpad);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(out.gyro[i], in.gyro[i]);
        EXPECT_EQ(out.accel[i], in.accel[i]);
    }
    EXPECT_TRUE(out.has_motion);
    EXPECT_TRUE(out.has_touch);
    EXPECT_TRUE(out.touch[0].active);
    EXPECT_EQ(out.touch[0].x, 1919);
    EXPECT_EQ(out.touch[0].y, 942);
    EXPECT_FALSE(out.touch[1].active);
    EXPECT_EQ(out.touch[1].x, 7);
}

TEST(StateLine, GoldenFormat) {
    // Wire-format regression guard: front-ends and daemon must agree on this exactly.
    InputState s;
    s.lx = 128; s.ly = 128; s.rx = 128; s.ry = 128;
    s.dpad = kDpadNeutral;
    EXPECT_EQ(format_state_line(kSourcePhysical, s),
              "STATE PHYSICAL 128 128 128 128 0 0 08 00 00 0 0 0 0 0 0 0 0 0 0 0 0\n");
}

TEST(StateLine, ParsesLinesFromOlderDaemons) {
    InputState out;
    // Pre-touch (16 fields): motion only.
    ASSERT_TRUE(parse_state("STATE VIRTUAL 1 2 3 4 5 6 28 00 00 1 2 3 4 5 6", &out));
    EXPECT_TRUE(out.has_motion);
    EXPECT_FALSE(out.has_touch);
    EXPECT_EQ(out.gyro[2], 3);
    EXPECT_EQ(out.accel[0], 4);

    // Pre-motion (10 fields): axes and buttons only.
    out = InputState();
    ASSERT_TRUE(parse_state("STATE PHYSICAL 1 2 3 4 5 6 28 00 00", &out));
    EXPECT_FALSE(out.has_motion);
    EXPECT_FALSE(out.has_touch);
    EXPECT_EQ(out.buttons, kBtnCross);
    EXPECT_EQ(out.dpad, 8);
}

TEST(StateLine, RejectsMalformedAndLeavesOutputUntouched) {
    InputState out;
    out.lx = 99;
    EXPECT_FALSE(parse_state("", &out));
    EXPECT_FALSE(parse_state("STATE", &out));
    EXPECT_FALSE(parse_state("OUTPUT 1 2 3 4 5", &out));
    EXPECT_FALSE(parse_state("STATE VIRTUAL 1 2 3", &out));
    EXPECT_FALSE(parse_state("STATE VIRTUAL 1 2 3 4 5 6 28 00 00 1 2 3", &out)); // 13 fields
    EXPECT_FALSE(parse_state("STATE VIRTUAL a b c d e f 00 00 00", &out));
    EXPECT_FALSE(parse_state("STATE VIRTUAL 1 2 3 4 5 6 28 00 00", nullptr));
    EXPECT_EQ(out.lx, 99);
}

TEST(StateLine, OverlongSourceDoesNotOverflow) {
    InputState out;
    const std::string line = "STATE " + std::string(4096, 'A') + " 1 2 3 4 5 6 28 00 00\n";
    // Must not crash; the 15-char cap means the numeric fields no longer line up.
    (void)parse_state(line, &out);
}

// ------------------------------------------------------------ TIMING/OUTPUT

TEST(TimingLine, RoundTripAndRate) {
    const std::string line = format_timing_line(250, 1000000, 4000, 3900, 4100, 800, 1500);
    EXPECT_EQ(line, "TIMING 250 1000000 4000 3900 4100 800 1500\n");
    TimingStats t;
    ASSERT_TRUE(parse_timing(line, &t));
    EXPECT_EQ(t.reports, 250u);
    EXPECT_EQ(t.window_us, 1000000u);
    EXPECT_EQ(t.interval_mean_us, 4000u);
    EXPECT_EQ(t.interval_min_us, 3900u);
    EXPECT_EQ(t.interval_max_us, 4100u);
    EXPECT_EQ(t.latency_mean_us, 800u);
    EXPECT_EQ(t.latency_max_us, 1500u);
    EXPECT_DOUBLE_EQ(t.reports_per_second(), 250.0);
}

TEST(TimingLine, LargeWindowSurvives64Bit) {
    TimingStats t;
    ASSERT_TRUE(parse_timing(format_timing_line(1, 5000000000ULL, 1, 1, 1, 1, 1), &t));
    EXPECT_EQ(t.window_us, 5000000000ULL);
}

TEST(TimingLine, RateIsZeroWithoutData) {
    TimingStats t;
    EXPECT_DOUBLE_EQ(t.reports_per_second(), 0.0);
    t.reports = 5;
    EXPECT_DOUBLE_EQ(t.reports_per_second(), 0.0); // window still zero
}

TEST(TimingLine, RejectsOtherLines) {
    TimingStats t;
    EXPECT_FALSE(parse_timing("TIMING 1 2 3", &t));
    EXPECT_FALSE(parse_timing("STATE VIRTUAL 1 2 3 4 5 6 28 00 00", &t));
    EXPECT_FALSE(parse_timing("TIMING 1 2 3 4 5 6 7", nullptr));
}

TEST(OutputLine, RoundTrip) {
    OutputState in;
    in.red = 255; in.green = 0; in.blue = 17; in.rumble_left = 3; in.rumble_right = 250;
    const std::string line = format_output_line(in);
    EXPECT_EQ(line, "OUTPUT 255 0 17 3 250\n");
    OutputState out;
    ASSERT_TRUE(parse_output(line, &out));
    EXPECT_EQ(out.red, 255);
    EXPECT_EQ(out.blue, 17);
    EXPECT_EQ(out.rumble_right, 250);
}

TEST(OutputLine, RejectsMalformed) {
    OutputState out;
    EXPECT_FALSE(parse_output("OUTPUT 1 2 3 4", &out));
    EXPECT_FALSE(parse_output("OUTPUT a b c d e", &out));
    EXPECT_FALSE(parse_output("NOTE VIRTUAL x", &out));
    EXPECT_FALSE(parse_output("OUTPUT 1 2 3 4 5", nullptr));
}

// -------------------------------------------------------------------- NOTE

TEST(NoteLine, WithSource) {
    std::string source, text;
    ASSERT_TRUE(parse_note("NOTE PHYSICAL no controller attached", &source, &text));
    EXPECT_EQ(source, "PHYSICAL");
    EXPECT_EQ(text, "no controller attached");
}

TEST(NoteLine, LegacyLineWithoutSource) {
    std::string source = "stale", text;
    ASSERT_TRUE(parse_note("NOTE emulation is off", &source, &text));
    EXPECT_EQ(source, "");
    EXPECT_EQ(text, "emulation is off");
}

TEST(NoteLine, SourceOnlyAndNullOutputs) {
    std::string source, text;
    ASSERT_TRUE(parse_note("NOTE VIRTUAL", &source, &text));
    EXPECT_EQ(source, "VIRTUAL");
    EXPECT_TRUE(parse_note("NOTE x", nullptr, nullptr));
    EXPECT_FALSE(parse_note("STATE x", &source, &text));
}

// ------------------------------------------------------------------ status

TEST(Status, ParsesKeyValueLinesInOrder) {
    const auto fields = parse_status("Emulated type: DualShock 4\nBackend:   uhid  \r\nno separator\nEmpty:\n");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[0].key, "Emulated type");
    EXPECT_EQ(fields[0].value, "DualShock 4");
    EXPECT_EQ(fields[1].key, "Backend");
    EXPECT_EQ(fields[1].value, "uhid");
    EXPECT_EQ(fields[2].key, "Empty");
    EXPECT_EQ(fields[2].value, "");
}

TEST(Status, ValueContainingColonKeepsRemainder) {
    const auto fields = parse_status("MAC: 00:11:22:33:44:55\n");
    ASSERT_EQ(fields.size(), 1u);
    EXPECT_EQ(fields[0].value, "00:11:22:33:44:55");
}

TEST(Status, LookupByKey) {
    const auto fields = parse_status("A: 1\nB: 2\n");
    EXPECT_EQ(status_value(fields, "B"), "2");
    EXPECT_EQ(status_value(fields, "missing"), "");
    EXPECT_TRUE(parse_status("").empty());
}

TEST(TypeNames, DisplayAndConfigRoundTrip) {
    for (const auto &config : type_values()) {
        if (config == "none") continue; // "None" has no distinct display->config source
        EXPECT_EQ(type_display_to_config(type_config_to_display(config)), config) << config;
    }
    EXPECT_EQ(type_config_to_display("garbage"), "None");
    EXPECT_EQ(type_display_to_config("garbage"), "none");
}

TEST(TrimResponse, RemovesOnlyTrailingNewlines) {
    std::string s = "ok\n\n";
    trim_response(&s);
    EXPECT_EQ(s, "ok");
    s = "a\nb\n";
    trim_response(&s);
    EXPECT_EQ(s, "a\nb");
    s = "";
    trim_response(&s);
    EXPECT_EQ(s, "");
}
