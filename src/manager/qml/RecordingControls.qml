// RecordingControls.qml — Floating recording toolbar overlay
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    width: controlRow.implicitWidth + 32
    height: 48
    radius: 24
    color: "#cc1e1e2e"
    border.color: "#3a3a5c"
    border.width: 1

    property bool recording: false
    property bool paused: false
    property int elapsedSeconds: 0
    property string outputPath: ""

    signal startRecording()
    signal stopRecording()
    signal pauseRecording()
    signal resumeRecording()

    // Elapsed time formatter
    function formatTime(secs) {
        var h = Math.floor(secs / 3600)
        var m = Math.floor((secs % 3600) / 60)
        var s = secs % 60
        if (h > 0) return pad(h) + ":" + pad(m) + ":" + pad(s)
        return pad(m) + ":" + pad(s)
    }
    function pad(n) { return n < 10 ? "0" + n : "" + n }

    RowLayout {
        id: controlRow
        anchors.centerIn: parent
        spacing: 8

        // Record/Stop button
        Rectangle {
            width: 32; height: 32; radius: 16
            color: root.recording ? "#f38ba8" : "#a6e3a1"
            Text {
                anchors.centerIn: parent
                text: root.recording ? "■" : "●"
                color: "#1e1e2e"
                font.pixelSize: 14
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.recording ? root.stopRecording() : root.startRecording()
                cursorShape: Qt.PointingHandCursor
            }
        }

        // Pause/Resume (visible while recording)
        Rectangle {
            width: 32; height: 32; radius: 16
            visible: root.recording
            color: "#313244"
            Text {
                anchors.centerIn: parent
                text: root.paused ? "▶" : "⏸"
                color: "#cdd6f4"
                font.pixelSize: 12
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.paused ? root.resumeRecording() : root.pauseRecording()
                cursorShape: Qt.PointingHandCursor
            }
        }

        // Elapsed time
        Text {
            visible: root.recording
            text: root.formatTime(root.elapsedSeconds)
            color: root.paused ? "#fab387" : "#a6e3a1"
            font.pixelSize: 13
            font.family: "Consolas, monospace"
            font.bold: true
        }

        // REC indicator dot
        Rectangle {
            width: 8; height: 8; radius: 4
            visible: root.recording && !root.paused
            color: "#f38ba8"
            SequentialAnimation on opacity {
                running: root.recording && !root.paused
                loops: Animation.Infinite
                NumberAnimation { to: 0.2; duration: 500 }
                NumberAnimation { to: 1.0; duration: 500 }
            }
        }
    }
}
