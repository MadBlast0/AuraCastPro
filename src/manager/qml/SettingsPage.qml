import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Item {
    id: root

    ScrollView {
        anchors.fill: parent
        anchors.margins: 28
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 20

            // Header
            Text {
                text: "Settings"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSizeH2
                font.weight: Font.Bold
                color: Theme.textPrimary
            }

            // Quick Actions Section
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: quickActionsContent.height + 32
                radius: 12
                color: Theme.bgCard
                border.color: Theme.borderNormal
                border.width: 1

                ColumnLayout {
                    id: quickActionsContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 20
                    spacing: 16

                    Text {
                        text: "Quick Actions"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeMD
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ActionButton {
                            Layout.fillWidth: true
                            text: "📖 User Guide"
                            onClicked: Qt.openUrlExternally("https://auracastpro.com/guide")
                        }
                        ActionButton {
                            Layout.fillWidth: true
                            text: "📋 View Logs"
                            onClicked: { /* Navigate to diagnostics */ }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ActionButton {
                            Layout.fillWidth: true
                            text: "🔄 Check Updates"
                            onClicked: { /* Check for updates */ }
                        }
                        ActionButton {
                            Layout.fillWidth: true
                            text: "ℹ️ About"
                            onClicked: { /* Show about */ }
                        }
                    }
                }
            }

            // View Options Section
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: viewOptionsContent.height + 32
                radius: 12
                color: Theme.bgCard
                border.color: Theme.borderNormal
                border.width: 1

                ColumnLayout {
                    id: viewOptionsContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 20
                    spacing: 12

                    Text {
                        text: "View Options"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeMD
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    ToggleRow {
                        Layout.fillWidth: true
                        label: "Dark Mode"
                        description: "Use dark color theme"
                        checked: true
                        onToggled: { /* Toggle theme */ }
                    }

                    ToggleRow {
                        Layout.fillWidth: true
                        label: "Auto-connect Devices"
                        description: "Connect to known devices automatically"
                        checked: settingsModel ? settingsModel.autoConnect : true
                        onToggled: if (settingsModel) settingsModel.autoConnect = checked
                    }
                }
            }

            // Recording Controls Section
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: recordingContent.height + 32
                radius: 12
                color: Theme.bgCard
                border.color: Theme.borderNormal
                border.width: 1

                ColumnLayout {
                    id: recordingContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 20
                    spacing: 12

                    Text {
                        text: "Recording"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeMD
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 56
                        radius: Theme.radiusMD
                        color: Theme.accentRed
                        border.color: Qt.darker(Theme.accentRed, 1.2)
                        border.width: 2

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 10

                            Rectangle {
                                width: 16
                                height: 16
                                radius: 8
                                color: "white"
                            }

                            Text {
                                text: "Start Recording"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontSizeMD
                                font.weight: Font.Bold
                                color: "white"
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { /* Start recording */ }
                        }
                    }

                    InfoRow {
                        Layout.fillWidth: true
                        label: "Output Folder"
                        value: settingsModel ? settingsModel.recordingFolder : "~/Videos"
                    }

                    InfoRow {
                        Layout.fillWidth: true
                        label: "Format"
                        value: "MP4 (H.265)"
                    }
                }
            }

            // Network Section
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: networkContent.height + 32
                radius: 12
                color: Theme.bgCard
                border.color: Theme.borderNormal
                border.width: 1

                ColumnLayout {
                    id: networkContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 20
                    spacing: 12

                    Text {
                        text: "Network"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeMD
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    InfoRow {
                        Layout.fillWidth: true
                        label: "AirPlay Port"
                        value: "7236"
                    }

                    InfoRow {
                        Layout.fillWidth: true
                        label: "Cast Port"
                        value: "8009"
                    }

                    InfoRow {
                        Layout.fillWidth: true
                        label: "Max Bitrate"
                        value: "50 Mbps"
                    }
                }
            }

            // About Section
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: aboutContent.height + 32
                radius: 12
                color: Theme.bgCard
                border.color: Theme.borderNormal
                border.width: 1

                ColumnLayout {
                    id: aboutContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 20
                    spacing: 12

                    Text {
                        text: "About"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeMD
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    InfoRow {
                        Layout.fillWidth: true
                        label: "Version"
                        value: "1.0.0"
                    }

                    InfoRow {
                        Layout.fillWidth: true
                        label: "Build"
                        value: "Release"
                    }

                    InfoRow {
                        Layout.fillWidth: true
                        label: "License"
                        value: licenseManager ? licenseManager.edition : "Free"
                    }
                }
            }

            Item { height: 20 }
        }
    }

    // Action Button Component
    component ActionButton: Rectangle {
        property string text: ""
        signal clicked()
        
        height: 44
        radius: 8
        color: Theme.bgElevated
        border.width: 0

        Text {
            anchors.centerIn: parent
            text: parent.text
            font.family: Theme.fontSans
            font.pixelSize: 13
            font.weight: Font.Medium
            color: Theme.textPrimary
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
            onEntered: parent.color = Theme.bgHover
            onExited: parent.color = Theme.bgElevated
        }
    }

    // Toggle Row Component
    component ToggleRow: Rectangle {
        property string label: ""
        property string description: ""
        property bool checked: false
        signal toggled(bool checked)

        height: 64
        radius: Theme.radiusMD
        color: Theme.bgElevated
        border.color: Theme.borderSubtle
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: label
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSizeSM
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }
                Text {
                    text: description
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSizeXS
                    color: Theme.textSecondary
                }
            }

            Rectangle {
                width: 48
                height: 26
                radius: 13
                color: checked ? Theme.accent : Theme.bgInput
                border.color: checked ? Theme.accent : Theme.borderNormal
                border.width: 1

                Rectangle {
                    width: 20
                    height: 20
                    radius: 10
                    color: "white"
                    x: checked ? parent.width - width - 3 : 3
                    y: 3

                    Behavior on x { NumberAnimation { duration: 150 } }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        checked = !checked
                        toggled(checked)
                    }
                }
            }
        }
    }

    // Info Row Component
    component InfoRow: Rectangle {
        property string label: ""
        property string value: ""

        height: 48
        radius: Theme.radiusMD
        color: Theme.bgElevated
        border.color: Theme.borderSubtle
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Text {
                text: label
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSizeSM
                color: Theme.textSecondary
                Layout.preferredWidth: 120
            }

            Text {
                text: value
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSizeSM
                font.weight: Font.Medium
                color: Theme.textPrimary
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
    }
}
