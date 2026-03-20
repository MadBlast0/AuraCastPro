// =============================================================================
// Dashboard.qml — Main view with Record button
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

        // Header with Record button
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                spacing: 5
                Text {
                    text: "Dashboard"
                    font.family: "Inter"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    color: "#e8eef7"
                }
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: "#7dd7b0"
                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 900 }
                            NumberAnimation { to: 1.0; duration: 900 }
                        }
                    }
                    Text {
                        text: "Listening for devices on your network"
                        font.family: "Inter"
                        font.pixelSize: 13
                        color: "#9cacbf"
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Device panel
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            radius: 12
            color: "#1c2531"
            border.color: "#3b4a5f"
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                Text {
                    text: "Devices"
                    font.family: "Inter"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: "#9cacbf"
                }

                // Empty state
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: !deviceManager || deviceManager.devices.length === 0

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
                            border.color: "#7bb3ff"
                            border.width: 2
                            
                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.3; duration: 1200 }
                                NumberAnimation { to: 1.0; duration: 1200 }
                            }
                            
                            Text {
                                anchors.centerIn: parent
                                text: "📡"
                                font.pixelSize: 28
                            }
                        }

                        Text {
                            text: "Waiting for devices..."
                            font.family: "Inter"
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            color: "#e8eef7"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        
                        Text {
                            text: "Open AirPlay or Cast on any device\non the same Wi-Fi to connect instantly."
                            font.family: "Inter"
                            font.pixelSize: 13
                            color: "#9cacbf"
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
                        radius: 8
                        color: model.isConnected ? Qt.rgba(123/255, 179/255, 255/255, 0.07) : "#223044"
                        border.color: model.isConnected ? "#7bb3ff" : "#3b4a5f"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 14

                            Rectangle {
                                width: 48
                                height: 26
                                radius: 13
                                color: model.isConnected ? Qt.rgba(123/255, 179/255, 255/255, 0.18) : "#1c2531"
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
                                spacing: 3
                                Text {
                                    text: model.name || "Unknown Device"
                                    font.family: "Inter"
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    color: "#e8eef7"
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: model.ipAddress || ""
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    color: "#9cacbf"
                                }
                            }

                            Rectangle {
                                width: statusLbl.width + 20
                                height: 28
                                radius: 14
                                color: model.isConnected ? Qt.rgba(125/255, 215/255, 176/255, 0.15) : Qt.rgba(1, 1, 1, 0.04)
                                border.color: model.isConnected ? "#7dd7b0" : "#2b3645"
                                border.width: 1
                                Text {
                                    id: statusLbl
                                    anchors.centerIn: parent
                                    text: model.isConnected ? "Mirroring" : "Idle"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    color: model.isConnected ? "#7dd7b0" : "#6f7d90"
                                }
                            }
                        }
                    }
                }
            }
        }

        // Video Preview Panel
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: "#1c2531"
            border.color: "#3b4a5f"
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Video Preview"
                        font.family: "Inter"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: "#9cacbf"
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        width: statusDot.width + statusText.width + 16
                        height: 24
                        radius: 12
                        color: hubModel && hubModel.connectionStatus === "MIRRORING" ? Qt.rgba(125/255, 215/255, 176/255, 0.15) : Qt.rgba(1, 1, 1, 0.04)
                        border.color: hubModel && hubModel.connectionStatus === "MIRRORING" ? "#7dd7b0" : "#2b3645"
                        border.width: 1
                        
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6
                            Rectangle {
                                id: statusDot
                                width: 6
                                height: 6
                                radius: 3
                                color: hubModel && hubModel.connectionStatus === "MIRRORING" ? "#7dd7b0" : "#6f7d90"
                            }
                            Text {
                                id: statusText
                                text: hubModel ? hubModel.connectionStatus : "Idle"
                                font.family: "Inter"
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                color: hubModel && hubModel.connectionStatus === "MIRRORING" ? "#7dd7b0" : "#6f7d90"
                            }
                        }
                    }
                }

                // Video display area
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#0f141b"
                    border.color: "#2b3645"
                    border.width: 1

                    // Actual video display
                    Image {
                        id: videoImage
                        anchors.fill: parent
                        anchors.margins: 1
                        fillMode: Image.PreserveAspectFit
                        source: hubModel && hubModel.videoFrame && !hubModel.videoFrame.isNull() 
                                ? "image://videoframe/" + Date.now() 
                                : ""
                        cache: false
                        asynchronous: false
                        visible: hubModel && hubModel.connectionStatus === "MIRRORING" && 
                                 hubModel.videoFrame && !hubModel.videoFrame.isNull()
                    }

                    // Placeholder when no video
                    Column {
                        anchors.centerIn: parent
                        spacing: 16
                        visible: !hubModel || hubModel.connectionStatus !== "MIRRORING" || 
                                 !hubModel.videoFrame || hubModel.videoFrame.isNull()

                        Text {
                            text: "📺"
                            font.pixelSize: 48
                            anchors.horizontalCenter: parent.horizontalCenter
                            opacity: 0.3
                        }

                        Text {
                            text: "No active stream"
                            font.family: "Inter"
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            color: "#6f7d90"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            text: "Connect a device to see the video preview here"
                            font.family: "Inter"
                            font.pixelSize: 12
                            color: "#4a5568"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }
        }
    }

}
