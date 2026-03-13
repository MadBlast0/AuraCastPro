// =============================================================================
// Main.qml -- AuraCastPro root window
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Item {
    id: root
    width: 1100
    height: 720

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#121923" }
            GradientStop { position: 0.55; color: Theme.bgBase }
            GradientStop { position: 1.0; color: "#0c1117" }
        }
    }

    FontLoader {
        id: fontRegular
        source: "qrc:/fonts/Inter-Regular.ttf"
        onStatusChanged: if (status === FontLoader.Error)
            console.warn("Inter Regular not found -- using fallback")
    }
    FontLoader {
        id: fontBold
        source: "qrc:/fonts/Inter-Bold.ttf"
        onStatusChanged: if (status === FontLoader.Error)
            console.warn("Inter Bold not found -- using fallback")
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            width: 220
            Layout.fillHeight: true
            color: Theme.bgPanel

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.borderSubtle
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                Rectangle {
                    height: 88
                    Layout.fillWidth: true
                    color: "#121923"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.borderSubtle
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 22
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        Text {
                            text: qsTr("AuraCast Pro")
                            font.family: Theme.fontSans
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        Text {
                            text: qsTr("Mirror your devices with less friction")
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeXS
                            color: Theme.textSecondary
                        }
                    }
                }

                Item { height: 18 }

                Repeater {
                    model: [
                        { label: qsTr("Dashboard"), page: 0 },
                        { label: qsTr("Devices"), page: 1 },
                        { label: qsTr("Settings"), page: 2 },
                        { label: qsTr("Diagnostics"), page: 3 }
                    ]

                    delegate: NavItem {
                        label: modelData.label
                        page: modelData.page
                        current: stack.currentIndex === modelData.page
                        onClicked: stack.currentIndex = modelData.page
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    height: 74
                    Layout.fillWidth: true
                    color: Theme.bgPanel

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: 1
                        color: Theme.borderSubtle
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 22
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        Text {
                            text: hubModel ? hubModel.connectionStatus : qsTr("Idle")
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeSM
                            font.weight: Font.Medium
                            color: Theme.statusColour(hubModel ? hubModel.connectionStatus.toLowerCase() : qsTr("idle"))
                        }

                        Text {
                            text: qsTr("v1.0.0")
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeXS
                            color: Theme.textDisabled
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                anchors.fill: parent
                anchors.margins: 18
                radius: Theme.radiusLG
                color: Qt.rgba(1, 1, 1, 0.02)
                border.color: Theme.borderSubtle
                border.width: 1
            }

            StackLayout {
                id: stack
                anchors.fill: parent
                anchors.margins: 18
                currentIndex: 0

                Dashboard {}
                DeviceList {}
                SettingsPage {}
                DiagnosticsPanel {}
            }
        }
    }

    Loader {
        id: firstRunLoader
        anchors.fill: parent
        active: typeof settingsModel !== "undefined"
                && !settingsModel.firstRunCompleted
        source: "qrc:/qml/FirstRunWizard.qml"
        onLoaded: {
            if (item) {
                item.wizardCompleted.connect(function() {
                    settingsModel.firstRunCompleted = true
                    firstRunLoader.active = false
                })
            }
        }
    }

    function toggleMirroring() {
        if (hubModel) {
            if (hubModel.isMirroring) hubModel.stopMirroring()
            else hubModel.startMirroring()
        }
    }

    function toggleRecording() {
        hubModel.toggleRecording()
    }

    function toggleFullscreen() {
    }

    function showDiagnostics() {
        stack.currentIndex = 3
    }

    function checkUpdates() {
        aboutDialog.open()
    }

    function showAbout() {
        aboutDialog.open()
    }

    function showPairingDialog(deviceName, pin) {
        pairingDialog.deviceName = deviceName
        pairingDialog.pinCode = pin
        pairingDialog.open()
    }

    function onPairingSuccess() {
        pairingDialog.close()
    }

    AboutDialog {
        id: aboutDialog
        anchors.centerIn: parent
    }

    PairingDialog {
        id: pairingDialog
        anchors.centerIn: parent
        property string deviceName: ""
        property string pinCode: ""

        Connections {
            target: hubModel
            function onPairingResult(success) {
                if (success) pairingDialog.showSuccess()
                else pairingDialog.showFailed()
            }
        }
    }

    component NavItem: Rectangle {
        property string label: ""
        property int page: 0
        property bool current: false
        signal clicked()

        height: 48
        Layout.fillWidth: true
        color: current ? Theme.bgElevated : "transparent"
        radius: Theme.radiusMD
        border.color: current ? Theme.borderActive : "transparent"
        border.width: 1

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 22
            text: label
            font.family: Theme.fontSans
            font.pixelSize: Theme.fontSizeMD
            font.weight: current ? Font.DemiBold : Font.Medium
            color: current ? Theme.textPrimary : Theme.textSecondary
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}
