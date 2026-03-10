// TroubleshootingPage.qml — Self-service troubleshooting guide
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    signal exportLogsRequested()
    signal runDiagnosticsRequested()
    signal openSettingsRequested()

    property var issues: [
        {
            title: "iPhone/iPad not appearing in device list",
            steps: [
                "Ensure your PC and iPhone are on the same Wi-Fi network",
                "On iPhone: Settings → Wi-Fi → tap your network → ensure no VPN is active",
                "Restart AuraCastPro to re-advertise mDNS services",
                "Check Windows Firewall allows AuraCastPro on private networks"
            ]
        },
        {
            title: "Android device not connecting via USB",
            steps: [
                "Enable Developer Options: Settings → About Phone → tap Build Number 7 times",
                "Enable USB Debugging in Developer Options",
                "Accept the RSA key fingerprint dialog on your Android device",
                "Try a different USB cable or port"
            ]
        },
        {
            title: "High latency / lag",
            steps: [
                "Switch to 5 GHz Wi-Fi band if available",
                "Reduce mirroring resolution in Settings → Quality",
                "Close other apps using network bandwidth",
                "For wired Android: use USB 3.0 port"
            ]
        },
        {
            title: "Black screen / no video",
            steps: [
                "Check GPU drivers are up to date",
                "Try toggling fullscreen mode (F11)",
                "Disable HDR in Settings if your monitor doesn't support it",
                "Check that no DRM-protected content is playing on the device"
            ]
        },
        {
            title: "No audio",
            steps: [
                "Check Settings → Audio → Output Device is correct",
                "Ensure the virtual audio driver is installed (run installer as admin)",
                "Check Windows Sound settings — AuraCastPro Virtual Audio should appear"
            ]
        }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Text {
            text: "Troubleshooting"
            color: "#cdd6f4"; font.pixelSize: 20; font.bold: true
        }

        RowLayout {
            spacing: 12
            Button {
                text: "Run Diagnostics"
                onClicked: root.runDiagnosticsRequested()
                background: Rectangle { color: "#89b4fa"; radius: 6 }
                contentItem: Text { text: parent.text; color: "#1e1e2e"; font.bold: true; horizontalAlignment: Text.AlignHCenter }
            }
            Button {
                text: "Export Logs"
                onClicked: root.exportLogsRequested()
                background: Rectangle { color: "#313244"; radius: 6 }
                contentItem: Text { text: parent.text; color: "#cdd6f4"; horizontalAlignment: Text.AlignHCenter }
            }
        }

        Rectangle { height: 1; Layout.fillWidth: true; color: "#313244" }

        Text {
            text: "Common Issues"
            color: "#a6adc8"; font.pixelSize: 14; font.bold: true
        }

        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: 8; clip: true
            model: root.issues

            delegate: Rectangle {
                width: ListView.view.width
                height: expanded ? titleRow.height + stepsList.implicitHeight + 24 : 48
                color: "#1e1e2e"; radius: 8
                border.color: expanded ? "#89b4fa" : "#313244"; border.width: 1
                clip: true

                property bool expanded: false

                Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                RowLayout {
                    id: titleRow
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top
                    height: 48; anchors.margins: 14

                    Text {
                        text: modelData.title
                        color: "#cdd6f4"; font.pixelSize: 13; font.bold: true
                        Layout.fillWidth: true; elide: Text.ElideRight
                    }
                    Text { text: expanded ? "▲" : "▼"; color: "#6c7086"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; onClicked: parent.parent.parent.expanded = !parent.parent.parent.expanded; cursorShape: Qt.PointingHandCursor }
                }

                ColumnLayout {
                    id: stepsList
                    anchors.top: titleRow.bottom; anchors.left: parent.left
                    anchors.right: parent.right; anchors.margins: 14
                    spacing: 6; visible: parent.expanded

                    Repeater {
                        model: modelData.steps
                        RowLayout {
                            Layout.fillWidth: true; spacing: 8
                            Text { text: "•"; color: "#89b4fa"; font.pixelSize: 13 }
                            Text {
                                text: modelData
                                color: "#a6adc8"; font.pixelSize: 12
                                Layout.fillWidth: true; wrapMode: Text.WordWrap
                            }
                        }
                    }
                    Item { height: 4 }
                }
            }
        }
    }
}
