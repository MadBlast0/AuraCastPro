// LatencyGraph.qml — Real-time latency chart (neo-brutalist)
import QtQuick 2.15
import QtCharts 2.15
import "."

Rectangle {
    id: root
    color:  Theme.bgCard
    border.color: Theme.borderNormal
    border.width: Theme.borderWidthNormal

    Rectangle {
        x: Theme.shadowOffX; y: Theme.shadowOffY
        width: parent.width; height: parent.height
        color: Theme.bgHover; z: -1
    }

    ChartView {
        anchors.fill: parent
        anchors.margins: 1
        backgroundColor: Theme.bgCard
        antialiasing: true
        legend.visible: false
        margins { left: 0; right: 0; top: 0; bottom: 0 }

        Text {
            parent: root
            x: 12; y: 8
            text: qsTr("LATENCY")
            font.family:    Theme.fontMono
            font.pixelSize: Theme.fontSizeSM
            font.weight:    Font.Bold
            color:          Theme.textSecondary
            letterSpacing:  2
        }

        ValueAxis {
            id: axisY
            min: 0; max: 150
            labelFormat: "%d ms"
            color: Theme.borderSubtle
            labelsColor: Theme.textDisabled
            labelsFont.family:    Theme.fontMono
            labelsFont.pixelSize: 9
            gridLineColor: Theme.borderSubtle
            tickCount: 4
        }

        ValueAxis {
            id: axisX
            min: 0; max: 60
            labelsVisible: false
            gridLineColor: qsTr("transparent")
        }

        LineSeries {
            id: series
            axisX: axisX; axisY: axisY
            color: Theme.textPrimary
            width: 2; useOpenGL: true
        }
    }

    Timer {
        interval: 500; running: true; repeat: true
        onTriggered: {
            if (!statsModel) return
            if (series.count >= 60) series.remove(0)
            series.append(series.count, statsModel.latencyMs)
        }
    }
}
