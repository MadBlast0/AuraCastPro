import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Item {
    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacingLG

        Rectangle {
            width: 190
            Layout.fillHeight: true
            radius: Theme.radiusLG
            color: Theme.bgCard
            border.color: Theme.borderNormal
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Text {
                    text: qsTr("Settings")
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSizeH2
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Repeater {
                    id: catRep
                    model: ["Network", "Video", "Audio", "Recording", "License", "About"]
                    property int selected: 0

                    delegate: Rectangle {
                        width: 154
                        height: 38
                        radius: 19
                        color: catRep.selected === index ? Theme.bgElevated : "transparent"
                        border.color: catRep.selected === index ? Theme.borderActive : "transparent"
                        border.width: 1

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            text: modelData
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeSM
                            font.weight: catRep.selected === index ? Font.DemiBold : Font.Medium
                            color: catRep.selected === index ? Theme.textPrimary : Theme.textSecondary
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                catRep.selected = index
                                settingsStack.currentIndex = index
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        StackLayout {
            id: settingsStack
            Layout.fillWidth: true
            Layout.fillHeight: true

            SettingsSection {
                title: qsTr("Network")
                rows: [
                    { label: qsTr("Network Interface"), key: qsTr("networkInterface") },
                    { label: qsTr("AirPlay Port"), key: qsTr("airplayPort") },
                    { label: qsTr("Cast Port"), key: qsTr("castPort") },
                    { label: qsTr("Max Bitrate (Mbps)"), key: qsTr("maxBitrateMbps") }
                ]
            }
            SettingsSection {
                title: qsTr("Video")
                rows: [
                    { label: qsTr("Target FPS"), key: qsTr("targetFps") },
                    { label: qsTr("Quality Preset"), key: qsTr("qualityPreset") },
                    { label: qsTr("Hardware Accel"), key: qsTr("hwAccel") },
                    { label: qsTr("HDR Output"), key: qsTr("hdrOutput") }
                ]
            }
            SettingsSection {
                title: qsTr("Audio")
                rows: [
                    { label: qsTr("Audio Output Device"), key: qsTr("audioDevice") },
                    { label: qsTr("Virtual Audio Cable"), key: qsTr("virtualAudio") },
                    { label: qsTr("A/V Sync Offset (ms)"), key: qsTr("avSyncOffsetMs") }
                ]
            }
            SettingsSection {
                title: qsTr("Recording")
                rows: [
                    { label: qsTr("Output Folder"), key: qsTr("outputFolder") },
                    { label: qsTr("Format"), key: qsTr("recordFormat") },
                    { label: qsTr("Disk Warn (GB)"), key: qsTr("diskWarnGb") }
                ]
            }
            SettingsSection {
                title: qsTr("License")
                rows: [
                    { label: qsTr("License Key"), key: qsTr("licenseKey") },
                    { label: qsTr("Edition"), key: qsTr("edition") },
                    { label: qsTr("Status"), key: qsTr("licenseStatus") }
                ]
            }
            SettingsSection {
                title: qsTr("About")
                rows: [
                    { label: qsTr("Version"), key: qsTr("appVersion") },
                    { label: qsTr("Build"), key: qsTr("buildDate") },
                    { label: qsTr("GPU"), key: qsTr("gpuName") },
                    { label: qsTr("OS"), key: qsTr("osVersion") }
                ]
            }
        }
    }

    component SettingsSection: Rectangle {
        property string title: ""
        property var rows: []

        radius: Theme.radiusLG
        color: Theme.bgCard
        border.color: Theme.borderNormal
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Text {
                text: title
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSizeH2
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Repeater {
                model: rows

                delegate: Rectangle {
                    Layout.fillWidth: true
                    height: 52
                    radius: Theme.radiusMD
                    color: index % 2 === 0 ? Theme.bgInput : Theme.bgPanel
                    border.color: Theme.borderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14

                        Text {
                            text: modelData.label
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeSM
                            color: Theme.textSecondary
                            Layout.preferredWidth: 190
                        }

                        Text {
                            text: settingsModel ? (settingsModel[modelData.key] || "--") : "--"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeSM
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
