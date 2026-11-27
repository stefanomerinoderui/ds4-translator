#ifndef DS4_STICK_COVERAGE_H
#define DS4_STICK_COVERAGE_H

// How far through a stick-range calibration the user is: how many full
// circles each stick has been rotated through at (nearly) full deflection.
//
// Same rule as dualshock-tools' range wizard: the circle is split into
// angle buckets, a sample at radius >= kMinRadius fills its bucket, and a
// circle counts once kBucketsForCircle of the buckets are filled; the
// buckets then empty for the next one. The wizard waits for kTargetCircles
// per stick before it lets the user store the result.
//
// Pure arithmetic on raw stick values (0..255, 128 centred), so the UI can
// feed it whatever its input stream reports and tests can feed it circles.

#include <array>
#include <cstddef>
#include <cstdint>

class StickCoverage {
public:
    static constexpr size_t kBuckets = 36;
    static constexpr size_t kBucketsForCircle = 35; // ~95% of kBuckets
    static constexpr double kMinRadius = 0.8;
    static constexpr int kTargetCircles = 4;

    enum Stick { Left = 0, Right = 1 };

    // One input sample for both sticks.
    void add(uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry);
    void reset();

    // Completed circles of one stick (not capped at kTargetCircles).
    int circles(Stick stick) const { return m_sticks[stick].circles; }
    // 0..1 for one stick: completed circles plus the current one's filled
    // buckets, over kTargetCircles.
    double progress(Stick stick) const;
    // 0..1 for both: the slower stick decides, as both have to be done.
    double progress() const;
    bool complete() const { return circles(Left) >= kTargetCircles && circles(Right) >= kTargetCircles; }

private:
    struct State {
        std::array<bool, kBuckets> filled{};
        size_t filled_count = 0;
        int circles = 0;
    };
    static void add_to(State &state, uint8_t x, uint8_t y);

    std::array<State, 2> m_sticks{};
};

#endif // DS4_STICK_COVERAGE_H
