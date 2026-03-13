// =============================================================================
// DeviceList.qml — All discovered devices
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import AuraCastPro 1.0

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 20

        ColumnLayout {
            spacing: 5
            Text {
                text: "Devices"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSizeH2
                font.weight: Font.Bold
                color: Theme.textPrimary
            }
            Text {
                text: "All devices on your network. They connect automatically — no PIN needed."
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSizeSM
                color: Theme.textSecondary
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: deviceManager ? deviceManager.devices : []

            delegate: Rectangle {
                width: ListView.view.width
                height: 76
                radius: Theme.radiusMD
                color: model.isConnected ? Qt.rgba(0.48, 0.70, 1.0, 0.06) : Theme.bgCard
                border.color: model.isConnected ? Theme.borderActive : Theme.borderNormal
                border.width: 1

                Behavior on color { ColorAnimation { duration: Theme.animNormal } }
                Behavior on border.color { ColorAnimation { duration: Theme.animNormal } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 16

                    // Protocol pill
                    Rectangle {
                        width: 52; height: 26; radius: 13
                        color: model.isConnected
                               ? Qt.rgba(0.48, 0.70, 1.0, 0.18)
                               : Theme.bgElevated
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
                        spacing: 4
                        Text {
                            text: model.name || "Unknown Device"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeMD
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        Text {
                            text: (model.ipAddress || "") +
                                  (model.modelIdentifier ? "  ·  " + model.modelIdentifier : "")
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeSM
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                        }
                    }

                    // Status indicator — no manual connect button
                    Rectangle {
                        width: stateTxt.width + 22; height: 30; radius: 15
                        color: {
                            if (model.isConnected) return Qt.rgba(0.49, 0.84, 0.69, 0.15)
                            return Qt.rgba(1, 1, 1, 0.04)
                        }
                        border.color: model.isConnected ? Theme.accentGreen : Theme.borderSubtle
                        border.width: 1

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: model.isConnected ? Theme.accentGreen : Theme.textDisabled
                                visible: model.isConnected
                            }
                            Text {
                                id: stateTxt
                                text: model.isConnected ? "Mirroring" : "Waiting"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontSizeSM
                                font.weight: Font.Medium
                                color: model.isConnected ? Theme.accentGreen : Theme.textDisabled
                            }
                        }
                    }
                }
            }

            // Empty state
            Rectangle {
                anchors.centerIn: parent
                visible: parent.count === 0
                width: 300; height: 140
                radius: Theme.radiusLG
                color: Theme.bgCard
                border.color: Theme.borderSubtle
                border.width: 1

                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    Text {
                        text: "No devices yet"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeMD
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: "Make sure your phone and PC\nare on the same Wi-Fi network."
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeSM
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
    }
}
