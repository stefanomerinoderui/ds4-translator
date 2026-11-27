#include <gtest/gtest.h>

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
