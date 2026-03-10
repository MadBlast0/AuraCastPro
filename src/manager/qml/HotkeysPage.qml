// HotkeysPage.qml — Keyboard shortcut configuration page
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    property var hotkeys: [
        { action: "Start/Stop Mirroring", keys: "Ctrl+Shift+M", editable: true },
        { action: "Start/Stop Recording", keys: "Ctrl+Shift+R", editable: true },
        { action: "Toggle Fullscreen",    keys: "F11",           editable: true },
        { action: "Screenshot",           keys: "Ctrl+Shift+S",  editable: true },
        { action: "Toggle Stats Overlay", keys: "Ctrl+Shift+I",  editable: true },
        { action: "Disconnect Device",    keys: "Ctrl+D",        editable: true },
        { action: "Show/Hide Window",     keys: "Ctrl+Shift+W",  editable: true }
    ]

    signal hotkeySaved(string action, string keys)
    signal hotkeyReset(string action)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Text {
            text: "Keyboard Shortcuts"
            color: "#cdd6f4"; font.pixelSize: 20; font.bold: true
        }

        Text {
            text: "Click a shortcut to reassign it. Press Escape to cancel."
            color: "#6c7086"; font.pixelSize: 13
        }

        Rectangle { height: 1; Layout.fillWidth: true; color: "#313244" }

        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: 4
            clip: true
            model: root.hotkeys

            delegate: Rectangle {
                width: ListView.view.width; height: 48
                color: index % 2 === 0 ? "#181825" : "#1e1e2e"
                radius: 6

                RowLayout {
                    anchors.fill: parent; anchors.margins: 12
                    spacing: 12

                    Text {
                        text: modelData.action
                        color: "#cdd6f4"; font.pixelSize: 13
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        width: 140; height: 32; radius: 6
                        color: keyArea.containsMouse ? "#313244" : "#252535"
                        border.color: "#45475a"; border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData.keys
                            color: "#89b4fa"
                            font.pixelSize: 12
                            font.family: "Consolas, monospace"
                            font.bold: true
                        }
                        MouseArea {
                            id: keyArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                        }
                    }

                    Text {
                        text: "Reset"
                        color: "#6c7086"; font.pixelSize: 12
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.hotkeyReset(modelData.action)
                        }
                    }
                }
            }
        }

        Text {
            text: "⚠  Changes take effect immediately"
            color: "#fab387"; font.pixelSize: 12
        }
    }
}
