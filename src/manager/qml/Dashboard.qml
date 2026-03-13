import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingLG

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMD

            ColumnLayout {
                spacing: 4

                Text {
                    text: qsTr("Dashboard")
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSizeH2
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Text {
                    text: qsTr("Keep an eye on connection health before you start mirroring.")
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSizeSM
                    color: Theme.textSecondary
                }
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                width: 154
                height: 46
                radius: Theme.radiusMD
                color: hubModel && hubModel.isMirroring ? Theme.bgElevated : Theme.accent
                border.color: hubModel && hubModel.isMirroring ? Theme.borderNormal : "#9bc5ff"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: hubModel && hubModel.isMirroring ? qsTr("Stop Mirroring") : qsTr("Start Mirroring")
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSizeSM
                    font.weight: Font.DemiBold
                    color: hubModel && hubModel.isMirroring ? Theme.textPrimary : Theme.textInverse
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: hubModel && hubModel.isMirroring
                        ? hubModel.stopMirroring()
                        : hubModel.startMirroring()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMD

            Repeater {
                model: [
                    { key: qsTr("Bitrate"), value: statsModel ? Theme.formatBitrate(statsModel.bitrateKbps) : "--" },
                    { key: qsTr("Latency"), value: statsModel ? statsModel.latencyMs.toFixed(0) + " ms" : "--" },
                    { key: qsTr("FPS"), value: statsModel ? statsModel.fps.toFixed(1) : "--" },
                    { key: qsTr("Loss"), value: statsModel ? statsModel.packetLossPct.toFixed(1) + "%" : "--" }
                ]

                delegate: StatTile {
                    Layout.fillWidth: true
                    label: modelData.key
                    value: modelData.value
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMD

            BitrateGraph { Layout.fillWidth: true; Layout.fillHeight: true }
            LatencyGraph { Layout.fillWidth: true; Layout.fillHeight: true }
        }
    }

    component StatTile: Rectangle {
        id: tile
        property string label: ""
        property string value: "--"

        height: 98
        radius: Theme.radiusMD
        color: Theme.bgCard
        border.color: Theme.borderNormal
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6

            Text {
                text: tile.label
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSizeXS
                font.weight: Font.Medium
                color: Theme.textSecondary
            }

            Item { Layout.fillHeight: true }

            Text {
                text: tile.value
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSizeH2
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
        }
    }
}
