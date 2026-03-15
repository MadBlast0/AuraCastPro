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
        spacing: 22

        // Header
        ColumnLayout {
            spacing: 5
            Text {
                text: "Devices"
                font.family: "Inter"
                font.pixelSize: 24
                font.weight: Font.Bold
                color: "#e8eef7"
            }
            Text {
                text: "All devices on your network. They connect automatically — no PIN needed."
                font.family: "Inter"
                font.pixelSize: 13
                color: "#9cacbf"
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
                radius: 12
                color: model.isConnected ? "rgba(123, 179, 255, 0.07)" : "#1c2531"
                border.color: model.isConnected ? "#7bb3ff" : "#3b4a5f"
                border.width: 1

                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on border.color { ColorAnimation { duration: 200 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 16

                    // Protocol pill
                    Rectangle {
                        width: 52; height: 26; radius: 13
                        color: model.isConnected ? "rgba(123, 179, 255, 0.18)" : "#223044"
                        border.color: model.isConnected ? "#7bb3ff" : "#2b3645"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: model.protocol ? model.protocol.toUpperCase() : "?"
                            font.family: "Inter"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            color: model.isConnected ? "#7bb3ff" : "#9cacbf"
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: model.name || "Unknown Device"
                            font.family: "Inter"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#e8eef7"
                            elide: Text.ElideRight
                        }
                        Text {
                            text: (model.ipAddress || "") +
                                  (model.modelIdentifier ? "  ·  " + model.modelIdentifier : "")
                            font.family: "Inter"
                            font.pixelSize: 12
                            color: "#9cacbf"
                            elide: Text.ElideRight
                        }
                    }

                    // Status indicator
                    Rectangle {
                        width: stateTxt.width + 22; height: 30; radius: 15
                        color: model.isConnected ? "rgba(125, 215, 176, 0.15)" : "rgba(255, 255, 255, 0.04)"
                        border.color: model.isConnected ? "#7dd7b0" : "#2b3645"
                        border.width: 1

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: model.isConnected ? "#7dd7b0" : "#6f7d90"
                                visible: model.isConnected
                            }
                            Text {
                                id: stateTxt
                                text: model.isConnected ? "Mirroring" : "Waiting"
                                font.family: "Inter"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: model.isConnected ? "#7dd7b0" : "#6f7d90"
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
                radius: 12
                color: "#1c2531"
                border.color: "#3b4a5f"
                border.width: 1

                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    Text {
                        text: "No devices yet"
                        font.family: "Inter"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: "#e8eef7"
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: "Make sure your phone and PC\nare on the same Wi-Fi network."
                        font.family: "Inter"
                        font.pixelSize: 12
                        color: "#9cacbf"
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
    }
}
