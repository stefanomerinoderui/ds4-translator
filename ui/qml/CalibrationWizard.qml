import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ds4Translator

// One calibration run, page by page: CalibrationController holds the state
// and talks to the daemon, this only shows the current page.
Dialog {
    id: root

    readonly property int phase: CalibrationController.phase
    readonly property bool sticks: CalibrationController.target !== "triggers"
    readonly property string targetLabel: {
        const t = CalibrationController.target;
        return t === "stick-center" ? qsTr("Stick centre")
            : t === "stick-range" ? qsTr("Stick range")
            : qsTr("Triggers");
    }

    title: qsTr("Calibrate: %1").arg(root.targetLabel)
    modal: true
    closePolicy: Popup.NoAutoClose
    anchors.centerIn: Overlay.overlay
    // Wide enough for the footer's buttons (the store page has three), but
    // never wider than the window.
    width: Math.min(Math.max(560, footerRow.implicitWidth + root.leftPadding + root.rightPadding + 24),
                    (parent ? parent.width : 560) - 24)
    // Opened by CalibrationController.begin(), closed once the controller goes
    // back to Idle. Not a binding on `visible`: closing the dialog would
    // break it.
    onPhaseChanged: {
        if (root.phase === CalibrationController.Idle) {
            root.close();
        } else if (!root.visible) {
            root.open();
        }
    }

    Connections {
        target: InputMonitor
        enabled: root.phase === CalibrationController.Rotating
        function onInputChanged() {
            CalibrationController.addStickSample(InputMonitor.leftX, InputMonitor.leftY,
                                                 InputMonitor.rightX, InputMonitor.rightY);
        }
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            visible: CalibrationController.stepCount > 0
                && root.phase !== CalibrationController.Confirm
                && root.phase !== CalibrationController.Checking
                && root.phase !== CalibrationController.Failed
            text: qsTr("Step %1 of %2").arg(Math.max(1, CalibrationController.stepIndex))
                .arg(CalibrationController.stepCount)
            opacity: 0.75
        }

        // Confirm: the warning, before anything is written.
        Label {
            visible: root.phase === CalibrationController.Confirm
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("This writes to the controller's calibration memory. If it is interrupted, or "
                + "the controller does not respond as expected, the controller can be left unusable.\n\n"
                + "Calibration cannot fix stick drift caused by worn parts, and it can make a worn "
                + "stick behave worse.\n\n"
                + "Keep the cable plugged in until the last page. At the end you choose whether to "
                + "keep the result only until the controller resets, or permanently.")
        }

        Label {
            visible: root.phase === CalibrationController.Checking
            text: qsTr("Checking the controller's connection…")
        }

        Label {
            visible: root.phase === CalibrationController.Instruct
                || root.phase === CalibrationController.Rotating
                || root.phase === CalibrationController.ChooseStore
                || root.phase === CalibrationController.Working
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.weight: Font.DemiBold
            text: root.phase === CalibrationController.Working && CalibrationController.instruction.length === 0
                ? qsTr("Working…") : CalibrationController.instruction
        }

        StickDials {
            id: dials
            Layout.fillWidth: true
            visible: root.sticks && root.phase !== CalibrationController.Confirm
                && root.phase !== CalibrationController.Checking
                && root.phase !== CalibrationController.Failed
            guideRadius: root.phase === CalibrationController.Rotating ? 0.8 : 0
            leftCaption: root.phase === CalibrationController.Rotating
                ? qsTr("%1 / %2 circles").arg(CalibrationController.leftCircles)
                    .arg(CalibrationController.targetCircles) : ""
            rightCaption: root.phase === CalibrationController.Rotating
                ? qsTr("%1 / %2 circles").arg(CalibrationController.rightCircles)
                    .arg(CalibrationController.targetCircles) : ""
        }

        // Triggers: what the pad reads right now, so "halfway" can be judged.
        GridLayout {
            visible: !root.sticks && root.phase === CalibrationController.Instruct
            columns: 2
            Layout.fillWidth: true
            Label { text: qsTr("L2") }
            ProgressBar { Layout.fillWidth: true; from: 0; to: 255; value: InputMonitor.hasState ? InputMonitor.l2 : 0 }
            Label { text: qsTr("R2") }
            ProgressBar { Layout.fillWidth: true; from: 0; to: 255; value: InputMonitor.hasState ? InputMonitor.r2 : 0 }
        }

        ProgressBar {
            visible: root.phase === CalibrationController.Rotating
            Layout.fillWidth: true
            value: CalibrationController.rangeProgress
        }
        Label {
            visible: root.phase === CalibrationController.Rotating && CalibrationController.keepRotatingHint
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#c0392b"
            text: qsTr("Keep rotating: push each stick all the way out and go round its edge.")
        }

        BusyIndicator {
            visible: running
            running: root.phase === CalibrationController.Working
                || root.phase === CalibrationController.Checking
            Layout.alignment: Qt.AlignHCenter
        }

        // Failed: the daemon's own words.
        Label {
            visible: root.phase === CalibrationController.Failed
                && CalibrationController.message.length > 0
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#c0392b"
            text: CalibrationController.message
        }
        Label {
            visible: root.phase === CalibrationController.Failed && CalibrationController.startedOnPad
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("The calibration was started on the controller but not stored. Closing resets the "
                + "controller, which ends it; otherwise it blocks the next calibration.")
        }
    }

    // A plain row rather than a DialogButtonBox: the box sized itself
    // without the buttons that appear later (the store page's), so they ran
    // past the dialog's edge. A row's implicit width always includes them.
    footer: Pane {
        RowLayout {
            id: footerRow
            anchors.fill: parent
            spacing: 8

            Button {
                // A calibration started on the pad and not stored stays open
                // on it (the pad has a battery, so unplugging does not clear
                // it) and blocks the next one; closing then resets the pad.
                text: {
                    const finished = root.phase === CalibrationController.Failed;
                    if (CalibrationController.startedOnPad) {
                        return finished ? qsTr("Close and reset controller") : qsTr("Cancel and reset controller");
                    }
                    return finished ? qsTr("Close") : qsTr("Cancel");
                }
                enabled: root.phase !== CalibrationController.Working
                ToolTip.visible: hovered && CalibrationController.startedOnPad
                ToolTip.text: qsTr("Also drops any calibration not stored permanently")
                onClicked: root.reject()
            }

            Item { Layout.fillWidth: true }

            Button {
                visible: root.phase === CalibrationController.Confirm
                    || root.phase === CalibrationController.Instruct
                    || root.phase === CalibrationController.Rotating
                text: root.phase === CalibrationController.Confirm ? qsTr("Start") : qsTr("Continue")
                enabled: CalibrationController.canContinue
                highlighted: true
                onClicked: CalibrationController.next()
            }
            Button {
                visible: root.phase === CalibrationController.ChooseStore
                text: qsTr("Store until reset")
                highlighted: true
                onClicked: CalibrationController.store(false)
            }
            Button {
                visible: root.phase === CalibrationController.ChooseStore
                text: qsTr("Store permanently…")
                onClicked: permanentConfirm.open()
            }
        }
    }

    onRejected: CalibrationController.cancel()

    Dialog {
        id: permanentConfirm

        title: qsTr("Store permanently?")
        modal: true
        // On the overlay, not inside the wizard: as the wizard's child it
        // took the wizard's size and grew past its text.
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, (parent ? parent.width : 440) - 48)
        standardButtons: Dialog.Yes | Dialog.Cancel

        contentItem: Label {
            wrapMode: Text.Wrap
            text: qsTr("The new calibration replaces the controller's current one for good. It survives "
                + "resets and works on every computer and console. The only way back is to "
                + "calibrate again.")
        }

        onAccepted: CalibrationController.store(true)
    }
}
