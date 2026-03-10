// SettingsPage.qml — Settings panel (neo-brutalist B/W/Grey)
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Item {
    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLG
        spacing: Theme.spacingLG

        // Left: category list
        ColumnLayout {
            width: 160
            Layout.fillHeight: true
            spacing: 0

            Text {
                text: qsTr("SETTINGS")
                font.family:    Theme.fontMono
                font.pixelSize: Theme.fontSizeH2
                font.weight:    Font.Black
                color:          Theme.textPrimary
                letterSpacing:  4
            }
            Rectangle { height: 2; width: parent.width; color: Theme.borderActive }
            Item { height: Theme.spacingMD }

            Repeater {
                id: catRep
                model: ["NETWORK","VIDEO","AUDIO","RECORDING","LICENSE","ABOUT"]
                property int selected: 0
                delegate: Rectangle {
                    width: 160; height: 36
                    color: catRep.selected === index ? Theme.borderActive : qsTr("transparent")
                    border.color: catRep.selected === index ? Theme.borderActive : qsTr("transparent")
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        text:  modelData
                        font.family:    Theme.fontMono
                        font.pixelSize: Theme.fontSizeSM
                        font.weight:    Font.Bold
                        color: catRep.selected === index ? Theme.textInverse : Theme.textSecondary
                        letterSpacing: 1
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { catRep.selected = index; settingsStack.currentIndex = index }
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }

        // Vertical divider
        Rectangle { width: 2; Layout.fillHeight: true; color: Theme.borderActive }

        // Right: content
        StackLayout {
            id: settingsStack
            Layout.fillWidth: true
            Layout.fillHeight: true

            // NETWORK
            SettingsSection {
                title: qsTr("NETWORK")
                rows: [
                    { label: qsTr("Network Interface"), type: qsTr("combo"), key: qsTr("networkInterface") },
                    { label: qsTr("AirPlay Port"),       type: qsTr("number"), key: qsTr("airplayPort")    },
                    { label: qsTr("Cast Port"),          type: qsTr("number"), key: qsTr("castPort")       },
                    { label: qsTr("Max Bitrate (Mbps)"), type: qsTr("number"), key: qsTr("maxBitrateMbps") },
                ]
            }
            // VIDEO
            SettingsSection {
                title: qsTr("VIDEO")
                rows: [
                    { label: qsTr("Target FPS"),     type: qsTr("combo"),  key: qsTr("targetFps")    },
                    { label: qsTr("Quality Preset"), type: qsTr("combo"),  key: qsTr("qualityPreset")},
                    { label: qsTr("Hardware Accel"), type: qsTr("toggle"), key: qsTr("hwAccel")      },
                    { label: qsTr("HDR Output"),     type: qsTr("toggle"), key: qsTr("hdrOutput")    },
                ]
            }
            // AUDIO
            SettingsSection {
                title: qsTr("AUDIO")
                rows: [
                    { label: qsTr("Audio Output Device"),  type: qsTr("combo"),  key: qsTr("audioDevice")  },
                    { label: qsTr("Virtual Audio Cable"),  type: qsTr("toggle"), key: qsTr("virtualAudio") },
                    { label: qsTr("A/V Sync Offset (ms)"), type: qsTr("number"), key: qsTr("avSyncOffsetMs")},
                ]
            }
            // RECORDING
            SettingsSection {
                title: qsTr("RECORDING")
                rows: [
                    { label: qsTr("Output Folder"),   type: qsTr("path"),   key: qsTr("outputFolder") },
                    { label: qsTr("Format"),          type: qsTr("combo"),  key: qsTr("recordFormat") },
                    { label: qsTr("Disk Warn (GB)"),  type: qsTr("number"), key: qsTr("diskWarnGb")   },
                ]
            }
            // LICENSE
            SettingsSection {
                title: qsTr("LICENSE")
                rows: [
                    { label: qsTr("License Key"),  type: qsTr("text"),   key: qsTr("licenseKey")  },
                    { label: qsTr("Edition"),      type: qsTr("label"),  key: qsTr("edition")     },
                    { label: qsTr("Status"),       type: qsTr("label"),  key: qsTr("licenseStatus")},
                ]
            }
            // ABOUT
            SettingsSection {
                title: qsTr("ABOUT")
                rows: [
                    { label: qsTr("Version"),  type: qsTr("label"), key: qsTr("appVersion")  },
                    { label: qsTr("Build"),    type: qsTr("label"), key: qsTr("buildDate")   },
                    { label: qsTr("GPU"),      type: qsTr("label"), key: qsTr("gpuName")     },
                    { label: qsTr("OS"),       type: qsTr("label"), key: qsTr("osVersion")   },
                ]
            }
        }
    }
component SettingsSection: Item {
    property string title: ""
    property var rows: []

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Text {
            text: title
            font.family:    Theme.fontMono
            font.pixelSize: Theme.fontSizeLG
            font.weight:    Font.Black
            color:          Theme.textPrimary
            letterSpacing:  3
        }
        Rectangle { height: 2; Layout.fillWidth: true; color: Theme.borderSubtle }
        Item { height: Theme.spacingMD }

        Repeater {
            model: rows
            delegate: Rectangle {
                Layout.fillWidth: true
                height: 44
                color: index % 2 === 0 ? Theme.bgCard : Theme.bgPanel
                border.color: Theme.borderSubtle
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMD

                    Text {
                        text:  modelData.label
                        font.family:    Theme.fontMono
                        font.pixelSize: Theme.fontSizeSM
                        color:          Theme.textSecondary
                        Layout.preferredWidth: 180
                    }

                    // Value display — simplified for prototype
                    Text {
                        text: settingsModel
                            ? (settingsModel[modelData.key] || "—")
                            : "—"
                        font.family:    Theme.fontMono
                        font.pixelSize: Theme.fontSizeSM
                        font.weight:    Font.Bold
                        color:          Theme.textPrimary
                        Layout.fillWidth: true
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
}
