import QtQuick 2.15
import QtCharts 2.15
import "."

Rectangle {
    id: root
    radius: Theme.radiusLG
    color: Theme.bgCard
    border.color: Theme.borderNormal
    border.width: 1

    Text {
        x: 16
        y: 14
        text: qsTr("Latency")
        font.family: Theme.fontSans
        font.pixelSize: Theme.fontSizeSM
        font.weight: Font.Medium
        color: Theme.textSecondary
        z: 2
    }

    ChartView {
        anchors.fill: parent
        anchors.margins: 6
        backgroundColor: Theme.bgCard
        antialiasing: true
        legend.visible: false
        margins { left: 0; right: 0; top: 0; bottom: 0 }

        ValueAxis {
            id: axisY
            min: 0
            max: 150
            labelFormat: "%d ms"
            color: Theme.borderSubtle
            labelsColor: Theme.textDisabled
            labelsFont.family: Theme.fontSans
            labelsFont.pixelSize: 10
            gridLineColor: "#223042"
            tickCount: 4
        }

        ValueAxis {
            id: axisX
            min: 0
            max: 60
            labelsVisible: false
            gridLineColor: "#00000000"
        }

        LineSeries {
            id: series
            axisX: axisX
            axisY: axisY
            color: Theme.accent
            width: 2.5
            useOpenGL: true
        }
    }

    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: {
            if (!statsModel) return
            if (series.count >= 60) series.remove(0)
            series.append(series.count, statsModel.latencyMs)
        }
    }
}
