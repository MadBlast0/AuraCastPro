// =============================================================================
// StatsOverlay.qml — In-mirror-window performance stats overlay (Ctrl+Shift+O)
// =============================================================================
import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: overlay
    width: 220; height: col.implicitHeight + 16
    color: "#CC000000"
    radius: 8
    border.color: "#30363D"
    visible: performanceOverlay ? performanceOverlay.visible : false

    readonly property color green:  "#3FB950"
    readonly property color yellow: "#E3B341"
    readonly property color red:    "#F85149"
    readonly property color text:   "#E6EDF3"
    readonly property color muted:  "#8B949E"

    function latencyColor(ms) {
        return ms < 50 ? green : ms < 100 ? yellow : red
    }
    function lossColor(pct) {
        return pct < 0.5 ? green : pct < 2.0 ? yellow : red
    }

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
        spacing: 4

        Text { text: qsTr("AuraCastPro Stats"); color: muted; font.pixelSize: 10; font.uppercase: true; font.letterSpacing: 1 }

        StatRow { label: qsTr("Latency");      value: (statsModel ? statsModel.latencyMs.toFixed(1) : "—") + " ms";   valueColor: statsModel ? latencyColor(statsModel.latencyMs) : text }
        StatRow { label: qsTr("Bitrate");      value: (statsModel ? (statsModel.bitrateKbps/1000).toFixed(1) : "—") + " Mbps"; valueColor: text }
        StatRow { label: qsTr("FPS");          value: statsModel ? statsModel.fps.toFixed(1) : "—";                    valueColor: statsModel && statsModel.fps > 55 ? green : yellow }
        StatRow { label: qsTr("Packet Loss");  value: (statsModel ? statsModel.packetLossPct.toFixed(2) : "—") + "%";  valueColor: statsModel ? lossColor(statsModel.packetLossPct) : text }
        StatRow { label: qsTr("GPU Frame");    value: (statsModel ? statsModel.gpuFrameMs.toFixed(1) : "—") + " ms";   valueColor: statsModel && statsModel.gpuFrameMs < 8 ? green : yellow }
        StatRow { label: qsTr("CPU");          value: performanceOverlay ? performanceOverlay.cpuUsagePct.toFixed(1) + "%" : "—"; valueColor: performanceOverlay && performanceOverlay.cpuUsagePct < 50 ? green : yellow }
        StatRow { label: qsTr("Memory");       value: performanceOverlay ? performanceOverlay.workingSetMB.toFixed(0) + " MB" : "—"; valueColor: text }

        // Mini bitrate history sparkline
        Canvas {
            id: sparkline
            Layout.fillWidth: true; height: 32
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                if (!statsModel || !statsModel.bitrateHistory || statsModel.bitrateHistory.length < 2) return
                const history = statsModel.bitrateHistory
                const maxVal  = Math.max(...history, 1)
                ctx.beginPath()
                ctx.strokeStyle = "#58A6FF"
                ctx.lineWidth = 1.5
                for (let i = 0; i < history.length; i++) {
                    const x = i / (history.length - 1) * width
                    const y = height - (history[i] / maxVal) * height
                    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y)
                }
                ctx.stroke()
            }
            Connections {
                target: statsModel
                function onStatsUpdated() { sparkline.requestPaint() }
            }
        }
    }

    component StatRow: RowLayout {
        property string label: ""
        property string value: ""
        property color  valueColor: "#E6EDF3"
        Layout.fillWidth: true; height: 16
        Text { text: label; color: muted; font.pixelSize: 11; Layout.fillWidth: true }
        Text { text: value; color: valueColor; font.pixelSize: 11; font.bold: true }
    }
}
