// =============================================================================
// FirstRunWizard.qml — Onboarding wizard shown on first launch
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Window {
    id: wizard
    title: qsTr("Welcome to AuraCastPro")
    width: 580; height: 520
    minimumWidth: 520; minimumHeight: 480
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    modality: Qt.ApplicationModal

    signal wizardCompleted()

    property int currentStep: 0
    readonly property int totalSteps: 4

    readonly property color bgDeep:    "#0D1117"
    readonly property color bgCard:    "#1C2128"
    readonly property color accent:    "#58A6FF"
    readonly property color textMain:  "#E6EDF3"
    readonly property color textMuted: "#8B949E"

    Rectangle {
        anchors.fill: parent
        color: bgDeep

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0
            spacing: 0

            // ── Progress bar ──────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true; height: 3; color: "#161B22"
                Rectangle {
                    height: parent.height; radius: 1.5; color: accent
                    width: parent.width * (wizard.currentStep + 1) / wizard.totalSteps
                    Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                }
            }

            // ── Step content ──────────────────────────────────────────────
            StackLayout {
                Layout.fillWidth: true; Layout.fillHeight: true
                currentIndex: wizard.currentStep

                // Step 0: Welcome
                WizardStep {
                    icon: "🎉"
                    title: qsTr("Welcome to AuraCastPro")
                    description: qsTr("The professional screen mirroring solution for Windows.\n\nYour iPhone and Android devices will appear in your PC — with ultra-low latency, hardware decoding, and seamless audio.")
                    bulletPoints: ["iOS AirPlay 2 — appears in Screen Mirroring", "Android Cast — appears in Cast menu", "USB via ADB — zero Wi-Fi dependency", "< 70ms end-to-end latency"]
                }

                // Step 1: Network setup
                WizardStep {
                    icon: "🌐"
                    title: qsTr("Network Setup")
                    description: qsTr("AuraCastPro opens firewall ports automatically during installation.\n\nFor best results, connect your PC and phone to the same Wi-Fi network.")
                    bulletPoints: ["TCP 7236 — AirPlay 2", "TCP 8009 — Google Cast", "UDP 7236-7238 — Video / Audio / Timing"]
                    extraContent: ColumnLayout {
                        spacing: 8
                        Rectangle {
                            Layout.fillWidth: true; height: 40; radius: 8; color: "#1C2128"
                            border.color: "#30363D"
                            RowLayout {
                                anchors { fill: parent; margins: 12 }
                                Text { text: "🔍"; font.pixelSize: 14 }
                                Text { text: qsTr("Scanning local network..."); color: "#8B949E"; font.pixelSize: 12 }
                                Item { Layout.fillWidth: true }
                                BusyIndicator { running: wizard.currentStep === 1; width: 20; height: 20 }
                            }
                        }
                    }
                }

                // Step 2: Display name
                WizardStep {
                    icon: "📱"
                    title: qsTr("Choose a Display Name")
                    description: qsTr("This is the name your devices will see when searching for screen mirroring targets.")
                    extraContent: ColumnLayout {
                        spacing: 12
                        TextField {
                            id: nameField
                            Layout.fillWidth: true
                            text: settingsModel ? settingsModel.displayName : qsTr("AuraCastPro")
                            placeholderText: qsTr("My AuraCastPro")
                            color: "#E6EDF3"
                            font.pixelSize: 16
                            background: Rectangle { color: "#1C2128"; border.color: "#58A6FF"; border.width: 2; radius: 8 }
                            onTextChanged: if (settingsModel) settingsModel.displayName = text
                        }
                        Text {
                            text: qsTr("Appears as: '") + (nameField.text || "AuraCastPro") + "' in Screen Mirroring"
                            color: "#8B949E"; font.pixelSize: 12
                        }
                    }
                }

                // Step 3: Ready
                WizardStep {
                    icon: "✅"
                    title: qsTr("You're All Set!")
                    description: qsTr("AuraCastPro is ready. Open Screen Mirroring on your iPhone or the Cast menu on Android and select your PC.")
                    bulletPoints: [
                        "Press Ctrl+Shift+O to toggle the stats overlay",
                        "Press Ctrl+R to start / stop recording",
                        "Press F for fullscreen in the mirror window",
                        "Right-click in mirror window = long press on device"
                    ]
                }
            }

            // ── Navigation buttons ────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true; height: 1; color: "#21262D"
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 20
                spacing: 12

                // Step indicator dots
                Row {
                    spacing: 8
                    Repeater {
                        model: wizard.totalSteps
                        Rectangle {
                            width: index === wizard.currentStep ? 20 : 8
                            height: 8; radius: 4
                            color: index === wizard.currentStep ? accent : "#30363D"
                            Behavior on width { NumberAnimation { duration: 200 } }
                            Behavior on color { ColorAnimation { duration: 200 } }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Back")
                    visible: wizard.currentStep > 0
                    background: Rectangle { color: "#21262D"; radius: 8; border.color: "#30363D" }
                    contentItem: Text { text: parent.text; color: "#E6EDF3"; horizontalAlignment: Text.AlignHCenter }
                    onClicked: wizard.currentStep--
                }
                Button {
                    text: wizard.currentStep === wizard.totalSteps - 1 ? qsTr("Get Started") : qsTr("Next")
                    background: Rectangle { color: accent; radius: 8 }
                    contentItem: Text { text: parent.text; color: qsTr("white"); font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    onClicked: {
                        if (wizard.currentStep === wizard.totalSteps - 1) {
                            if (settingsModel) { settingsModel.firstRunCompleted = true; settingsModel.save(); }
                            wizard.wizardCompleted();
                            wizard.close();
                        } else {
                            wizard.currentStep++;
                        }
                    }
                }
            }
        }
    }

    // ── Reusable step component ───────────────────────────────────────────
    component WizardStep: Item {
        property string icon: ""
        property string title: ""
        property string description: ""
        property var bulletPoints: []
        default property alias extraContent: extraSlot.data

        ColumnLayout {
            anchors { fill: parent; margins: 32 }
            spacing: 16

            Text { text: icon; font.pixelSize: 48 }
            Text { text: title; color: textMain; font.pixelSize: 22; font.bold: true }
            Text {
                text: description
                color: textMuted; font.pixelSize: 13
                wrapMode: Text.WordWrap; Layout.fillWidth: true
            }

            Repeater {
                model: bulletPoints
                RowLayout {
                    spacing: 8
                    Text { text: "•"; color: accent; font.pixelSize: 13 }
                    Text { text: modelData; color: textMain; font.pixelSize: 13 }
                }
            }

            ColumnLayout { id: extraSlot; Layout.fillWidth: true; spacing: 8 }
            Item { Layout.fillHeight: true }
        }
    }
}
