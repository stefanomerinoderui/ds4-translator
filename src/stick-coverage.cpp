#include "stick-coverage.h"

#include <algorithm>
#include <cmath>

namespace {
// -1..1, with 128 as the centre: 0 maps to -1 and 255 to 1, the same scale
// the UI's dials draw with.
double normalise(uint8_t value) {
    const double offset = static_cast<double>(value) - 128.0;
    return offset >= 0 ? offset / 127.0 : offset / 128.0;
}

// 0..1 round the circle, from angle 0 (pointing right).
double turn_of(double x, double y) { return (std::atan2(y, x) + M_PI) / (2.0 * M_PI); }
} // namespace

void StickCoverage::add_to(State &state, uint8_t x, uint8_t y) {
    const double nx = normalise(x);
    const double ny = normalise(y);
    if (std::hypot(nx, ny) < kMinRadius) return;

    const size_t bucket = std::min(static_cast<size_t>(turn_of(nx, ny) * kBuckets), kBuckets - 1);
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

void StickCircularity::add(uint8_t x, uint8_t y) {
    const double nx = normalise(x);
    const double ny = normalise(y);
    const double radius = std::hypot(nx, ny);
    if (radius < kMinRadius) return;
    // atan2 + pi puts angle 0 at bucket kBuckets/2; shift so bucket 0 points right.
    const double turn = std::fmod(turn_of(nx, ny) + 0.5, 1.0);
    const size_t bucket = std::min(static_cast<size_t>(turn * kBuckets), kBuckets - 1);
    m_max[bucket] = std::max(m_max[bucket], radius);
}

void StickCircularity::reset() { m_max = {}; }

double StickCircularity::coverage() const {
    size_t reached = 0;
    for (double r : m_max) {
        if (r > 0) ++reached;
    }
    return static_cast<double>(reached) / kBuckets;
}

double StickCircularity::error_percent() const {
    double sum = 0;
    size_t reached = 0;
    for (double r : m_max) {
        if (r > 0) {
            sum += (r - 1.0) * (r - 1.0);
            ++reached;
        }
    }
    return reached == 0 ? 0.0 : 100.0 * std::sqrt(sum / static_cast<double>(reached));
}
