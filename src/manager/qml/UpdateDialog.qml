// UpdateDialog.qml — Software update notification dialog
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Popup {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 420
    height: contentCol.implicitHeight + 48
    padding: 24

    property string currentVersion: "1.0.0"
    property string newVersion: "1.0.0"
    property string releaseNotes: ""
    property bool downloading: false
    property real downloadProgress: 0.0

    signal installRequested()
    signal dismissed()

    background: Rectangle {
        color: "#1e1e2e"
        radius: 12
        border.color: "#3a3a5c"
        border.width: 1
    }

    ColumnLayout {
        id: contentCol
        anchors.fill: parent
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "⬆  Update Available"
                color: "#cdd6f4"
                font.pixelSize: 18
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                text: root.newVersion
                color: "#a6e3a1"
                font.pixelSize: 14
                font.bold: true
            }
        }

        Text {
            text: "Version " + root.currentVersion + " → " + root.newVersion
            color: "#bac2de"
            font.pixelSize: 13
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 120
            color: "#181825"
            radius: 6
            clip: true
            Flickable {
                anchors.fill: parent
                anchors.margins: 8
                contentHeight: notesText.implicitHeight
                clip: true
                Text {
                    id: notesText
                    text: root.releaseNotes || "No release notes available."
                    color: "#a6adc8"
                    font.pixelSize: 12
                    width: parent.width
                    wrapMode: Text.WordWrap
                }
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            value: root.downloadProgress
            visible: root.downloading
            background: Rectangle { color: "#313244"; radius: 3 }
            contentItem: Rectangle { width: parent.width * parent.value; color: "#89b4fa"; radius: 3 }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                text: "Later"
                Layout.fillWidth: true
                onClicked: { root.dismissed(); root.close() }
                background: Rectangle { color: "#313244"; radius: 6 }
                contentItem: Text { text: parent.text; color: "#cdd6f4"; horizontalAlignment: Text.AlignHCenter }
            }
            Button {
                text: root.downloading ? "Downloading…" : "Install Update"
                enabled: !root.downloading
                Layout.fillWidth: true
                onClicked: root.installRequested()
                background: Rectangle { color: enabled ? "#89b4fa" : "#45475a"; radius: 6 }
                contentItem: Text { text: parent.text; color: "#1e1e2e"; horizontalAlignment: Text.AlignHCenter; font.bold: true }
            }
        }
    }
}
