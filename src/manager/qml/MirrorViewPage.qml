// MirrorViewPage.qml — Full mirroring view with overlay controls
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property bool mirroring: false
    property string deviceName: ""
    property real fps: 0
    property real latencyMs: 0
    property real bitrateKbps: 0
    property bool showStats: true
    property bool recording: false
    property int recordingSeconds: 0

    signal stopRequested()
    signal recordToggled()
    signal fullscreenToggled()
    signal screenshotRequested()

    // Dark background when no stream
    Rectangle {
        anchors.fill: parent
        color: "#0f0f1a"
        visible: !root.mirroring
        Text {
            anchors.centerIn: parent
            text: root.deviceName.length > 0 ?
                "Connecting to " + root.deviceName + "…" :
                "No device connected"
            color: "#585b70"
            font.pixelSize: 18
        }
    }

    // Stats overlay (top-right)
    Rectangle {
        anchors.top: parent.top; anchors.right: parent.right
        anchors.margins: 16
        visible: root.showStats && root.mirroring
        width: statsCol.implicitWidth + 20
        height: statsCol.implicitHeight + 16
        color: "#aa1e1e2e"; radius: 8

        ColumnLayout {
            id: statsCol
            anchors.centerIn: parent
            spacing: 3
            Text { text: root.fps.toFixed(1) + " fps"; color: "#a6e3a1"; font.pixelSize: 12; font.family: "Consolas" }
            Text { text: root.latencyMs.toFixed(1) + " ms";  color: "#89b4fa"; font.pixelSize: 12; font.family: "Consolas" }
            Text { text: (root.bitrateKbps/1000).toFixed(2) + " Mbps"; color: "#fab387"; font.pixelSize: 12; font.family: "Consolas" }
        }
    }

    // Device name (top-left)
    Text {
        anchors.top: parent.top; anchors.left: parent.left
        anchors.margins: 16
        text: root.deviceName
        color: "#cdd6f4"; font.pixelSize: 14; font.bold: true
        visible: root.mirroring
        style: Text.Outline; styleColor: "#000000"
    }

    // Recording controls (bottom-center)
    RecordingControls {
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 24
        recording: root.recording
        elapsedSeconds: root.recordingSeconds
        visible: root.mirroring
        onStartRecording: root.recordToggled()
        onStopRecording: root.recordToggled()
    }

    // Toolbar (bottom-right)
    Row {
        anchors.bottom: parent.bottom; anchors.right: parent.right
        anchors.margins: 16
        spacing: 8
        visible: root.mirroring

        Repeater {
            model: [
                { icon: "⛶", tip: "Fullscreen", action: function() { root.fullscreenToggled() } },
                { icon: "📷", tip: "Screenshot", action: function() { root.screenshotRequested() } },
                { icon: "✕",  tip: "Stop",       action: function() { root.stopRequested() } }
            ]
            Rectangle {
                width: 36; height: 36; radius: 8
                color: "#cc313244"
                Text { anchors.centerIn: parent; text: modelData.icon; font.pixelSize: 16; color: "#cdd6f4" }
                MouseArea { anchors.fill: parent; onClicked: modelData.action(); cursorShape: Qt.PointingHandCursor }
                ToolTip.visible: hovered; ToolTip.text: modelData.tip; ToolTip.delay: 600
            }
        }
    }
}
