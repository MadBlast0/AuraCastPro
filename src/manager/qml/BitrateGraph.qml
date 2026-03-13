import QtQuick 2.15
import "."

Rectangle {
    id: root
    radius: Theme.radiusLG
    color: Theme.bgCard
    border.color: Theme.borderNormal
    border.width: 1

    Text {
        id: headerLabel
        x: 16
        y: 14
        text: qsTr("Bitrate")
        font.family: Theme.fontSans
        font.pixelSize: Theme.fontSizeSM
        font.weight: Font.Medium
        color: Theme.textSecondary
    }

    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        text: statsModel ? Theme.formatBitrate(statsModel.bitrateKbps) : "--"
        font.family: Theme.fontSans
        font.pixelSize: Theme.fontSizeMD
        font.weight: Font.DemiBold
        color: statsModel ? Theme.bitrateColour(statsModel.bitrateKbps) : Theme.textDisabled
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

            var history = statsModel.bitrateHistory
            if (!history || history.length === 0) return

            var n = history.length
            var maxV = 5000
            for (var i = 0; i < n; i++)
                if (history[i] > maxV) maxV = history[i] * 1.2

            var yScale = (height - 8) / maxV
            var xStep = width / Math.max(n - 1, 1)

            ctx.strokeStyle = "rgba(255,255,255,0.07)"
            ctx.lineWidth = 1
            for (var g = 1; g <= 3; g++) {
                var gy = (height / 4) * g
                ctx.beginPath()
                ctx.moveTo(0, gy)
                ctx.lineTo(width, gy)
                ctx.stroke()
            }

            var grad = ctx.createLinearGradient(0, 0, 0, height)
            grad.addColorStop(0, "rgba(123,179,255,0.28)")
            grad.addColorStop(1, "rgba(123,179,255,0.02)")
            ctx.fillStyle = grad
            ctx.beginPath()
            ctx.moveTo(0, height)
            for (var j = 0; j < n; j++)
                ctx.lineTo(j * xStep, height - history[j] * yScale)
            ctx.lineTo((n - 1) * xStep, height)
            ctx.closePath()
            ctx.fill()

            var cur = history[n - 1]
            ctx.strokeStyle = cur >= 10000 ? Theme.accentGreen
                            : cur >= 3000 ? Theme.accentYellow
                            : Theme.accentRed
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
