// =============================================================================
// SplashProgress.qml — Task 191: Startup splash with progress bar
// Used as overlay during SubsystemInitialiser.initAll()
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "#0A0A14"
    width: 1920; height: 1080

    property int   progressPct: 0
    property string statusText: ""

    Image {
        source: qsTr("qrc:/textures/8K_Splashes/splash.png")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        opacity: 0.6
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20
        width: 480

        Text {
            text: qsTr("AuraCastPro")
            font.pixelSize: 48
            font.weight: Font.Bold
            color: "#FFFFFF"
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            text: qsTr("Ultra-Low Latency Screen Mirroring")
            font.pixelSize: 16
            color: "#8888AA"
            Layout.alignment: Qt.AlignHCenter
        }
    }

    // Progress bar + status at bottom
    ColumnLayout {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        anchors.horizontalCenter: parent.horizontalCenter
        width: 480
        spacing: 10

        Text {
            text: root.statusText
            color: "#AAAACC"
            font.pixelSize: 12
            font.family: qsTr("JetBrains Mono")
            Layout.alignment: Qt.AlignHCenter
        }

        Rectangle {
            Layout.fillWidth: true
            height: 4
            color: "#1E1E32"
            radius: 2
            Rectangle {
                width: parent.width * (root.progressPct / 100.0)
                height: parent.height
                color: "#6C63FF"
                radius: 2
                Behavior on width { NumberAnimation { duration: 300 } }
            }
        }

        Text {
            text: root.progressPct + "%"
            color: "#6C63FF"
            font.pixelSize: 11
            font.family: qsTr("JetBrains Mono")
            Layout.alignment: Qt.AlignRight
        }
    }

    // Fade-in animation
    opacity: 0
    NumberAnimation on opacity { to: 1; duration: 400; running: true }

    function dismiss() {
        fadeOut.start()
    }
    NumberAnimation {
        id: fadeOut; target: root; property: qsTr("opacity")
        to: 0; duration: 500
        onFinished: root.visible = false
    }
}
