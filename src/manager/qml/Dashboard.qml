// Dashboard.qml — Neo-brutalist main view
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLG
        spacing: Theme.spacingLG

        // ── Top bar ──────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMD

            Text {
                text: qsTr("DASHBOARD")
                font.family:    Theme.fontMono
                font.pixelSize: Theme.fontSizeH2
                font.weight:    Font.Black
                color:          Theme.textPrimary
                letterSpacing:  4
            }
            Item { Layout.fillWidth: true }

            // Big START/STOP button — brutalist pill-less rectangle
            Rectangle {
                width:  160; height: 44
                color:  hubModel && hubModel.isMirroring ? Theme.bgCard : Theme.textPrimary
                border.color: Theme.borderActive
                border.width: Theme.borderThick
                // Hard shadow
                Rectangle {
                    x: Theme.shadowOffX; y: Theme.shadowOffY
                    width: parent.width; height: parent.height
                    color: Theme.borderActive
                    z: -1
                }
                Text {
                    anchors.centerIn: parent
                    text:  hubModel && hubModel.isMirroring ? "■ STOP" : "▶ START"
                    font.family:    Theme.fontMono
                    font.pixelSize: Theme.fontSizeMD
                    font.weight:    Font.Black
                    letterSpacing:  2
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

        // ── Stats row — 4 brutalist stat tiles ───────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMD

            Repeater {
                model: [
                    { key: qsTr("BITRATE"),  value: statsModel ? Theme.formatBitrate(statsModel.bitrateKbps)   : "—", unit: "" },
                    { key: qsTr("LATENCY"),  value: statsModel ? statsModel.latencyMs.toFixed(0)               : "—", unit: qsTr("ms") },
                    { key: qsTr("FPS"),      value: statsModel ? statsModel.fps.toFixed(1)                     : "—", unit: "" },
                    { key: qsTr("LOSS"),     value: statsModel ? statsModel.packetLossPct.toFixed(1)      : "—", unit: "%" },
                ]
                delegate: StatTile {
                    Layout.fillWidth: true
                    label: modelData.key
                    value: modelData.value + modelData.unit
                }
            }
        }

        // ── Graphs row ───────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMD

            BitrateGraph { Layout.fillWidth: true; Layout.fillHeight: true }
            LatencyGraph { Layout.fillWidth: true; Layout.fillHeight: true }
        }
    }
// ── StatTile ─────────────────────────────────────────────────────────────────
component StatTile: Rectangle {
    id: root_tile
    property string label: ""
    property string value: "—"

    height: 90
    color:  Theme.bgCard
    border.color: Theme.borderNormal
    border.width: Theme.borderWidthNormal
    // Brutalist offset shadow
    Rectangle {
        x: Theme.shadowOffX; y: Theme.shadowOffY
        width: parent.width; height: parent.height
        color: Theme.borderNormal
        z: -1
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 4
        Text {
            text:  root_tile.value
            font.family:    Theme.fontMono
            font.pixelSize: Theme.fontSizeH2
            font.weight:    Font.Black
            color:          Theme.textPrimary
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            text:  root_tile.label
            font.family:    Theme.fontMono
            font.pixelSize: Theme.fontSizeXS
            font.weight:    Font.Bold
            color:          Theme.textSecondary
            letterSpacing:  2
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
}
