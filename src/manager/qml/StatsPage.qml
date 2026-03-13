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
        ColumnLayout {
            spacing: 5
            
            RowLayout {
                Layout.fillWidth: true
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    
                    Text {
                        text: "Network Statistics"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeH2
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }
                    Text {
                        text: "Real-time performance metrics and network analysis"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSizeSM
                        color: Theme.textSecondary
                    }
                }
                
                // Stats Overlay Toggle - macOS style
                Rectangle {
                    width: overlayContent.width + 16
                    height: 28
                    radius: 6
                    color: statsOverlayEnabled ? Theme.accent : Theme.bgElevated
                    
                    property bool statsOverlayEnabled: false
                    
                    RowLayout {
                        id: overlayContent
                        anchors.centerIn: parent
                        spacing: 6
                        
                        Text {
                            text: "📊"
                            font.pixelSize: 13
                            color: parent.parent.statsOverlayEnabled ? "white" : Theme.textPrimary
                        }
                        
                        Text {
                            text: "Stats Overlay"
                            font.family: Theme.fontSans
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: parent.parent.statsOverlayEnabled ? "white" : Theme.textPrimary
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
                                parent.color = Theme.bgHover
                            }
                        }
                        onExited: {
                            if (!parent.statsOverlayEnabled) {
                                parent.color = Theme.bgElevated
                            }
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
                    { label: "Bitrate",     value: statsModel ? Theme.formatBitrate(statsModel.bitrateKbps) : "—" },
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
        color: Theme.bgCard
        border.color: Theme.borderNormal
        border.width: 1
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6
            Text {
                text: card.label
                font.family: Theme.fontSans
                font.pixelSize: 11
                font.weight: Font.Medium
                color: Theme.textSecondary
                opacity: 0.8
            }
            Item { Layout.fillHeight: true }
            Text {
                text: card.value
                font.family: Theme.fontSans
                font.pixelSize: 24
                font.weight: Font.Bold
                color: Theme.textPrimary
            }
        }
    }
}
