#ifndef DS4_UI_CIRCULARITYTEST_H
#define DS4_UI_CIRCULARITYTEST_H

#include <QList>
#include <QObject>
#include <QQmlEngine>

#include "stick-coverage.h"

// The circularity test of the calibration page: feed it live stick values
// while the user rotates both sticks round their edge, and it reports how
// round each stick's range is (StickCircularity) plus the shape reached, for
// the dials to draw.
class CircularityTest : public QObject {
    Q_OBJECT
    QML_ELEMENT

    // RMS distance of the reached edge from a perfect circle, in percent.
    Q_PROPERTY(double leftError READ leftError NOTIFY changed)
    Q_PROPERTY(double rightError READ rightError NOTIFY changed)
    // Fraction of the edge reached so far, 0..1: the error only means much
    // once this is near 1.
    Q_PROPERTY(double leftCoverage READ leftCoverage NOTIFY changed)
    Q_PROPERTY(double rightCoverage READ rightCoverage NOTIFY changed)
    // Furthest radius reached per angle (0 = not yet), angle 0 pointing right.
    Q_PROPERTY(QList<qreal> leftShape READ leftShape NOTIFY changed)
    Q_PROPERTY(QList<qreal> rightShape READ rightShape NOTIFY changed)

public:
    explicit CircularityTest(QObject *parent = nullptr) : QObject(parent) {}

    double leftError() const { return m_left.error_percent(); }
    double rightError() const { return m_right.error_percent(); }
    double leftCoverage() const { return m_left.coverage(); }
    double rightCoverage() const { return m_right.coverage(); }
    QList<qreal> leftShape() const { return shapeOf(m_left); }
    QList<qreal> rightShape() const { return shapeOf(m_right); }

    // Raw stick values, 0..255 with 128 centred.
    Q_INVOKABLE void add(int lx, int ly, int rx, int ry);
    Q_INVOKABLE void reset();

signals:
    void changed();

private:
    static QList<qreal> shapeOf(const StickCircularity &stick);

    StickCircularity m_left;
    StickCircularity m_right;
};

#endif // DS4_UI_CIRCULARITYTEST_H
