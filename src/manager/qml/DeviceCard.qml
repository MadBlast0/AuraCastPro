// =============================================================================
// DeviceCard.qml — Single device card in the device list
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: card
    height: 76
    radius: 10
    color: mouseArea.containsMouse ? "#21262D" : "#1C2128"
    border.color: isStreaming ? "#3FB950" : "#30363D"
    border.width: isStreaming ? 2 : 1

    property string deviceId: ""
    property string deviceName: qsTr("Unknown Device")
    property string deviceType: qsTr("iOS")    // "iOS", "Android", "USB"
    property string deviceState: qsTr("Idle")  // "Idle", "Connected", "Streaming", "Pairing"
    property string ipAddress: ""
    property real   latencyMs: 0
    property real   bitrateKbps: 0
    property bool   isStreaming: deviceState === "Streaming"

    signal mirrorRequested(string deviceId)
    signal disconnectRequested(string deviceId)

    Behavior on color { ColorAnimation { duration: 120 } }

    RowLayout {
        anchors { fill: parent; margins: 12 }
        spacing: 12

        // Device icon
        Rectangle {
            width: 44; height: 44; radius: 10
            color: {
                if (deviceType === "iOS")     return "#1A3A5C"
                if (deviceType === "Android") return "#1A4A2A"
                return "#3A3A1A"
            }
            Text {
                anchors.centerIn: parent
                text: deviceType === "iOS" ? "📱" : deviceType === "Android" ? "🤖" : "🔌"
                font.pixelSize: 22
            }
        }

        // Device info
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: deviceName
                color: "#E6EDF3"; font.pixelSize: 13; font.bold: true
                elide: Text.ElideRight; Layout.fillWidth: true
            }

            RowLayout {
                spacing: 8
                Text {
                    text: deviceState
                    color: {
                        if (deviceState === "Streaming") return "#3FB950"
                        if (deviceState === "Connected") return "#58A6FF"
                        if (deviceState === "Pairing")   return "#E3B341"
                        return "#8B949E"
                    }
                    font.pixelSize: 11
                }
                Text {
                    visible: isStreaming && latencyMs > 0
                    text: latencyMs.toFixed(0) + "ms"
                    color: latencyMs < 50 ? "#3FB950" : latencyMs < 100 ? "#E3B341" : "#F85149"
                    font.pixelSize: 11
                }
                Text {
                    visible: isStreaming && bitrateKbps > 0
                    text: (bitrateKbps / 1000).toFixed(1) + " Mbps"
                    color: "#8B949E"; font.pixelSize: 11
                }
            }
        }

        // Action buttons
        RowLayout {
            spacing: 6
            visible: mouseArea.containsMouse || isStreaming

            Button {
                visible: !isStreaming && deviceState === "Connected"
                text: qsTr("Mirror")
                background: Rectangle { color: "#238636"; radius: 6 }
                contentItem: Text { text: parent.text; color: qsTr("white"); font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                height: 28
                onClicked: card.mirrorRequested(deviceId)
            }

            Button {
                visible: isStreaming
                text: qsTr("Stop")
                background: Rectangle { color: "#DA3633"; radius: 6 }
                contentItem: Text { text: parent.text; color: qsTr("white"); font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                height: 28
                onClicked: card.disconnectRequested(deviceId)
            }

            Button {
                visible: deviceState !== "Idle"
                text: qsTr("✕")
                flat: true
                background: Rectangle { color: qsTr("transparent") }
                contentItem: Text { text: parent.text; color: "#8B949E"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                height: 28; width: 28
                onClicked: card.disconnectRequested(deviceId)
            }
        }
    }

    // Streaming pulse animation
    Rectangle {
        visible: isStreaming
        anchors { right: parent.right; top: parent.top; margins: 10 }
        width: 8; height: 8; radius: 4
        color: "#3FB950"
        SequentialAnimation on opacity {
            running: isStreaming; loops: Animation.Infinite
            NumberAnimation { to: 0.2; duration: 800 }
            NumberAnimation { to: 1.0; duration: 800 }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onDoubleClicked: if (deviceState === "Connected") card.mirrorRequested(deviceId)
    }
}
