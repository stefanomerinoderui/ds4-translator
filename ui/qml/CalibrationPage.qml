// The target cards reach the file's root id; Bound makes that explicit.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ds4Translator

// Stick and trigger calibration of the physical controller, one wizard per
// target (CalibrationWizard). USB only: the daemon refuses it over Bluetooth.
ScrollView {
    id: root

    readonly property bool hasPad: DaemonController.online
        && DaemonController.physicalController.length > 0
    readonly property bool onUsb: root.hasPad && DaemonController.connectionType === "USB"
    readonly property bool ready: root.onUsb
        && CalibrationController.phase === CalibrationController.Idle

    contentWidth: availableWidth

    // The error says little until most of the edge has been reached.
    function circularityCaption(error, coverage) {
        if (coverage === 0) {
            return qsTr("Rotate the stick");
        }
        if (coverage < 0.9) {
            return qsTr("%1% of the edge reached").arg(Math.round(coverage * 100));
        }
        return qsTr("Circularity error: %1%").arg(error.toFixed(1));
    }

    // Stopped, the outline stays as it was, so it can be read while the
    // sticks are let go.
    property bool circularityRunning: true

    CircularityTest {
        id: circularity
    }

    // Only while the page shows the physical pad, and not during a wizard
    // (stick range would otherwise feed the test with calibration moves).
    Connections {
        target: InputMonitor
        enabled: root.visible && root.circularityRunning
            && CalibrationController.phase === CalibrationController.Idle
        function onInputChanged() {
            if (InputMonitor.hasState && InputMonitor.source === InputMonitor.Physical) {
                circularity.add(InputMonitor.leftX, InputMonitor.leftY,
                                InputMonitor.rightX, InputMonitor.rightY);
            }
        }
    }

    component TargetCard: GroupBox {
        id: card

        property string target: ""
        property string description: ""

        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 12

        RowLayout {
            anchors.fill: parent
            spacing: 12

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: card.description
            }
            Button {
                text: qsTr("Calibrate…")
                enabled: root.ready
                onClicked: CalibrationController.begin(card.target)
            }
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.bottomMargin: 0
            spacing: 12

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: root.hasPad && !root.onUsb ? "#c0392b" : palette.text
                text: !DaemonController.online ? qsTr("The daemon is not reachable.")
                    : !root.hasPad ? qsTr("No controller connected. Plug it in with a USB cable.")
                    : !root.onUsb ? qsTr("%1 is connected over %2. Calibration needs the controller on USB: "
                        + "connect it by cable (and unpair the Bluetooth link if it stays on Bluetooth).")
                        .arg(DaemonController.physicalController).arg(DaemonController.connectionType)
                    : qsTr("%1, connected over USB.").arg(DaemonController.physicalController)
            }

            // Undoes any calibration not stored permanently and ends one left
            // open (a cancelled run blocks the next start until a reset).
            Button {
                text: qsTr("Reset controller")
                enabled: root.onUsb && CalibrationController.phase === CalibrationController.Idle
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Reboots the controller: it goes back to its permanent calibration")
                onClicked: CalibrationController.resetController()
            }
        }

        // Unsaved results, as dualshock-tools shows them: the pad uses them
        // now but drops them on its next reset.
        Pane {
            visible: CalibrationController.unsavedChanges
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            background: Rectangle {
                color: "#33f39c12"
                border.color: "#f39c12"
                radius: 4
            }

            RowLayout {
                anchors.fill: parent
                spacing: 12
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: qsTr("The controller has a calibration that is not stored permanently. It is "
                        + "lost when the controller resets or goes into standby. To keep it, calibrate "
                        + "again and choose \"Store permanently\"; to undo it, reset the controller.")
                }
            }
        }

        TargetCard {
            title: qsTr("Stick centre")
            target: "stick-center"
            description: qsTr("Sets where each stick rests. Use it when a released stick does not read as "
                + "centred.")
        }

        TargetCard {
            title: qsTr("Stick range")
            target: "stick-range"
            description: qsTr("Sets how far each stick reaches. You rotate both sticks round their edge. "
                + "Use it when a stick cannot reach full deflection in some direction, or reaches it too "
                + "early.")
        }

        TargetCard {
            title: qsTr("Triggers")
            target: "triggers"
            description: qsTr("Sets the released, halfway and fully pressed points of L2 and R2. Marked "
                + "beta by the tools this procedure comes from.")
        }

        GroupBox {
            title: qsTr("Current sticks and circularity test")
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.75
                    text: qsTr("Rotate each stick slowly round its edge, pushed all the way, a few times. "
                        + "The orange outline is how far it reached at each angle, and the error is how far "
                        + "that is from a perfect circle: lower is rounder. Too high means the stick falls "
                        + "short somewhere or reaches too far on the diagonals.")
                }

                StickDials {
                    Layout.fillWidth: true
                    leftShape: circularity.leftShape
                    rightShape: circularity.rightShape
                    leftCaption: root.circularityCaption(circularity.leftError, circularity.leftCoverage)
                    rightCaption: root.circularityCaption(circularity.rightError, circularity.rightCoverage)
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8

                    Button {
                        text: root.circularityRunning ? qsTr("Stop test") : qsTr("Resume test")
                        onClicked: root.circularityRunning = !root.circularityRunning
                    }
                    Button {
                        text: qsTr("Reset test")
                        onClicked: circularity.reset()
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 12
            wrapMode: Text.Wrap
            opacity: 0.75
            text: qsTr("Results are kept only until the controller resets, unless you choose otherwise "
                + "at the end of a calibration. Calibration cannot fix drift from worn parts. "
                + "Fine-tuning single values is not possible on a DualShock 4: its firmware does not "
                + "accept them.")
        }

        Label {
            visible: CalibrationController.phase === CalibrationController.Idle
                && CalibrationController.message.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 12
            wrapMode: Text.Wrap
            text: CalibrationController.message
        }
    }

    CalibrationWizard {
        parent: Overlay.overlay
    }
}
