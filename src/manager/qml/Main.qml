// =============================================================================
// Main.qml — AuraCastPro Root Window — Neo-brutalist B/W/Grey
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

ApplicationWindow {
    id: root
    visible: true
    width:  1100
    height: 720
    minimumWidth:  800
    minimumHeight: 560
    title: qsTr("AuraCastPro")
    color: Theme.bgBase

    // ── Font loading — Roboto Mono (bundled) with system fallback ─────────
    // Fonts are embedded in resources.qrc from assets/fonts/.
    // If the font files are missing (first build without downloaded fonts),
    // Qt falls back to the closest system monospace font automatically.
    FontLoader {
        id: fontRegular
        source: "qrc:/fonts/Inter-Regular.ttf"
        onStatusChanged: if (status === FontLoader.Error)
            console.warn("Roboto Mono Regular not found — using system monospace fallback")
    }
    FontLoader {
        id: fontBold
        source: "qrc:/fonts/Inter-Bold.ttf"
        onStatusChanged: if (status === FontLoader.Error)
            console.warn("Roboto Mono Bold not found — using system monospace fallback")
    }

    // ── Sidebar + content split ───────────────────────────────────────────
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Left sidebar
        Rectangle {
            width:  200
            Layout.fillHeight: true
            color:  Theme.bgPanel
            // Hard right border — brutalist divider
            Rectangle {
                anchors.right:  parent.right
                anchors.top:    parent.top
                anchors.bottom: parent.bottom
                width: 2
                color: Theme.borderActive
            }

            ColumnLayout {
                anchors.fill:    parent
                anchors.margins: 0
                spacing: 0

                // App wordmark
                Rectangle {
                    height: 64
                    Layout.fillWidth: true
                    color: Theme.bgBase
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width; height: 2
                        color: Theme.borderActive
                    }
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("AURACAST\nPRO")
                        font.family:    Theme.fontMono
                        font.pixelSize: 13
                        font.weight:    Font.Black
                        color:          Theme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 1.1
                        letterSpacing: 3
                    }
                }

                // Nav items
                Repeater {
                    model: [
                        { label: qsTr("DASHBOARD"),    page: 0 },
                        { label: qsTr("DEVICES"),      page: 1 },
                        { label: qsTr("SETTINGS"),     page: 2 },
                        { label: qsTr("DIAGNOSTICS"),  page: 3 },
                    ]
                    delegate: NavItem {
                        label:   modelData.label
                        page:    modelData.page
                        current: stack.currentIndex === modelData.page
                        onClicked: stack.currentIndex = modelData.page
                    }
                }

                Item { Layout.fillHeight: true }

                // Status bar at bottom of sidebar
                Rectangle {
                    height: 56
                    Layout.fillWidth: true
                    color: Theme.bgPanel
                    Rectangle {
                        anchors.top: parent.top
                        width: parent.width; height: 2
                        color: Theme.borderSubtle
                    }
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        Text {
                            text: hubModel ? hubModel.connectionStatus : qsTr("IDLE")
                            font.family:    Theme.fontMono
                            font.pixelSize: Theme.fontSizeXS
                            font.weight:    Font.Bold
                            color:          Theme.statusColour(hubModel ? hubModel.connectionStatus.toLowerCase() : qsTr("idle"))
                            letterSpacing:  2
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: qsTr("v1.0.0")
                            font.family:    Theme.fontMono
                            font.pixelSize: 9
                            color:          Theme.textDisabled
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }

        // Main content area
        StackLayout {
            id: stack
            Layout.fillWidth:  true
            Layout.fillHeight: true
            currentIndex: 0

            Dashboard   {}
            DeviceList  {}
            SettingsPage {}
            DiagnosticsPanel {}
        }
    }

    // ── Task 169: First Run Wizard — shown over main UI on first launch ────
    // settingsModel.firstRunCompleted is false until user finishes wizard.
    Loader {
        id: firstRunLoader
        anchors.fill: parent
        active: typeof settingsModel !== "undefined"
                && !settingsModel.firstRunCompleted
        source: "qrc:/qml/FirstRunWizard.qml"
        // Pass completion callback into the wizard
        onLoaded: {
            if (item) {
                item.wizardCompleted.connect(function() {
                    settingsModel.firstRunCompleted = true
                    firstRunLoader.active = false
                })
            }
        }
    }

// ── C++ invokable methods ─────────────────────────────────────────────────────
    // Called from HubWindow via QMetaObject::invokeMethod

    function toggleMirroring() {
        if (hubModel) {
            if (hubModel.isMirroring) hubModel.stopMirroring()
            else hubModel.startMirroring()
        }
    }

    function toggleRecording() {
        hubModel.toggleRecording()
    }

    function toggleFullscreen() {
        if (root.visibility === Window.FullScreen)
            root.showNormal()
        else
            root.showFullScreen()
    }

    function showDiagnostics() {
        stack.currentIndex = 3  // DiagnosticsPanel
    }

    function checkUpdates() {
        aboutDialog.open()      // re-use about dialog; update check fires on open
    }

    function showAbout() {
        aboutDialog.open()
    }

    function showPairingDialog(deviceName, pin) {
        pairingDialog.deviceName = deviceName
        pairingDialog.pinCode    = pin
        pairingDialog.open()
    }

    function onPairingSuccess() {
        pairingDialog.close()
    }

    // ── Dialogs ───────────────────────────────────────────────────────────────
    AboutDialog {
        id: aboutDialog
        anchors.centerIn: parent
    }

    PairingDialog {
        id: pairingDialog
        anchors.centerIn: parent
        property string deviceName: ""
        property string pinCode:    ""

        // Wire C++ pairing result → PairingDialog state machine
        Connections {
            target: hubModel
            function onPairingResult(success) {
                if (success) pairingDialog.showSuccess()
                else         pairingDialog.showFailed()
            }
        }
    }

// ── NavItem component (inline) ────────────────────────────────────────────────
component NavItem: Rectangle {
    property string label:   ""
    property int    page:    0
    property bool   current: false
    signal clicked()

    height: 48
    Layout.fillWidth: true
    color:  current ? Theme.borderActive : qsTr("transparent")

    Rectangle {
        visible: current
        anchors.left: parent.left
        anchors.top:  parent.top; anchors.bottom: parent.bottom
        width: 3
        color: Theme.bgBase
    }

    Text {
        anchors.centerIn: parent
        text:  label
        font.family:    Theme.fontMono
        font.pixelSize: Theme.fontSizeSM
        font.weight:    Font.Bold
        color:          current ? Theme.textInverse : Theme.textSecondary
        letterSpacing:  1.5
    }

    MouseArea {
        anchors.fill: parent
        cursorShape:  Qt.PointingHandCursor
        onClicked:    parent.clicked()
        hoverEnabled: true
        onEntered: if (!current) parent.color = Theme.bgHover
        onExited:  if (!current) parent.color = "transparent"
    }
}
}
