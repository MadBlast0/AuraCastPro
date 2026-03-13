import QtQuick 2.15
import "."

Rectangle {
    id: root
    radius: 12
    color: "#1c2531"
    border.color: "#3b4a5f"
    border.width: 1

    Text {
        id: headerLabel
        x: 16
        y: 14
        text: qsTr("Latency")
        font.family: "Inter"
        font.pixelSize: 12
        font.weight: Font.Medium
        color: "#9cacbf"
    }

    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        text: statsModel ? statsModel.latencyMs.toFixed(0) + " ms" : "--"
        font.family: "Inter"
        font.pixelSize: 14
        font.weight: Font.DemiBold
        color: statsModel ? (statsModel.latencyMs <= 40 ? "#7dd7b0" : statsModel.latencyMs <= 70 ? "#e6c37a" : "#f28b82") : "#6f7d90"
    }

    Canvas {
        id: canvas
        anchors {
            top: headerLabel.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            topMargin: 8
            bottomMargin: 12
            leftMargin: 12
            rightMargin: 12
        }

        Connections {
            target: statsModel
            function onStatsUpdated() { canvas.requestPaint() }
        }

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            if (!statsModel) return

            var history = statsModel.latencyHistory
            if (!history || history.length === 0) return

            var n = history.length
            var maxV = 100
            for (var i = 0; i < n; i++)
                if (history[i] > maxV) maxV = history[i] * 1.2

            var yScale = (height - 8) / maxV
            var xStep = width / Math.max(n - 1, 1)

            // Grid lines
            ctx.strokeStyle = "rgba(255,255,255,0.05)"
            ctx.lineWidth = 1
            for (var g = 1; g <= 3; g++) {
                var gy = (height / 4) * g
                ctx.beginPath()
                ctx.moveTo(0, gy)
                ctx.lineTo(width, gy)
                ctx.stroke()
            }

            // Gradient fill
            var grad = ctx.createLinearGradient(0, 0, 0, height)
            grad.addColorStop(0, "rgba(125,215,176,0.25)")
            grad.addColorStop(1, "rgba(125,215,176,0.0)")
            ctx.fillStyle = grad
            ctx.beginPath()
            ctx.moveTo(0, height)
            for (var j = 0; j < n; j++)
                ctx.lineTo(j * xStep, height - history[j] * yScale)
            ctx.lineTo((n - 1) * xStep, height)
            ctx.closePath()
            ctx.fill()

            // Line
            var cur = history[n - 1]
            ctx.strokeStyle = cur <= 40 ? "#7dd7b0"
                            : cur <= 70 ? "#e6c37a"
                            : "#f28b82"
            ctx.lineWidth = 2.5
            ctx.lineJoin = "round"
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.moveTo(0, height - history[0] * yScale)
            for (var k = 1; k < n; k++) {
                var mx = (k - 0.5) * xStep
                var y0 = height - history[k - 1] * yScale
                var y1 = height - history[k] * yScale
                ctx.bezierCurveTo(mx, y0, mx, y1, k * xStep, y1)
            }
            ctx.stroke()
        }
    }
}
