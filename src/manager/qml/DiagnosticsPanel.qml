// DiagnosticsPanel.qml — Live diagnostic data
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 22

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                
                Text {
                    text: "Diagnostics"
                    font.family: "Inter"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    color: "#e8eef7"
                }
                Text {
                    text: "System logs and performance diagnostics"
                    font.family: "Inter"
                    font.pixelSize: 13
                    color: "#9cacbf"
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Rectangle {
                width: exportBtn.width + 20
                height: 36
                radius: 6
                color: "#223044"
                border.width: 0
                
                Text {
                    id: exportBtn
                    anchors.centerIn: parent
                    text: "Export Logs"
                    font.family: "Inter"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: "#e8eef7"
                }
                
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: if (hubModel) hubModel.exportLogs()
                    onEntered: parent.color = "#263243"
                    onExited: parent.color = "#223044"
                }
            }
        }

        // Stats grid
        GridLayout {
            columns: 2
            Layout.fillWidth: true
            columnSpacing: 12
            rowSpacing: 12

            Repeater {
                model: [
                    { label: "Packet Loss",     value: statsModel ? statsModel.packetLossPct.toFixed(2) + "%" : "—" },
                    { label: "Jitter",          value: statsModel ? statsModel.jitterMs.toFixed(1) + " ms" : "—" },
                    { label: "Reorder Cache",   value: statsModel ? statsModel.reorderQueueDepth + " pkts" : "—" },
                    { label: "FEC Recoveries",  value: statsModel ? statsModel.fecRecoveries + "" : "—" },
                    { label: "Decode Time",     value: statsModel ? statsModel.decodeTimeMs.toFixed(1) + " ms" : "—" },
                    { label: "GPU Frame Time",  value: statsModel ? statsModel.gpuFrameTimeMs.toFixed(1) + " ms" : "—" }
                ]
                delegate: Rectangle {
                    Layout.fillWidth: true
                    height: 80
                    radius: 12
                    color: "#1c2531"
                    border.color: "#3b4a5f"
                    border.width: 1
                    
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        
                        Text {
                            text: modelData.value
                            font.family: "Inter"
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            color: "#e8eef7"
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: modelData.label
                            font.family: "Inter"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: "#9cacbf"
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }

        // Log viewer
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: "#0e131a"
            border.color: "#3b4a5f"
            border.width: 1

            ScrollView {
                anchors.fill: parent
                anchors.margins: 16
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                TextArea {
                    readOnly: true
                    text: hubModel ? hubModel.recentLogLines : "No logs available."
                    font.family: "Consolas"
                    font.pixelSize: 11
                    color: "#9cacbf"
                    background: null
                    wrapMode: TextEdit.NoWrap
                }
            }
        }
    }
}
