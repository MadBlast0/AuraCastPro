// BitrateGraph.qml — Real-time bitrate history graph (neo-brutalist)
// Task 184: Canvas-based 30-second scrolling bitrate graph.
// Binds to NetworkStatsModel.bitrateHistory (QList<float> in kbps, 300 samples).
import QtQuick 2.15
import "."

Rectangle {
    id: root
    color:        Theme.bgCard
    border.color: Theme.borderNormal
    border.width: Theme.borderWidthNormal

    // Hard shadow (brutalist)
    Rectangle {
        x: Theme.shadowOffX; y: Theme.shadowOffY
        width: parent.width; height: parent.height
        color: Theme.bgHover; z: -1
    }

    // Header label
    Text {
        id: headerLabel
        x: 12; y: 8
        text: qsTr("BITRATE")
        font.family:    Theme.fontMono
        font.pixelSize: Theme.fontSizeSM
        font.weight:    Font.Bold
        color:          Theme.textSecondary
        letterSpacing:  2
    }

    // Current value (top-right)
    Text {
        anchors.right:   parent.right
        anchors.top:     parent.top
        anchors.margins: 10
        text: statsModel ? Theme.formatBitrate(statsModel.bitrateKbps) : "—"
        font.family:    Theme.fontMono
        font.pixelSize: Theme.fontSizeMD
        font.weight:    Font.Black
        color: statsModel ? Theme.bitrateColour(statsModel.bitrateKbps)
                          : Theme.textDisabled
        letterSpacing: 1
    }

    // Canvas graph — scrolling 30-second bitrateHistory
    Canvas {
        id: canvas
        anchors {
            top:    headerLabel.bottom
            bottom: parent.bottom
            left:   parent.left
            right:  parent.right
            topMargin: 4; bottomMargin: 8
            leftMargin: 8; rightMargin: 8
        }

        // Redraw whenever stats update
        Connections {
            target: statsModel
            function onStatsUpdated() { canvas.requestPaint() }
        }

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            if (!statsModel) return

            // bitrateHistory = QList<float> kbps, up to 300 samples (30s at 10Hz)
            var history = statsModel.bitrateHistory
            if (!history || history.length === 0) return

            var n    = history.length
            var maxV = 5000  // minimum Y-axis: 5 Mbps
            for (var i = 0; i < n; i++)
                if (history[i] > maxV) maxV = history[i] * 1.2

            var yScale = (height - 4) / maxV
            var xStep  = width / Math.max(n - 1, 1)

            // Gradient fill
            var grad = ctx.createLinearGradient(0, 0, 0, height)
            grad.addColorStop(0, "rgba(255,255,255,0.15)")
            grad.addColorStop(1, "rgba(255,255,255,0.01)")
            ctx.fillStyle = grad
            ctx.beginPath()
            ctx.moveTo(0, height)
            for (var j = 0; j < n; j++)
                ctx.lineTo(j * xStep, height - history[j] * yScale)
            ctx.lineTo((n-1) * xStep, height)
            ctx.closePath()
            ctx.fill()

            // Line — colour based on current bitrate
            var cur   = history[n - 1]
            ctx.strokeStyle = cur >= 10000 ? Theme.accentGreen
                            : cur >=  3000 ? Theme.accentYellow
                            : Theme.accentRed
            ctx.lineWidth = 2
            ctx.lineJoin  = "round"
            ctx.beginPath()
            ctx.moveTo(0, height - history[0] * yScale)
            for (var k = 1; k < n; k++) {
                var mx = (k - 0.5) * xStep
                var y0 = height - history[k-1] * yScale
                var y1 = height - history[k]   * yScale
                ctx.bezierCurveTo(mx, y0, mx, y1, k * xStep, y1)
            }
            ctx.stroke()

            // Dashed reference at 10 Mbps
            if (10000 < maxV) {
                ctx.setLineDash([4, 4])
                ctx.strokeStyle = "rgba(255,255,255,0.15)"
                ctx.lineWidth   = 1
                ctx.beginPath()
                var ry = height - 10000 * yScale
                ctx.moveTo(0, ry); ctx.lineTo(width, ry)
                ctx.stroke()
                ctx.setLineDash([])
            }
        }
    }
}
