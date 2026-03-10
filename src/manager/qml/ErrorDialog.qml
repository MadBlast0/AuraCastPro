// ErrorDialog.qml — Error display popup with details and actions
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Popup {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 440
    height: contentCol.implicitHeight + 48
    padding: 24
    closePolicy: Popup.NoAutoClose

    property string title: "Error"
    property string message: ""
    property string details: ""
    property bool showDetails: false

    signal copyRequested(string text)
    signal acknowledged()

    background: Rectangle {
        color: "#1e1e2e"
        radius: 12
        border.color: "#f38ba8"
        border.width: 1
    }

    ColumnLayout {
        id: contentCol
        anchors.fill: parent
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Text { text: "✖"; color: "#f38ba8"; font.pixelSize: 20 }
            Text {
                text: root.title
                color: "#cdd6f4"
                font.pixelSize: 17
                font.bold: true
                Layout.fillWidth: true
                leftPadding: 6
            }
        }

        Text {
            text: root.message
            color: "#cdd6f4"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: root.showDetails ? "▾ Hide Details" : "▸ Show Details"
            color: "#89b4fa"
            font.pixelSize: 12
            visible: root.details.length > 0
            MouseArea { anchors.fill: parent; onClicked: root.showDetails = !root.showDetails }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 100
            color: "#181825"
            radius: 6
            visible: root.showDetails && root.details.length > 0
            clip: true
            Flickable {
                anchors.fill: parent; anchors.margins: 8
                contentHeight: detailsText.implicitHeight
                clip: true
                Text {
                    id: detailsText
                    text: root.details
                    color: "#f38ba8"
                    font.pixelSize: 11
                    font.family: "Consolas, monospace"
                    width: parent.width
                    wrapMode: Text.WrapAnywhere
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Button {
                text: "Copy Error"
                visible: root.details.length > 0
                Layout.fillWidth: true
                onClicked: root.copyRequested(root.title + "\n" + root.message + "\n" + root.details)
                background: Rectangle { color: "#313244"; radius: 6 }
                contentItem: Text { text: parent.text; color: "#cdd6f4"; horizontalAlignment: Text.AlignHCenter }
            }
            Button {
                text: "OK"
                Layout.fillWidth: true
                onClicked: { root.acknowledged(); root.close() }
                background: Rectangle { color: "#f38ba8"; radius: 6 }
                contentItem: Text { text: parent.text; color: "#1e1e2e"; font.bold: true; horizontalAlignment: Text.AlignHCenter }
            }
        }
    }
}
