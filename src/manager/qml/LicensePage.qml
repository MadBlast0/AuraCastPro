// =============================================================================
// LicensePage.qml — License activation and tier info screen
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    readonly property color bgDeep:    "#0D1117"
    readonly property color bgCard:    "#1C2128"
    readonly property color accent:    "#58A6FF"
    readonly property color gold:      "#E3B341"
    readonly property color green:     "#3FB950"
    readonly property color textMain:  "#E6EDF3"
    readonly property color textMuted: "#8B949E"
    readonly property color border:    "#30363D"

    property string currentTier: qsTr("Free")
    property bool isActivated: false
    property string activationStatus: ""

    Rectangle {
        anchors.fill: parent
        color: bgDeep

        ScrollView {
            anchors.fill: parent; anchors.margins: 24; clip: true

            ColumnLayout {
                width: Math.min(760, root.width - 48)
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 24

                // ── Current license badge ─────────────────────────────────
                Rectangle {
                    Layout.fillWidth: true; height: 80; radius: 12
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: currentTier === "Business" ? "#1A2A4A" : currentTier === "Pro" ? "#1A3A2A" : "#1C2128" }
                        GradientStop { position: 1; color: "#1C2128" }
                    }
                    border.color: currentTier === "Business" ? gold : currentTier === "Pro" ? green : border

                    RowLayout {
                        anchors { fill: parent; margins: 16 }
                        Text {
                            text: currentTier === "Business" ? "💼" : currentTier === "Pro" ? "⭐" : "🆓"
                            font.pixelSize: 32
                        }
                        ColumnLayout {
                            spacing: 2
                            Text { text: qsTr("AuraCastPro ") + currentTier; color: textMain; font.pixelSize: 16; font.bold: true }
                            Text {
                                text: currentTier === "Free" ? qsTr("Upgrade to unlock recording, 4K, and streaming") : qsTr("All features unlocked")
                                color: textMuted; font.pixelSize: 12
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            visible: currentTier === "Free"
                            text: qsTr("Upgrade")
                            background: Rectangle { color: gold; radius: 8 }
                            contentItem: Text { text: parent.text; color: "#0D1117"; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            onClicked: Qt.openUrlExternally("https://auracastpro.com/pricing")
                        }
                    }
                }

                // ── Activation section ────────────────────────────────────
                Rectangle {
                    Layout.fillWidth: true; radius: 10
                    color: bgCard; border.color: border
                    height: activationCol.height + 32

                    ColumnLayout {
                        id: activationCol
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
                        spacing: 14

                        Text { text: qsTr("Activate License"); color: textMain; font.pixelSize: 14; font.bold: true }
                        Text { text: qsTr("Enter your license key to unlock Pro or Business features."); color: textMuted; font.pixelSize: 12 }

                        TextField {
                            id: licenseKeyField
                            Layout.fillWidth: true
                            placeholderText: qsTr("AURA1-XXXXX-XXXXX-XXXXX-XXXXX")
                            color: textMain
                            background: Rectangle { color: "#0D1117"; border.color: accent; border.width: 2; radius: 8 }
                        }

                        TextField {
                            id: emailField
                            Layout.fillWidth: true
                            placeholderText: qsTr("your@email.com")
                            color: textMain
                            background: Rectangle { color: "#0D1117"; border.color: border; radius: 8 }
                        }

                        Text {
                            text: activationStatus
                            color: activationStatus.indexOf("success") !== -1 ? green :
                activationStatus.length > 0 ? "#F85149" : "transparent"
                            font.pixelSize: 12
                            visible: activationStatus.length > 0
                        }

                        RowLayout {
                            Button {
                                text: qsTr("Activate")
                                background: Rectangle { color: accent; radius: 8 }
                                contentItem: Text { text: parent.text; color: qsTr("white"); font.bold: true; horizontalAlignment: Text.AlignHCenter }
                                onClicked: {
                                    activationStatus = "Activating…"
                                    // Call C++ licenseManager.activate(key, email)
                                    if (typeof licenseManager !== "undefined")
                                        licenseManager.activate(licenseKeyField.text, emailField.text)
                                }
                            }
                            Button {
                                text: qsTr("Buy License")
                                flat: true
                                contentItem: Text { text: parent.text; color: accent; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                                background: Item {}
                                onClicked: Qt.openUrlExternally("https://auracastpro.com/pricing")
                            }
                        }
                    }
                }

                // ── Feature comparison table ──────────────────────────────
                Rectangle {
                    Layout.fillWidth: true; radius: 10
                    color: bgCard; border.color: border
                    height: tierTable.height + 32

                    ColumnLayout {
                        id: tierTable
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
                        spacing: 0

                        Text { text: qsTr("Plan Comparison"); color: textMain; font.pixelSize: 14; font.bold: true; bottomPadding: 12 }

                        // Header
                        RowLayout {
                            Layout.fillWidth: true; height: 32
                            Text { text: qsTr("Feature"); color: textMuted; font.pixelSize: 11; Layout.fillWidth: true }
                            Text { text: qsTr("Free"); color: textMuted; font.pixelSize: 11; width: 60; horizontalAlignment: Text.AlignHCenter }
                            Text { text: qsTr("Pro");  color: green;     font.pixelSize: 11; width: 60; horizontalAlignment: Text.AlignHCenter }
                            Text { text: qsTr("Biz");  color: gold;      font.pixelSize: 11; width: 60; horizontalAlignment: Text.AlignHCenter }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: border }

                        Repeater {
                            model: [
                                { f: qsTr("AirPlay + Cast + ADB"),  free: true,  pro: true,  biz: true },
                                { f: "1080p 60fps",           free: true,  pro: true,  biz: true },
                                { f: "4K 120fps",             free: false, pro: true,  biz: true },
                                { f: qsTr("Screen Recording"),      free: false, pro: true,  biz: true },
                                { f: qsTr("Virtual Camera (OBS)"),  free: false, pro: true,  biz: true },
                                { f: qsTr("No Watermark"),          free: false, pro: true,  biz: true },
                                { f: qsTr("RTMP Live Streaming"),   free: false, pro: false, biz: true },
                                { f: qsTr("Multi-Device (5+)"),     free: false, pro: false, biz: true },
                                { f: qsTr("API + Plugin Access"),   free: false, pro: false, biz: true },
                            ]
                            Rectangle {
                                Layout.fillWidth: true; height: 36
                    color: index % 2 === 0 ? "transparent" : "#0D1117"
                                RowLayout {
                                    anchors { fill: parent; leftMargin: 0; rightMargin: 0; topMargin: 0; bottomMargin: 0 }
                                    Text { text: modelData.f; color: textMain; font.pixelSize: 12; Layout.fillWidth: true; verticalAlignment: Text.AlignVCenter }
                                    Text { text: modelData.free ? "✓" : "—"; color: modelData.free ? green : textMuted; width: 60; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    Text { text: modelData.pro  ? "✓" : "—"; color: modelData.pro  ? green : textMuted; width: 60; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    Text { text: modelData.biz  ? "✓" : "—"; color: modelData.biz  ? green : textMuted; width: 60; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                }
                            }
                        }
                    }
                }
                Item { height: 16 }
            }
        }
    }
}
