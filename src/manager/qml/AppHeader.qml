// =============================================================================
// AppHeader.qml — macOS-style header with window controls on right
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Rectangle {
    id: root
    height: 52
    color: "#151c25"  // Dark header color matching Theme.bgPanel
    
    // Top rounded corners mask
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 12
        color: "#151c25"
    }
    
    // Make header draggable
    MouseArea {
        id: dragArea
        anchors.fill: parent
        property point clickPos: Qt.point(0, 0)
        
        onPressed: {
            clickPos = Qt.point(mouse.x, mouse.y)
        }
        
        onPositionChanged: {
            if (pressed) {
                // Window dragging
            }
        }
    }
    
    // Bottom border
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.borderSubtle
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12
        z: 1

        Item { Layout.fillWidth: true }

        // Always on Top button
        MacOSButton {
            iconText: "📌"
            label: "Always on Top"
            isActive: settingsModel ? settingsModel.alwaysOnTop : false
            onClicked: {
                if (settingsModel) {
                    settingsModel.alwaysOnTop = !settingsModel.alwaysOnTop
                }
            }
        }

        // Recording button
        MacOSButton {
            iconText: "⏺"
            label: "Record"
            isRed: true
            onClicked: {
                if (hubModel) hubModel.toggleRecording()
            }
        }

        // macOS-style window controls (RIGHT side - Windows position)
        Row {
            spacing: 8
            Layout.alignment: Qt.AlignVCenter
            
            MacButton {
                buttonColor: "#28c840"
                onClicked: {
                    // Maximize/restore
                }
            }
            
            MacButton {
                buttonColor: "#febc2e"
                onClicked: {
                    // Minimize
                }
            }
            
            MacButton {
                buttonColor: "#ff5f57"
                onClicked: Qt.quit()
            }
        }
    }

    // macOS window control button
    component MacButton: Rectangle {
        property color buttonColor: "#ff5f57"
        signal clicked()
        
        width: 12
        height: 12
        radius: 6
        color: buttonColor
        
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    // macOS-style action button
    component MacOSButton: Rectangle {
        property string iconText: ""
        property string label: ""
        property bool isActive: false
        property bool isRed: false
        signal clicked()

        implicitWidth: buttonRow.width + 16
        implicitHeight: 28
        radius: 6
        color: {
            if (isActive) return Theme.accent
            if (isRed) return Theme.accentRed
            return Theme.bgElevated
        }
        border.width: 0

        RowLayout {
            id: buttonRow
            anchors.centerIn: parent
            spacing: 6

            Text {
                text: iconText
                font.pixelSize: 13
                color: {
                    if (isActive || isRed) return "white"
                    return Theme.textPrimary
                }
            }

            Text {
                text: label
                font.family: Theme.fontSans
                font.pixelSize: 13
                font.weight: Font.Medium
                color: {
                    if (isActive || isRed) return "white"
                    return Theme.textPrimary
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
            onEntered: {
                if (!isActive && !isRed) {
                    parent.color = Theme.bgHover
                }
            }
            onExited: {
                if (!isActive && !isRed) {
                    parent.color = Theme.bgElevated
                }
            }
        }
    }
}
