import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Item {
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingLG

        ColumnLayout {
            spacing: 4

            Text {
                text: qsTr("Devices")
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSizeH2
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Text {
                text: qsTr("Phones and tablets on your network will appear here.")
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSizeSM
                color: Theme.textSecondary
            }
        }

        ListView {
            id: deviceListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacingMD
            model: deviceManager ? deviceManager.devices : []

            delegate: Rectangle {
                width: ListView.view.width
                height: 84
                radius: Theme.radiusMD
                color: Theme.bgCard
                border.color: Theme.borderNormal
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    Rectangle {
                        width: 64
                        height: 32
                        radius: 16
                        color: Theme.bgElevated
                        border.color: Theme.borderSubtle
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: model.protocol ? model.protocol.toUpperCase() : "?"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeXS
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Text {
                            text: model.name || qsTr("Unknown Device")
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeMD
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        Text {
                            text: (model.ipAddress || "") + ((model.modelIdentifier || "") ? "  " + model.modelIdentifier : "")
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeSM
                            color: Theme.textSecondary
                        }
                    }

                    Rectangle {
                        width: 112
                        height: 38
                        radius: 19
                        color: model.isConnected ? Theme.bgElevated : Theme.accent
                        border.color: model.isConnected ? Theme.borderNormal : "#9bc5ff"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: model.isConnected ? qsTr("Connected") : qsTr("Connect")
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSizeSM
                            font.weight: Font.DemiBold
                            color: model.isConnected ? Theme.textPrimary : Theme.textInverse
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!model.isConnected && deviceManager)
                                    deviceManager.connectDevice(model.deviceId)
                            }
                        }
                    }
                }
            }

            Rectangle {
                anchors.centerIn: parent
                visible: parent.count === 0
                width: 320
                height: 150
                radius: Theme.radiusLG
                color: Theme.bgCard
                border.color: Theme.borderSubtle
                border.width: 1

                Column {
                    anchors.centerIn: parent
                    spacing: 10

                    Text {
                        text: qsTr("No devices found")
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeLG
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        text: qsTr("Make sure your phone and this PC\nare on the same Wi-Fi network.")
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeSM
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
