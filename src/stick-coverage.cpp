#include "stick-coverage.h"

#include <algorithm>
#include <cmath>

namespace {
// -1..1, with 128 as the centre. 0 and 255 map to just past -1 and to 1.
double normalise(uint8_t value) { return (static_cast<double>(value) - 128.0) / 127.0; }
} // namespace

void StickCoverage::add_to(State &state, uint8_t x, uint8_t y) {
    const double nx = normalise(x);
    const double ny = normalise(y);
    if (std::hypot(nx, ny) < kMinRadius) return;

    const double turn = (std::atan2(ny, nx) + M_PI) / (2.0 * M_PI); // 0..1
    const size_t bucket = std::min(static_cast<size_t>(turn * kBuckets), kBuckets - 1);
    if (state.filled[bucket]) return;
    state.filled[bucket] = true;
    if (++state.filled_count >= kBucketsForCircle) {
        ++state.circles;
        state.filled.fill(false);
        state.filled_count = 0;
    }
}

void StickCoverage::add(uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry) {
    add_to(m_sticks[Left], lx, ly);
    add_to(m_sticks[Right], rx, ry);
}

void StickCoverage::reset() { m_sticks = {}; }

double StickCoverage::progress(Stick stick) const {
    const State &state = m_sticks[stick];
    const double done = state.circles + static_cast<double>(state.filled_count) / kBucketsForCircle;
    return std::min(1.0, done / kTargetCircles);
}

double StickCoverage::progress() const { return std::min(progress(Left), progress(Right)); }
