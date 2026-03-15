// =============================================================================
// Main.qml -- AuraCastPro root window - macOS-style with rounded corners
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import AuraCastPro 1.0

Rectangle {
    id: root
    width: 1100
    height: 700
    color: "#0f141b"
    
    // Note: Rounded corners don't work on Windows frameless windows
    // This is a Qt/Windows limitation

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // ── Sidebar ────────────────────────────────────────────────────────
            Rectangle {
                width: 200
                Layout.fillHeight: true
                color: "#151c25"

                // Right border
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: "#2b3645"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item { height: 16 }

                    // Nav items
                    Repeater {
                        model: [
                            { icon: "⬛", label: "Dashboard",   page: 0 },
                            { icon: "📱", label: "Devices",     page: 1 },
                            { icon: "📊", label: "Statistics",  page: 2 },
                            { icon: "⚙️",  label: "Settings",   page: 3 },
                            { icon: "🔧", label: "Diagnostics", page: 4 }
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
                            color: "#2b3645"
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
                                    if (s === "mirroring" || s === "connected") return "#7dd7b0"
                                    if (s === "connecting") return "#e6c37a"
                                    if (s === "error") return "#f28b82"
                                    return "#6f7d90"
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: hubModel ? hubModel.connectionStatus : "Idle"
                                font.family: "Inter"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: "#9cacbf"
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            // ── Main content ───────────────────────────────────────────────────
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // Content area
                StackLayout {
                    id: stack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: 0

                    Dashboard {}
                    DeviceList {}
                    StatsPage {}
                    SettingsPage {}
                    DiagnosticsPanel {}
                }
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

    // ── Nav item component ────────────────────────────────────────────────
    component NavItem: Item {
        property string icon:  ""
        property string label: ""
        property int    page:  0
        property bool   current: false
        signal clicked()

        height: 36
        Layout.fillWidth: true

        Rectangle {
            id: navBg
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            radius: 8
            color: current ? "#223044" : "transparent"
            border.width: 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                spacing: 10

                Text {
                    text: icon || ""
                    font.pixelSize: 14
                    visible: false
                }
                Text {
                    text: label
                    font.family: "Inter"
                    font.pixelSize: 13
                    font.weight: current ? Font.DemiBold : Font.Normal
                    color: current ? "#e8eef7" : "#9cacbf"
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: parent.parent.clicked()
                onEntered: if (!current) navBg.color = Qt.rgba(1,1,1,0.03)
                onExited:  if (!current) navBg.color = "transparent"
            }
        }
    }
}
