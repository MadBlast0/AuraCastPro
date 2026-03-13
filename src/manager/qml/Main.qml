// =============================================================================
// Main.qml -- AuraCastPro root window - clean no-friction layout
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Rectangle {
    id: root
    width: 1100
    height: 700
    color: Theme.bgBase

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Sidebar ────────────────────────────────────────────────────────
        Rectangle {
            width: 200
            Layout.fillHeight: true
            color: Theme.bgPanel

            // Right border
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.borderSubtle
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Logo area
                Rectangle {
                    height: 80
                    Layout.fillWidth: true
                    color: "#0e131a"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.borderSubtle
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 20
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 3

                        Text {
                            text: "AuraCast Pro"
                            font.family: Theme.fontSans
                            font.pixelSize: 18
                            font.weight: Font.Bold
                            color: Theme.textPrimary
                        }
                        Text {
                            text: "v1.0.0"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeXS
                            color: Theme.textDisabled
                        }
                    }
                }

                Item { height: 16 }

                // Nav items
                Repeater {
                    model: [
                        { icon: "⬛", label: "Dashboard",   page: 0 },
                        { icon: "📱", label: "Devices",     page: 1 },
                        { icon: "⚙️",  label: "Settings",   page: 2 },
                        { icon: "📊", label: "Diagnostics", page: 3 }
                    ]

                    delegate: NavItem {
                        icon:    modelData.icon
                        label:   modelData.label
                        page:    modelData.page
                        current: stack.currentIndex === modelData.page
                        onClicked: stack.currentIndex = modelData.page
                    }
                }

                Item { Layout.fillHeight: true }

                // Status footer
                Rectangle {
                    height: 68
                    Layout.fillWidth: true
                    color: "transparent"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: 1
                        color: Theme.borderSubtle
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 14
                        spacing: 10

                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: {
                                var s = hubModel ? hubModel.connectionStatus.toLowerCase() : ""
                                if (s === "mirroring" || s === "connected") return Theme.accentGreen
                                if (s === "connecting") return Theme.accentYellow
                                if (s === "error") return Theme.accentRed
                                return Theme.textDisabled
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: hubModel ? hubModel.connectionStatus : "Idle"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeSM
                            font.weight: Font.Medium
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        // ── Main content ───────────────────────────────────────────────────
        StackLayout {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0

            Dashboard {}
            DeviceList {}
            SettingsPage {}
            DiagnosticsPanel {}
        }
    }

    // First run wizard overlay
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

    // About dialog
    AboutDialog {
        id: aboutDialog
        anchors.centerIn: parent
    }

    // Public functions used by HubWindow
    function showDiagnostics()   { stack.currentIndex = 3 }
    function checkUpdates()      { aboutDialog.open() }
    function showAbout()         { aboutDialog.open() }
    function toggleRecording()   { if (hubModel) hubModel.toggleRecording() }
    function toggleFullscreen()  {}

    // No pairing dialog — PIN-free

    // ── Nav item component ────────────────────────────────────────────────
    component NavItem: Item {
        property string icon:  ""
        property string label: ""
        property int    page:  0
        property bool   current: false
        signal clicked()

        height: 46
        Layout.fillWidth: true

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            radius: Theme.radiusMD
            color: current ? Theme.bgElevated : "transparent"
            border.color: current ? Theme.borderActive : "transparent"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                spacing: 10

                Text {
                    text: parent.parent.icon || ""
                    font.pixelSize: 14
                    visible: false  // hide emoji, just use text label
                }
                Text {
                    text: parent.parent.label
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSizeSM
                    font.weight: current ? Font.DemiBold : Font.Normal
                    color: current ? Theme.textPrimary : Theme.textSecondary
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: parent.parent.clicked()
                onEntered: if (!current) parent.color = Qt.rgba(1,1,1,0.03)
                onExited:  if (!current) parent.color = "transparent"
            }
        }
    }
}
