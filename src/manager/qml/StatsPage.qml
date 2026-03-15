// =============================================================================
// StatsPage.qml — Network statistics and performance graphs
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import AuraCastPro 1.0

Item {
    id: root

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
                    text: "Statistics"
                    font.family: "Inter"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    color: "#e8eef7"
                }
                Text {
                    text: "Real-time performance metrics and network analysis"
                    font.family: "Inter"
                    font.pixelSize: 13
                    color: "#9cacbf"
                }
            }
            
            Item { Layout.fillWidth: true }
            
            // Stats Overlay Toggle
            Rectangle {
                width: overlayContent.width + 20
                height: 36
                radius: 6
                color: statsOverlayEnabled ? "#7bb3ff" : "#223044"
                
                property bool statsOverlayEnabled: false
                
                RowLayout {
                    id: overlayContent
                    anchors.centerIn: parent
                    spacing: 8
                    
                    Text {
                        text: "📊"
                        font.pixelSize: 13
                    }
                    
                    Text {
                        text: "Stats Overlay"
                        font.family: "Inter"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: parent.parent.statsOverlayEnabled ? "white" : "#e8eef7"
                    }
                }
                
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: {
                        parent.statsOverlayEnabled = !parent.statsOverlayEnabled
                    }
                    onEntered: {
                        if (!parent.statsOverlayEnabled) {
                            parent.color = "#263243"
                        }
                    }
                    onExited: {
                        if (!parent.statsOverlayEnabled) {
                            parent.color = "#223044"
                        }
                    }
                }
            }
        }

        // Stats cards
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Repeater {
                model: [
                    { label: "Bitrate",     value: statsModel ? (statsModel.bitrateKbps / 1000).toFixed(1) + " Mbps" : "—" },
                    { label: "Latency",     value: statsModel ? statsModel.latencyMs.toFixed(0) + " ms" : "—" },
                    { label: "FPS",         value: statsModel ? statsModel.fps.toFixed(1) : "—" },
                    { label: "Packet Loss", value: statsModel ? statsModel.packetLossPct.toFixed(1) + "%" : "—" }
                ]
                delegate: StatCard {
                    Layout.fillWidth: true
                    label: modelData.label
                    value: modelData.value
                }
            }
        }

        // Graphs
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14
            
            BitrateGraph { 
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 200
            }
            LatencyGraph {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 200
            }
        }
    }

    component StatCard: Rectangle {
        id: card
        property string label: ""
        property string value: "\u2014"
        height: 88
        radius: 12
        color: "#1c2531"
        border.color: "#3b4a5f"
        border.width: 1
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6
            Text {
                text: card.label
                font.family: "Inter"
                font.pixelSize: 11
                font.weight: Font.Medium
                color: "#9cacbf"
                opacity: 0.8
            }
            Item { Layout.fillHeight: true }
            Text {
                text: card.value
                font.family: "Inter"
                font.pixelSize: 24
                font.weight: Font.Bold
                color: "#e8eef7"
            }
        }
    }
}
