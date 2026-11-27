pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ds4Translator

// Both sticks of the physical controller as two round dials: the outer edge
// is full deflection and the dot is where the stick is now. With a shape
// (CircularityTest) the edge each stick has reached is drawn too.
RowLayout {
    id: root

    // Ring drawn at this fraction of full deflection: where a sample starts
    // counting towards a circle during range calibration.
    property real guideRadius: 0
    // Text under each dial, e.g. circles done.
    property string leftCaption: ""
    property string rightCaption: ""
    // Furthest radius reached per angle (CircularityTest), drawn as the
    // outline the stick has traced; empty draws nothing.
    property var leftShape: []
    property var rightShape: []

    readonly property color accent: "#2d8cf0"
    // Outline, crosshair and guide ring. A Canvas is not a Control and has no
    // palette, so this is a fixed grey that reads on light and dark themes.
    readonly property color frame: "#8a8f98"

    spacing: 24

    component Dial: ColumnLayout {
        id: dial

        property string title: ""
        property string caption: ""
        property int axisX: 128
        property int axisY: 128
        property var shape: []

        // -1..1 each way, 128 centred (as the input stream reports them).
        readonly property real nx: dial.axisX >= 128 ? (dial.axisX - 128) / 127 : (dial.axisX - 128) / 128
        readonly property real ny: dial.axisY >= 128 ? (dial.axisY - 128) / 127 : (dial.axisY - 128) / 128

        spacing: 4
        onNxChanged: canvas.requestPaint()
        onNyChanged: canvas.requestPaint()
        onShapeChanged: canvas.requestPaint()

        Label {
            text: dial.title
            Layout.alignment: Qt.AlignHCenter
        }

        Canvas {
            id: canvas

            readonly property real radius: Math.min(width, height) / 2 - 6

            Layout.preferredWidth: 150
            Layout.preferredHeight: 150
            Layout.alignment: Qt.AlignHCenter

            onPaint: {
                const ctx = getContext("2d");
                const cx = width / 2;
                const cy = height / 2;
                ctx.reset();

                ctx.strokeStyle = root.frame;
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(cx - radius, cy);
                ctx.lineTo(cx + radius, cy);
                ctx.moveTo(cx, cy - radius);
                ctx.lineTo(cx, cy + radius);
                ctx.stroke();

                if (root.guideRadius > 0) {
                    ctx.setLineDash([3, 3]);
                    ctx.beginPath();
                    ctx.arc(cx, cy, radius * root.guideRadius, 0, 2 * Math.PI);
                    ctx.stroke();
                    ctx.setLineDash([]);
                }

                // Reached edge: one point per angle bucket, joined round,
                // skipping the angles not reached yet.
                const shape = dial.shape;
                if (shape && shape.length > 0) {
                    ctx.strokeStyle = "#f39c12";
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    let drawing = false;
                    for (let i = 0; i <= shape.length; ++i) {
                        const r = shape[i % shape.length];
                        if (r <= 0) {
                            drawing = false;
                            continue;
                        }
                        const a = (i + 0.5) * 2 * Math.PI / shape.length;
                        const px = cx + Math.cos(a) * r * radius;
                        const py = cy + Math.sin(a) * r * radius;
                        if (drawing) {
                            ctx.lineTo(px, py);
                        } else {
                            ctx.moveTo(px, py);
                            drawing = true;
                        }
                    }
                    ctx.stroke();
                    ctx.lineWidth = 1;
                }

                ctx.fillStyle = root.accent;
                ctx.beginPath();
                ctx.arc(cx + dial.nx * radius, cy + dial.ny * radius, 5, 0, 2 * Math.PI);
                ctx.fill();
            }
        }

        Label {
            text: dial.caption
            visible: text.length > 0
            Layout.alignment: Qt.AlignHCenter
            opacity: 0.75
        }
    }

    Item { Layout.fillWidth: true }

    Dial {
        id: leftDial
        title: qsTr("Left stick")
        caption: root.leftCaption
        shape: root.leftShape
        axisX: InputMonitor.hasState ? InputMonitor.leftX : 128
        axisY: InputMonitor.hasState ? InputMonitor.leftY : 128
    }

    Dial {
        id: rightDial
        title: qsTr("Right stick")
        caption: root.rightCaption
        shape: root.rightShape
        axisX: InputMonitor.hasState ? InputMonitor.rightX : 128
        axisY: InputMonitor.hasState ? InputMonitor.rightY : 128
    }

    Item { Layout.fillWidth: true }
}
