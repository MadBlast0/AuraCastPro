// PairingDialog.qml — AirPlay PIN entry with full state machine (neo-brutalist)
// Task 187: States: WaitingForPIN → Verifying → Success | Failed
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Dialog {
    id: root
    title: ""
    modal: true
    width: 400; height: 300
    closePolicy: Popup.NoAutoClose  // prevent accidental dismiss during verify

    // ── Public API ────────────────────────────────────────────────────────────
    property string deviceName:  ""
    property int    failCount:   0   // increments on Failed; resets on open()

    // Called by C++ after C++ receives PIN result:
    //   hubModel.pairingResult(success: bool)  → connect to onPairingResult
    function showSuccess() { stateMachine.state = "Success" }
    function showFailed()  {
        failCount++
        stateMachine.state = failCount >= 3 ? "TooManyAttempts" : "Failed"
    }

    // Reset when opened
    onOpened: {
        failCount = 0
        pinInput.text = ""
        stateMachine.state = "WaitingForPIN"
        pinInput.forceActiveFocus()
    }

    // ── Background (brutalist card) ───────────────────────────────────────────
    background: Rectangle {
        color:        Theme.bgCard
        border.color: Theme.borderActive
        border.width: Theme.borderThick
        Rectangle {
            x: Theme.shadowOffX + 2; y: Theme.shadowOffY + 2
            width: parent.width; height: parent.height
            color: Theme.borderActive; z: -1
        }
    }

    // ── State machine ─────────────────────────────────────────────────────────
    Item {
        id: stateMachine
        state: "WaitingForPIN"
        states: [
            State { name: "WaitingForPIN" },
            State { name: "Verifying"     },
            State { name: "Success"        },
            State { name: "Failed"         },
            State { name: "TooManyAttempts" }
        ]
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLG
        spacing: Theme.spacingMD

        // Title
        Text {
            text: root.deviceName.length > 0
                  ? qsTr("PAIR: ") + root.deviceName.toUpperCase()
                  : qsTr("PAIR DEVICE")
            font.family:    Theme.fontMono
            font.pixelSize: Theme.fontSizeXL
            font.weight:    Font.Black
            color:          Theme.textPrimary
            letterSpacing:  4
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Rectangle { height: 2; Layout.fillWidth: true; color: Theme.borderActive }

        // ── WaitingForPIN ─────────────────────────────────────────────────────
        ColumnLayout {
            visible: stateMachine.state === "WaitingForPIN"
            spacing: Theme.spacingMD
            Layout.fillWidth: true

            Text {
                text: qsTr("Enter the PIN displayed on your iPhone:")
                font.family:    Theme.fontMono
                font.pixelSize: Theme.fontSizeSM
                color:          Theme.textSecondary
                wrapMode:       Text.WordWrap
                Layout.fillWidth: true
            }

            // 4-digit PIN input boxes
            RowLayout {
                spacing: 8
                Layout.fillWidth: true

                Repeater {
                    model: 4
                    Rectangle {
                        width: 64; height: 64
                        color:        Theme.bgBase
                        border.color: Theme.borderActive
                        border.width: 2
                        Text {
                            anchors.centerIn: parent
                            text: pinInput.text.length > index ? pinInput.text[index] : ""
                            font.family:    Theme.fontMono
                            font.pixelSize: 28
                            font.weight:    Font.Black
                            color:          Theme.textPrimary
                        }
                    }
                }

                // Hidden TextInput captures keyboard
                TextInput {
                    id: pinInput
                    width: 1; height: 1; opacity: 0
                    maximumLength: 4
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,4}/ }
                    focus: stateMachine.state === "WaitingForPIN"
                    inputMethodHints: Qt.ImhDigitsOnly
                    onTextChanged: if (text.length === 4) pairBtn.submitPin()
                }
            }

            // Tap anywhere on PIN area to focus
            MouseArea {
                width: 4 * 64 + 3 * 8; height: 64
                onClicked: pinInput.forceActiveFocus()
                cursorShape: Qt.IBeamCursor
            }
        }

        // ── Verifying ─────────────────────────────────────────────────────────
        ColumnLayout {
            visible: stateMachine.state === "Verifying"
            spacing: Theme.spacingMD
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item { Layout.fillHeight: true }
            Text {
                text: qsTr("VERIFYING PIN...")
                font.family:    Theme.fontMono
                font.pixelSize: Theme.fontSizeLG
                font.weight:    Font.Black
                color:          Theme.textPrimary
                letterSpacing:  2
                Layout.alignment: Qt.AlignHCenter
            }
            // Animated dots
            Row {
                spacing: 8
                Layout.alignment: Qt.AlignHCenter
                Repeater {
                    model: 3
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: Theme.textPrimary
                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.2; duration: 400 }
                            NumberAnimation { to: 1.0; duration: 400 }
                            PauseAnimation  { duration: index * 150 }
                        }
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }

        // ── Success ───────────────────────────────────────────────────────────
        ColumnLayout {
            visible: stateMachine.state === "Success"
            spacing: Theme.spacingMD
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item { Layout.fillHeight: true }
            Text {
                text: "✓"
                font.family:    Theme.fontMono
                font.pixelSize: 48
                color:          Theme.accentGreen
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: qsTr("PAIRED!")
                font.family:    Theme.fontMono
                font.pixelSize: Theme.fontSizeXL
                font.weight:    Font.Black
                color:          Theme.accentGreen
                letterSpacing:  4
                Layout.alignment: Qt.AlignHCenter
            }
            Item { Layout.fillHeight: true }

            // Auto-close after 1.8 seconds
            Timer {
                running: stateMachine.state === "Success"
                interval: 1800
                onTriggered: root.accept()
            }
        }

        // ── Failed ────────────────────────────────────────────────────────────
        ColumnLayout {
            visible: stateMachine.state === "Failed" || stateMachine.state === "TooManyAttempts"
            spacing: Theme.spacingMD
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item { Layout.fillHeight: true }
            Text {
                text: "✗"
                font.family:    Theme.fontMono
                font.pixelSize: 40
                color:          Theme.accentRed
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: stateMachine.state === "TooManyAttempts"
                      ? qsTr("Too many attempts.\nRestart AirPlay on your device.")
                      : qsTr("Incorrect PIN. Please try again.")
                font.family:    Theme.fontMono
                font.pixelSize: Theme.fontSizeSM
                color:          Theme.accentRed
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
            }
            Item { Layout.fillHeight: true }
        }

        Item { Layout.fillHeight: true }

        // ── Buttons ───────────────────────────────────────────────────────────
        RowLayout {
            visible: stateMachine.state === "WaitingForPIN" || stateMachine.state === "Failed"
            Layout.fillWidth: true
            spacing: Theme.spacingMD

            // Cancel
            Rectangle {
                Layout.fillWidth: true; height: 40
                color: "transparent"
                border.color: Theme.borderNormal; border.width: 2
                Text {
                    anchors.centerIn: parent
                    text: qsTr("CANCEL")
                    font.family:  Theme.fontMono; font.pixelSize: Theme.fontSizeSM
                    font.weight:  Font.Bold; color: Theme.textSecondary; letterSpacing: 2
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: root.reject()
                }
            }

            // Pair / Try Again
            Rectangle {
                id: pairBtn
                Layout.fillWidth: true; height: 40
                color: Theme.textPrimary
                border.color: Theme.borderActive; border.width: 2
                enabled: stateMachine.state === "Failed"
                         || (stateMachine.state === "WaitingForPIN" && pinInput.text.length === 4)
                opacity: enabled ? 1.0 : 0.4
                Rectangle {
                    x: 3; y: 3; width: parent.width; height: parent.height
                    color: Theme.borderSubtle; z: -1
                }
                Text {
                    anchors.centerIn: parent
                    text: stateMachine.state === "Failed" ? qsTr("TRY AGAIN") : qsTr("PAIR")
                    font.family:  Theme.fontMono; font.pixelSize: Theme.fontSizeSM
                    font.weight:  Font.Black; color: Theme.textInverse; letterSpacing: 2
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: pairBtn.submitPin()
                }

                function submitPin() {
                    if (!enabled) return
                    if (stateMachine.state === "Failed") {
                        pinInput.text = ""
                        stateMachine.state = "WaitingForPIN"
                        pinInput.forceActiveFocus()
                        return
                    }
                    stateMachine.state = "Verifying"
                    if (hubModel) hubModel.submitPairingPin(pinInput.text)
                }
            }
        }

        // TooManyAttempts — only Close button
        Rectangle {
            visible: stateMachine.state === "TooManyAttempts"
            Layout.fillWidth: true; height: 40
            color: "transparent"
            border.color: Theme.borderNormal; border.width: 2
            Text {
                anchors.centerIn: parent
                text: qsTr("CLOSE")
                font.family:  Theme.fontMono; font.pixelSize: Theme.fontSizeSM
                font.weight:  Font.Bold; color: Theme.textSecondary; letterSpacing: 2
            }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.reject() }
        }

        // Verifying — Cancel button
        Rectangle {
            visible: stateMachine.state === "Verifying"
            Layout.fillWidth: true; height: 40
            color: "transparent"
            border.color: Theme.borderNormal; border.width: 2
            Text {
                anchors.centerIn: parent
                text: qsTr("CANCEL")
                font.family:  Theme.fontMono; font.pixelSize: Theme.fontSizeSM
                font.weight:  Font.Bold; color: Theme.textSecondary; letterSpacing: 2
            }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.reject() }
        }
    }
}
