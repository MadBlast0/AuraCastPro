// =============================================================================
// AppHeader.qml — macOS-style header with functional window controls
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "."

Rectangle {
    id: root
    height: 52
    color: "#151c25"
    
    // Make header draggable
    DragHandler {
        target: null
        onActiveChanged: {
            if (active) {
                Window.window.startSystemMove()
            }
        }
    }
    
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: {
            if (Window.window.visibility === Window.Maximized) {
                Window.window.showNormal()
            } else {
                Window.window.showMaximized()
            }
        }
    }
    
    // Bottom border
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#2b3645"
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

        // Spacer
        Item { width: 8 }

        // Windows-style window controls
        Row {
            spacing: 0
            Layout.alignment: Qt.AlignVCenter
            
            // Minimize
            WindowButton {
                icon: "−"
                onClicked: Window.window.showMinimized()
            }
            
            // Maximize/Restore
            WindowButton {
                icon: Window.window.visibility === Window.Maximized ? "❐" : "□"
                onClicked: {
                    if (Window.window.visibility === Window.Maximized) {
                        Window.window.showNormal()
                    } else {
                        Window.window.showMaximized()
                    }
                }
            }
            
            // Close
            WindowButton {
                icon: "✕"
                isClose: true
                onClicked: Qt.quit()
            }
        }
    }

    // Windows-style window control button
    component WindowButton: Rectangle {
        property string icon: ""
        property bool isClose: false
        signal clicked()
        
        width: 46
        height: 32
        color: {
            if (isClose && buttonMouse.containsMouse) return "#e81123"
            if (buttonMouse.containsMouse) return "#3b4a5f"
            return "transparent"
        }
        
        Behavior on color { ColorAnimation { duration: 100 } }
        
        Text {
            anchors.centerIn: parent
            text: icon
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: 10
            color: {
                if (isClose && buttonMouse.containsMouse) return "white"
                return "#e8eef7"
            }
        }
        
        MouseArea {
            id: buttonMouse
            anchors.fill: parent
            hoverEnabled: true
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
            if (isActive) return "#7bb3ff"
            if (isRed) return "#f28b82"
            return "#223044"
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
                    return "#e8eef7"
                }
            }

            Text {
                text: label
                font.family: "Inter"
                font.pixelSize: 13
                font.weight: Font.Medium
                color: {
                    if (isActive || isRed) return "white"
                    return "#e8eef7"
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
                    parent.color = "#263243"
                }
            }
            onExited: {
                if (!isActive && !isRed) {
                    parent.color = "#223044"
                }
            }
        }
    }
}
