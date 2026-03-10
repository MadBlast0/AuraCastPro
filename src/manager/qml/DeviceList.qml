// DeviceList.qml — Discovered devices list
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLG
        spacing: Theme.spacingLG

        Text {
            text: qsTr("DEVICES")
            font.family:    Theme.fontMono
            font.pixelSize: Theme.fontSizeH2
            font.weight:    Font.Black
            color:          Theme.textPrimary
            letterSpacing:  4
        }

        // Divider
        Rectangle { height: 2; Layout.fillWidth: true; color: Theme.borderActive }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            id: deviceListView
            // deviceManager is set as a context property in HubWindow::init()
            // Its Q_PROPERTY 'devices' returns QList<DeviceInfo> which QML uses as model
            model: deviceManager ? deviceManager.devices : []
            spacing: Theme.spacingMD

            delegate: Rectangle {
                width:  ListView.view.width
                height: 72
                color:  Theme.bgCard
                border.color: Theme.borderNormal
                border.width: Theme.borderWidthNormal

                // Offset shadow
                Rectangle {
                    x: Theme.shadowOffX; y: Theme.shadowOffY
                    width: parent.width; height: parent.height
                    color: Theme.bgHover
                    z: -1
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMD
                    spacing: Theme.spacingMD

                    // Protocol badge
                    Rectangle {
                        width: 56; height: 28
                        color: qsTr("transparent")
                        border.color: Theme.borderActive
                        border.width: 2
                        Text {
                            anchors.centerIn: parent
                            text: model.protocol ? model.protocol.toUpperCase() : "?"
                            font.family:    Theme.fontMono
                            font.pixelSize: 10
                            font.weight:    Font.Bold
                            color:          Theme.textPrimary
                            letterSpacing:  1
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: model.name || "Unknown Device"
                            font.family:    Theme.fontMono
                            font.pixelSize: Theme.fontSizeMD
                            font.weight:    Font.Bold
                            color:          Theme.textPrimary
                        }
                        Text {
                            text: (model.ipAddress || "") + "  " + (model.modelIdentifier || "")
                            font.family:    Theme.fontMono
                            font.pixelSize: Theme.fontSizeSM
                            color:          Theme.textSecondary
                        }
                    }

                    // Connect button
                    Rectangle {
                        width: 100; height: 36
                        color: model.isConnected ? Theme.bgHover : Theme.textPrimary
                        border.color: Theme.borderActive
                        border.width: 2
                        Text {
                            anchors.centerIn: parent
                            text: model.isConnected ? qsTr("CONNECTED") : qsTr("CONNECT")
                            font.family:    Theme.fontMono
                            font.pixelSize: 11
                            font.weight:    Font.Bold
                            color: model.isConnected ? Theme.textSecondary : Theme.textInverse
                            letterSpacing: 1
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

            // Empty state
            Text {
                anchors.centerIn: parent
                visible: parent.count === 0
                text: qsTr("NO DEVICES FOUND\n\nEnsure your phone is on the\nsame Wi-Fi network.")
                font.family:    Theme.fontMono
                font.pixelSize: Theme.fontSizeSM
                color:          Theme.textDisabled
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.6
            }
        }
    }
}
