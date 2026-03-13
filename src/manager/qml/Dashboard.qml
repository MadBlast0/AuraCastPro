// =============================================================================
// Dashboard.qml — AuraCastPro main view - devices only
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import AuraCastPro 1.0

Item {
    id: root
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 22

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 0

            ColumnLayout {
                spacing: 5
                Text {
                    text: "AuraCast Pro"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSizeH2
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: Theme.accentGreen
                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 900 }
                            NumberAnimation { to: 1.0; duration: 900 }
                        }
                    }
                    Text {
                        text: "Listening for devices on your network"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeSM
                        color: Theme.textSecondary
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                width: connCountText.width + 32
                height: 38
                radius: 19
                color: Theme.bgCard
                border.color: Theme.borderNormal
                border.width: 1
                visible: deviceManager && deviceManager.connectedCount > 0

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 7
                    Rectangle { width: 8; height: 8; radius: 4; color: Theme.accentGreen }
                    Text {
                        id: connCountText
                        text: deviceManager ? deviceManager.connectedCount + " connected" : ""
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeSM
                        font.weight: Font.Medium
                        color: Theme.textPrimary
                    }
                }
            }
        }

        // Device panel
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusLG
            color: Theme.bgCard
            border.color: Theme.borderNormal
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                Text {
                    text: "Devices"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSizeMD
                    font.weight: Font.DemiBold
                    color: Theme.textSecondary
                }

                // Empty state
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: !deviceManager || deviceManager.devices.length === 0
                    clip: true

                    Column {
                        anchors.centerIn: parent
                        spacing: 18
                        width: Math.min(parent.width - 40, 400)

                        Rectangle {
                            width: 64
                            height: 64
                            radius: 32
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "transparent"
                            border.color: Theme.accent
                            border.width: 2
                            
                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.3; duration: 1200 }
                                NumberAnimation { to: 1.0; duration: 1200 }
                            }
                            
                            Text {
                                anchors.centerIn: parent
                                text: "\uD83D\uDCE1"
                                font.pixelSize: 28
                            }
                        }

                        Text {
                            text: "Waiting for devices\u2026"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeLG
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        
                        Text {
                            text: "Open AirPlay or Cast on any device\non the same Wi-Fi to connect instantly."
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeSM
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }
                    }
                }

                // Device list
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    visible: deviceManager && deviceManager.devices.length > 0
                    model: deviceManager ? deviceManager.devices : []

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 72
                        radius: Theme.radiusMD
                        color: model.isConnected ? Qt.rgba(0.48, 0.70, 1.0, 0.07) : Theme.bgElevated
                        border.color: model.isConnected ? Theme.borderActive : Theme.borderNormal
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18; anchors.rightMargin: 18
                            spacing: 14

                            Rectangle {
                                width: 48; height: 26; radius: 13
                                color: model.isConnected ? Qt.rgba(0.48, 0.70, 1.0, 0.18) : Theme.bgCard
                                border.color: model.isConnected ? Theme.borderActive : Theme.borderSubtle
                                border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: model.protocol ? model.protocol.toUpperCase() : "?"
                                    font.family: Theme.fontSans
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    color: model.isConnected ? Theme.accent : Theme.textSecondary
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text {
                                    text: model.name || "Unknown Device"
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontSizeMD
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: model.ipAddress || ""
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontSizeSM
                                    color: Theme.textSecondary
                                }
                            }

                            Rectangle {
                                width: statusLbl.width + 20
                                height: 28; radius: 14
                                color: model.isConnected ? Qt.rgba(0.49,0.84,0.69,0.15) : Qt.rgba(1,1,1,0.04)
                                border.color: model.isConnected ? Theme.accentGreen : Theme.borderSubtle
                                border.width: 1
                                Text {
                                    id: statusLbl
                                    anchors.centerIn: parent
                                    text: model.isConnected ? "Mirroring" : "Idle"
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontSizeSM
                                    font.weight: Font.Medium
                                    color: model.isConnected ? Theme.accentGreen : Theme.textDisabled
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
