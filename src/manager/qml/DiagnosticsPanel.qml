// DiagnosticsPanel.qml — Live diagnostic data (neo-brutalist)
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Item {
    ColumnLayout {
        anchors.fill: parent; anchors.margins: Theme.spacingLG
        spacing: Theme.spacingLG

        // Header
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("DIAGNOSTICS")
                font.family:    Theme.fontMono; font.pixelSize: Theme.fontSizeH2
                font.weight:    Font.Black; color: Theme.textPrimary; letterSpacing: 4
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 120; height: 36; color: qsTr("transparent")
                border.color: Theme.borderNormal; border.width: 2
                Text {
                    anchors.centerIn: parent; text: qsTr("EXPORT LOGS")
                    font.family:    Theme.fontMono; font.pixelSize: 11
                    font.weight:    Font.Bold; color: Theme.textSecondary; letterSpacing: 1
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: if (hubModel) hubModel.exportLogs()
                }
            }
        }
        Rectangle { height: 2; Layout.fillWidth: true; color: Theme.borderActive }

        // Stats grid
        GridLayout {
            columns: 2; Layout.fillWidth: true
            columnSpacing: Theme.spacingMD; rowSpacing: Theme.spacingMD

            Repeater {
                model: [
                    { label: qsTr("PACKET LOSS"),     value: statsModel ? statsModel.packetLossPct.toFixed(2) + "%" : "—" },
                    { label: qsTr("JITTER"),          value: statsModel ? statsModel.jitterMs.toFixed(1) + " ms"           : "—" },
                    { label: qsTr("REORDER CACHE"),   value: statsModel ? statsModel.reorderQueueDepth + " pkts"           : "—" },
                    { label: qsTr("FEC RECOVERIES"),  value: statsModel ? statsModel.fecRecoveries + ""                    : "—" },
                    { label: qsTr("DECODE TIME"),     value: statsModel ? statsModel.decodeTimeMs.toFixed(1) + " ms"       : "—" },
                    { label: qsTr("GPU FRAME TIME"),  value: statsModel ? statsModel.gpuFrameTimeMs.toFixed(1) + " ms"     : "—" },
                ]
                delegate: Rectangle {
                    Layout.fillWidth: true; height: 60
                    color: Theme.bgCard; border.color: Theme.borderNormal; border.width: 2
                    Rectangle { x: 3; y: 3; width: parent.width; height: parent.height; color: Theme.bgHover; z: -1 }
                    ColumnLayout {
                        anchors.centerIn: parent; spacing: 3
                        Text {
                            text: modelData.value
                            font.family:    Theme.fontMono; font.pixelSize: Theme.fontSizeLG
                            font.weight:    Font.Black; color: Theme.textPrimary
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: modelData.label
                            font.family:    Theme.fontMono; font.pixelSize: Theme.fontSizeXS
                            font.weight:    Font.Bold; color: Theme.textSecondary; letterSpacing: 2
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }

        // Log viewer
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: Theme.bgBase; border.color: Theme.borderNormal; border.width: 2

            ScrollView {
                anchors.fill: parent; anchors.margins: 8; clip: true
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                TextArea {
                    readOnly: true
                    text: hubModel ? hubModel.recentLogLines : qsTr("No logs available.")
                    font.family:    Theme.fontMono; font.pixelSize: 11
                    color:          Theme.textSecondary
                    background: null
                    wrapMode: TextEdit.NoWrap
                }
            }
        }
    }
}
