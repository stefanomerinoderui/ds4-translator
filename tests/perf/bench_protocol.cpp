// Performance checks for the per-report hot paths (STATE formatting/parsing,
// CRC32, Xbox report building). These run once per controller report -- up to
// ~1000 times a second -- so they must stay far below the report interval.
//
// Budgets are deliberately generous (~100x typical) so a noisy CI runner does
// not flake, while a real algorithmic regression still trips them. Measured
// numbers go to stdout (and the CI job summary) either way.
#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <string>

#include "ipc-client.h"
#include "translate.h"

namespace {

template <typename Fn>
double ns_per_op(int iterations, Fn &&fn) {
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) fn(i);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()) /
           iterations;
}

void report(const char *name, double ns, double budget_ns) {
    std::printf("PERF %-28s %10.1f ns/op (budget %.0f)\n", name, ns, budget_ns);
}

} // namespace

TEST(Perf, StateLineFormatAndParse) {
    ds4ipc::InputState in;
    in.has_motion = in.has_touch = true;
    ds4ipc::InputState out;
    volatile size_t sink = 0;
    const double ns = ns_per_op(100000, [&](int i) {
        in.lx = static_cast<uint8_t>(i);
        const std::string line = ds4ipc::format_state_line(ds4ipc::kSourceVirtual, in);
        sink = sink + (ds4ipc::parse_state(line, &out) ? line.size() : 0);
    });
    report("format+parse STATE", ns, 50000);
    EXPECT_LT(ns, 50000);
}

TEST(Perf, Crc32OfBluetoothReport) {
    uint8_t buf[74] = {};
    volatile uint32_t sink = 0;
    const double ns = ns_per_op(200000, [&](int i) {
        buf[0] = static_cast<uint8_t>(i);
        sink = sink + calculate_crc32(0xA2, buf, sizeof(buf));
    });
    report("crc32 74-byte report", ns, 20000);
    EXPECT_LT(ns, 20000);
}

TEST(Perf, XboxReportBuilders) {
    dualshock4_input_report_common c = {};
    uint8_t x360[XBOX360_REPORT_SIZE];
    uint8_t gip[FFS_PACKET_MAX];
    volatile size_t sink = 0;
    const double ns = ns_per_op(200000, [&](int i) {
        c.x = static_cast<uint8_t>(i);
        build_xbox360_report(c, x360);
        sink = sink + build_gip_input_report(c, true, static_cast<uint8_t>(i), gip);
    });
    report("xbox360 + GIP series build", ns, 20000);
    EXPECT_LT(ns, 20000);
}
