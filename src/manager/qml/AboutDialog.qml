// AboutDialog.qml — About dialog (neo-brutalist)
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Dialog {
    id: root
    modal: true; width: 400; height: 320
    background: Rectangle {
        color: Theme.bgCard
        border.color: Theme.borderActive; border.width: Theme.borderThick
        Rectangle { x: 5; y: 5; width: parent.width; height: parent.height; color: Theme.borderSubtle; z: -1 }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: Theme.spacingLG
        spacing: Theme.spacingMD

        Text {
            text: qsTr("AURACASTPRO")
            font.family:    Theme.fontMono; font.pixelSize: Theme.fontSizeH1
            font.weight:    Font.Black; color: Theme.textPrimary
        }
        Rectangle { height: 2; Layout.fillWidth: true; color: Theme.borderActive }

        Repeater {
            model: [
                "Version:   1.0.0",
                "Build:     2025-01-01",
                "Platform:  Windows 10/11 x64",
                "Protocols: AirPlay 2 · Google Cast · ADB",
                "GPU API:   DirectX 12",
                "Audio:     WASAPI + Virtual Cable",
            ]
            delegate: Text {
                text: modelData
                font.family:    Theme.fontMono; font.pixelSize: Theme.fontSizeSM
                color:          Theme.textSecondary
            }
        }

        Item { Layout.fillHeight: true }

        Text {
            text: qsTr("© 2025 AuraCastPro. All rights reserved.")
            font.family:    Theme.fontMono; font.pixelSize: Theme.fontSizeXS
            color:          Theme.textDisabled
        }

        Rectangle {
            Layout.fillWidth: true; height: 38; color: Theme.textPrimary
            border.color: Theme.borderActive; border.width: 2
            Text {
                anchors.centerIn: parent; text: qsTr("CLOSE")
                font.family:    Theme.fontMono; font.pixelSize: Theme.fontSizeSM
                font.weight:    Font.Black; color: Theme.textInverse
            }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.close() }
        }
    }
}
