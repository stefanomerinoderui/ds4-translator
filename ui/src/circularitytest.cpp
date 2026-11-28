#include "circularitytest.h"

namespace {
uint8_t axis(int value) { return static_cast<uint8_t>(qBound(0, value, 255)); }
} // namespace

void CircularityTest::add(int lx, int ly, int rx, int ry) {
    m_left.add(axis(lx), axis(ly));
    m_right.add(axis(rx), axis(ry));
    emit changed();
}

void CircularityTest::reset() {
    m_left.reset();
    m_right.reset();
    emit changed();
}

QList<qreal> CircularityTest::shapeOf(const StickCircularity &stick) {
    QList<qreal> shape;
    shape.reserve(static_cast<qsizetype>(StickCircularity::kBuckets));
    for (double radius : stick.max_radius()) {
        shape.append(radius);
    }
    return shape;
}
