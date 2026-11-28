#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include "stick-coverage.h"

namespace {

uint8_t axis(double v) { return static_cast<uint8_t>(std::lround(128.0 + v * 127.0)); }

// One full turn of the left stick at `radius`, sampled every degree; the
// right stick stays centred unless `both` is set.
void turn(StickCoverage &c, double radius, bool both = false) {
    for (int deg = 0; deg < 360; ++deg) {
        const double a = deg * M_PI / 180.0;
        const uint8_t x = axis(radius * std::cos(a));
        const uint8_t y = axis(radius * std::sin(a));
        c.add(x, y, both ? x : 128, both ? y : 128);
    }
}

} // namespace

TEST(StickCoverage, StartsEmpty) {
    StickCoverage c;
    EXPECT_EQ(c.circles(StickCoverage::Left), 0);
    EXPECT_DOUBLE_EQ(c.progress(), 0.0);
    EXPECT_FALSE(c.complete());
}

TEST(StickCoverage, FullDeflectionTurnCountsOneCircle) {
    StickCoverage c;
    turn(c, 1.0);
    EXPECT_EQ(c.circles(StickCoverage::Left), 1);
    EXPECT_EQ(c.circles(StickCoverage::Right), 0);
    EXPECT_NEAR(c.progress(StickCoverage::Left), 0.25, 0.03);
}

TEST(StickCoverage, SmallCirclesNeverCount) {
    StickCoverage c;
    for (int i = 0; i < 10; ++i) turn(c, 0.5, true);
    EXPECT_EQ(c.circles(StickCoverage::Left), 0);
    EXPECT_DOUBLE_EQ(c.progress(), 0.0);
}

TEST(StickCoverage, HalfATurnIsPartialProgress) {
    StickCoverage c;
    for (int deg = 0; deg < 180; ++deg) {
        const double a = deg * M_PI / 180.0;
        c.add(axis(std::cos(a)), axis(std::sin(a)), 128, 128);
    }
    EXPECT_EQ(c.circles(StickCoverage::Left), 0);
    EXPECT_GT(c.progress(StickCoverage::Left), 0.1);
    EXPECT_LT(c.progress(StickCoverage::Left), 0.25);
}

TEST(StickCoverage, BothSticksNeedTheTarget) {
    StickCoverage c;
    for (int i = 0; i < StickCoverage::kTargetCircles; ++i) turn(c, 1.0);
    EXPECT_FALSE(c.complete()); // right stick never moved
    EXPECT_DOUBLE_EQ(c.progress(), 0.0);
    for (int i = 0; i < StickCoverage::kTargetCircles; ++i) turn(c, 1.0, true);
    EXPECT_TRUE(c.complete());
    EXPECT_DOUBLE_EQ(c.progress(), 1.0);
}

TEST(StickCoverage, ResetForgetsEverything) {
    StickCoverage c;
    turn(c, 1.0, true);
    c.reset();
    EXPECT_EQ(c.circles(StickCoverage::Left), 0);
    EXPECT_DOUBLE_EQ(c.progress(), 0.0);
}

namespace {

// One turn at `radius`, or a square path (|x| or |y| at 1) with `square`.
void circle_into(StickCircularity &c, double radius, bool square = false) {
    for (int deg = 0; deg < 360; ++deg) {
        const double a = deg * M_PI / 180.0;
        double x = radius * std::cos(a);
        double y = radius * std::sin(a);
        if (square) {
            const double scale = 1.0 / std::max(std::fabs(x), std::fabs(y));
            x *= scale;
            y *= scale;
        }
        c.add(axis(std::clamp(x, -1.0, 1.0)), axis(std::clamp(y, -1.0, 1.0)));
    }
}

} // namespace

TEST(StickCircularity, PerfectCircleScoresNearZero) {
    StickCircularity c;
    circle_into(c, 1.0);
    EXPECT_DOUBLE_EQ(c.coverage(), 1.0);
    EXPECT_LT(c.error_percent(), 1.5);
}

TEST(StickCircularity, ShortRangeScoresItsShortfall) {
    StickCircularity c;
    circle_into(c, 0.9);
    EXPECT_NEAR(c.error_percent(), 10.0, 1.5);
}

TEST(StickCircularity, SquareRangeScoresHigh) {
    StickCircularity c;
    circle_into(c, 1.0, true);
    EXPECT_GT(c.error_percent(), 15.0);
}

TEST(StickCircularity, CentreSamplesAreIgnoredAndBucketZeroPointsRight) {
    StickCircularity c;
    c.add(128, 128);
    EXPECT_DOUBLE_EQ(c.coverage(), 0.0);
    EXPECT_DOUBLE_EQ(c.error_percent(), 0.0);
    c.add(255, 128); // full right
    EXPECT_NEAR(c.max_radius()[0], 1.0, 1e-9);
    c.reset();
    EXPECT_DOUBLE_EQ(c.coverage(), 0.0);
}
