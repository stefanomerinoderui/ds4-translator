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

        Label {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.bottomMargin: 0
            wrapMode: Text.Wrap
            color: root.hasPad && !root.onUsb ? "#c0392b" : palette.text
            text: !DaemonController.online ? qsTr("The daemon is not reachable.")
                : !root.hasPad ? qsTr("No controller connected. Plug it in with a USB cable.")
                : !root.onUsb ? qsTr("%1 is connected over %2. Calibration needs the controller on USB: "
                    + "connect it by cable (and unpair the Bluetooth link if it stays on Bluetooth).")
                    .arg(DaemonController.physicalController).arg(DaemonController.connectionType)
                : qsTr("%1, connected over USB.").arg(DaemonController.physicalController)
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
                        + "again and choose \"Store permanently\".")
                }
                Button {
                    text: qsTr("Revert")
                    enabled: root.onUsb
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Reset the controller, going back to its permanent calibration")
                    onClicked: CalibrationController.resetController()
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
            title: qsTr("Current sticks")
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12

            StickDials {
                anchors.fill: parent
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
