pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ds4Translator

// Both sticks of the physical controller as two round dials: the outer edge
// is full deflection, the dot is where the stick is now, and (with `trace`)
// every position it has passed through stays drawn. Rotating a stick round
// its edge then shows how round its range really is -- the same view the
// GUI calibrators give while calibrating the range.
RowLayout {
    id: root

    // Keep the path the sticks have drawn (stick range).
    property bool trace: false
    // Ring drawn at this fraction of full deflection: where a sample starts
    // counting towards a circle during range calibration.
    property real guideRadius: 0
    // Text under each dial, e.g. circles done.
    property string leftCaption: ""
    property string rightCaption: ""

    readonly property color accent: "#2d8cf0"
    // Outline, crosshair and guide ring. A Canvas is not a Control and has no
    // palette, so this is a fixed grey that reads on light and dark themes.
    readonly property color frame: "#8a8f98"

    function clearTrace() {
        leftDial.clear();
        rightDial.clear();
    }

    spacing: 24

    component Dial: ColumnLayout {
        id: dial

        property string title: ""
        property string caption: ""
        property int axisX: 128
        property int axisY: 128

        // -1..1 each way, 128 centred (as the input stream reports them).
        readonly property real nx: dial.axisX >= 128 ? (dial.axisX - 128) / 127 : (dial.axisX - 128) / 128
        readonly property real ny: dial.axisY >= 128 ? (dial.axisY - 128) / 127 : (dial.axisY - 128) / 128

        function clear() {
            canvas.points = [];
            canvas.requestPaint();
        }

        spacing: 4
        onNxChanged: canvas.addPoint()
        onNyChanged: canvas.addPoint()

        Label {
            text: dial.title
            Layout.alignment: Qt.AlignHCenter
        }

        Canvas {
            id: canvas

            property var points: []
            readonly property real radius: Math.min(width, height) / 2 - 6

            function addPoint() {
                if (root.trace) {
                    points.push([dial.nx, dial.ny]);
                }
                requestPaint();
            }

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

                if (points.length > 1) {
                    ctx.strokeStyle = root.accent;
                    ctx.globalAlpha = 0.5;
                    ctx.beginPath();
                    ctx.moveTo(cx + points[0][0] * radius, cy + points[0][1] * radius);
                    for (let i = 1; i < points.length; ++i) {
                        ctx.lineTo(cx + points[i][0] * radius, cy + points[i][1] * radius);
                    }
                    ctx.stroke();
                    ctx.globalAlpha = 1;
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
        axisX: InputMonitor.hasState ? InputMonitor.leftX : 128
        axisY: InputMonitor.hasState ? InputMonitor.leftY : 128
    }

    Dial {
        id: rightDial
        title: qsTr("Right stick")
        caption: root.rightCaption
        axisX: InputMonitor.hasState ? InputMonitor.rightX : 128
        axisY: InputMonitor.hasState ? InputMonitor.rightY : 128
    }

    Item { Layout.fillWidth: true }
}
