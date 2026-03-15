// =============================================================================
// SettingsPage.qml — Clean, functional settings
// =============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import "."

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top header with category tabs
        Rectangle {
            Layout.fillWidth: true
            height: 80
            color: "#0f141b"
            
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#2b3645"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                anchors.topMargin: 16
                anchors.bottomMargin: 12
                spacing: 12

                Text {
                    text: "Settings"
                    font.family: "Inter"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    color: "#e8eef7"
                }

                // Category tabs
                Row {
                    spacing: 8

                    Repeater {
                        model: [
                            { label: "Display", page: 0 },
                            { label: "Recording", page: 1 },
                            { label: "Network", page: 2 },
                            { label: "Device", page: 3 },
                            { label: "Hardware", page: 4 },
                            { label: "Advanced", page: 5 }
                        ]

                        delegate: Rectangle {
                            width: tabText.width + 24
                            height: 32
                            radius: 6
                            color: settingsStack.currentIndex === modelData.page ? "#223044" : "transparent"

                            Text {
                                id: tabText
                                anchors.centerIn: parent
                                text: modelData.label
                                font.family: "Inter"
                                font.pixelSize: 13
                                font.weight: settingsStack.currentIndex === modelData.page ? Font.DemiBold : Font.Normal
                                color: settingsStack.currentIndex === modelData.page ? "#e8eef7" : "#9cacbf"
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsStack.currentIndex = modelData.page
                                onEntered: if (settingsStack.currentIndex !== modelData.page) parent.color = "rgba(255,255,255,0.03)"
                                onExited: if (settingsStack.currentIndex !== modelData.page) parent.color = "transparent"
                            }
                        }
                    }
                }
            }
        }

        // Content area
        StackLayout {
            id: settingsStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0

            // Display Settings (Mirror Settings)
            ScrollView {
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: Math.min(parent.width - 56, 900)
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 20

                    Item { height: 12 }

                    Text {
                        text: "Display & Mirror Settings"
                        font.family: "Inter"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        color: "#e8eef7"
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Configure video quality, resolution, and encoding settings for screen mirroring from different devices."
                        font.family: "Inter"
                        font.pixelSize: 13
                        color: "#9cacbf"
                        wrapMode: Text.WordWrap
                    }

                    // iOS Settings Card
                    Rectangle {
                        Layout.fillWidth: true
                        radius: 10
                        color: "#1a2230"
                        border.color: "#2d3a4d"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 20

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Rectangle {
                                    width: 4
                                    height: 20
                                    radius: 2
                                    color: "#7bb3ff"
                                }

                                Text {
                                    text: "iOS / AirPlay Settings"
                                    font.family: "Inter"
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    color: "#e8eef7"
                                }
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 24
                                rowSpacing: 18

                                // Resolution
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Resolution"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: ["Native (Auto)", "2160p (4K)", "1440p (2K)", "1080p (Full HD)", "720p (HD)", "360p"]
                                        currentIndex: 0
                                    }
                                }

                                // Frame Rate
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Frame Rate"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: ["Auto", "120 FPS", "90 FPS", "60 FPS", "30 FPS", "24 FPS"]
                                        currentIndex: 0
                                    }
                                }

                                // Color Space
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Color Space"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: ["sRGB", "Display P3", "Rec. 709", "Rec. 2020", "Adobe RGB"]
                                        currentIndex: 0
                                    }
                                }

                                // Hardware Encoder
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Hardware Encoder"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: ["Auto", "Direct3D 12", "Direct3D 11", "OpenGL", "Vulkan", "Software (CPU)"]
                                        currentIndex: 0
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // Bitrate Slider for iOS
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true

                                    Text {
                                        text: "Bitrate"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: Math.round(iosBitrateSlider.value) + " MB/s"
                                        font.family: "Inter"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        color: "#7bb3ff"
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 48
                                    radius: 8
                                    color: "#0f1621"

                                    Slider {
                                        id: iosBitrateSlider
                                        anchors.fill: parent
                                        anchors.leftMargin: 16
                                        anchors.rightMargin: 16
                                        from: 2
                                        to: 200
                                        stepSize: 1
                                        value: 50
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true

                                    Text {
                                        text: "2 MB/s"
                                        font.family: "Inter"
                                        font.pixelSize: 11
                                        color: "#6f7d90"
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: "200 MB/s"
                                        font.family: "Inter"
                                        font.pixelSize: 11
                                        color: "#6f7d90"
                                    }
                                }
                            }
                        }
                    }

                    // Android Settings Card
                    Rectangle {
                        Layout.fillWidth: true
                        radius: 10
                        color: "#1a2230"
                        border.color: "#2d3a4d"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 20

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Rectangle {
                                    width: 4
                                    height: 20
                                    radius: 2
                                    color: "#7dd7b0"
                                }

                                Text {
                                    text: "Android / Cast Settings"
                                    font.family: "Inter"
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    color: "#e8eef7"
                                }
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 24
                                rowSpacing: 18

                                // Resolution
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Resolution"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: ["Native (Auto)", "2160p (4K)", "1440p (2K)", "1080p (Full HD)", "720p (HD)", "360p"]
                                        currentIndex: 0
                                    }
                                }

                                // Frame Rate
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Frame Rate"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: ["Auto", "120 FPS", "90 FPS", "60 FPS", "30 FPS", "24 FPS"]
                                        currentIndex: 0
                                    }
                                }

                                // Color Space
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Color Space"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: ["sRGB", "Display P3", "Rec. 709", "Rec. 2020", "Adobe RGB"]
                                        currentIndex: 0
                                    }
                                }

                                // Hardware Encoder
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Hardware Encoder"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: ["Auto", "Direct3D 12", "Direct3D 11", "OpenGL", "Vulkan", "Software (CPU)"]
                                        currentIndex: 0
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // Bitrate Slider for Android
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true

                                    Text {
                                        text: "Bitrate"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: Math.round(androidBitrateSlider.value) + " MB/s"
                                        font.family: "Inter"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        color: "#7dd7b0"
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 48
                                    radius: 8
                                    color: "#0f1621"

                                    Slider {
                                        id: androidBitrateSlider
                                        anchors.fill: parent
                                        anchors.leftMargin: 16
                                        anchors.rightMargin: 16
                                        from: 2
                                        to: 200
                                        stepSize: 1
                                        value: 50
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true

                                    Text {
                                        text: "2 MB/s"
                                        font.family: "Inter"
                                        font.pixelSize: 11
                                        color: "#6f7d90"
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: "200 MB/s"
                                        font.family: "Inter"
                                        font.pixelSize: 11
                                        color: "#6f7d90"
                                    }
                                }
                            }
                        }
                    }

                    Item { height: 32 }
                }
            }

            // Recording Settings
            ScrollView {
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: Math.min(parent.width - 56, 900)
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 20

                    Item { height: 12 }

                    // ═══════════════════════════════════════════════════════════
                    // OUTPUT SETTINGS CARD
                    // ═══════════════════════════════════════════════════════════
                    Rectangle {
                        Layout.fillWidth: true
                        radius: 10
                        color: "#1a2230"
                        border.color: "#2d3a4d"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 20

                            // Section header
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Rectangle {
                                    width: 4
                                    height: 20
                                    radius: 2
                                    color: "#7bb3ff"
                                }

                                Text {
                                    text: "Output Settings"
                                    font.family: "Inter"
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    color: "#e8eef7"
                                }
                            }

                            // Recording Path
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "Recording Path"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    color: "#b8c5d6"
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 38
                                        radius: 6
                                        color: "#0f1621"
                                        border.color: recordingPathInput.activeFocus ? "#7bb3ff" : "#3b4a5f"
                                        border.width: 1

                                        Behavior on border.color { ColorAnimation { duration: 150 } }

                                        TextInput {
                                            id: recordingPathInput
                                            anchors.fill: parent
                                            anchors.leftMargin: 12
                                            anchors.rightMargin: 12
                                            verticalAlignment: TextInput.AlignVCenter
                                            text: settingsModel ? settingsModel.recordingFolder : ""
                                            font.family: "Inter"
                                            font.pixelSize: 13
                                            color: "#e8eef7"
                                            selectByMouse: true
                                            onEditingFinished: {
                                                if (settingsModel) settingsModel.recordingFolder = text
                                            }
                                        }
                                    }

                                    Rectangle {
                                        width: 90
                                        height: 38
                                        radius: 6
                                        color: browseMouse.containsMouse ? "#3d5270" : "#2d4159"

                                        Behavior on color { ColorAnimation { duration: 150 } }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "Browse"
                                            font.family: "Inter"
                                            font.pixelSize: 13
                                            font.weight: Font.Medium
                                            color: "#e8eef7"
                                        }

                                        MouseArea {
                                            id: browseMouse
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            hoverEnabled: true
                                            onClicked: folderDialog.open()
                                        }
                                    }
                                }

                                CheckBox {
                                    text: "Generate file names without spaces"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    checked: settingsModel ? settingsModel.generateFileNameWithoutSpace : false
                                    onCheckedChanged: {
                                        if (settingsModel) settingsModel.generateFileNameWithoutSpace = checked
                                    }
                                }
                            }

                            // Divider
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // Format & Encoder Grid
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 24
                                rowSpacing: 18

                                // Recording Format
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Recording Format"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        id: recordingFormatCombo
                                        Layout.fillWidth: true
                                        model: ["Matroska Video (.mkv)", "MP4 (.mp4)", "FLV (.flv)", "MOV (.mov)", "AVI (.avi)"]
                                        currentIndex: {
                                            if (!settingsModel) return 0
                                            var idx = model.indexOf(settingsModel.recordingFormat)
                                            return idx >= 0 ? idx : 0
                                        }
                                        onActivated: {
                                            if (settingsModel) settingsModel.recordingFormat = currentText
                                        }
                                    }
                                }

                                // Video Encoder
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Video Encoder"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        id: videoEncoderCombo
                                        Layout.fillWidth: true
                                        model: ["NVIDIA NVENC H.264", "NVIDIA NVENC H.265", "x264", "x265", "AMD AMF H.264", "Intel QSV H.264"]
                                        currentIndex: {
                                            if (!settingsModel) return 0
                                            var idx = model.indexOf(settingsModel.videoEncoder)
                                            return idx >= 0 ? idx : 0
                                        }
                                        onActivated: {
                                            if (settingsModel) settingsModel.videoEncoder = currentText
                                        }
                                    }
                                }

                                // Audio Encoder
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Audio Encoder"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        id: audioEncoderCombo
                                        Layout.fillWidth: true
                                        model: ["FFmpeg AAC", "CoreAudio AAC", "Opus", "MP3", "FLAC"]
                                        currentIndex: {
                                            if (!settingsModel) return 0
                                            var idx = model.indexOf(settingsModel.audioEncoder)
                                            return idx >= 0 ? idx : 0
                                        }
                                        onActivated: {
                                            if (settingsModel) settingsModel.audioEncoder = currentText
                                        }
                                    }
                                }

                                // Audio Tracks
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Audio Tracks"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    Row {
                                        spacing: 14
                                        Repeater {
                                            model: 6
                                            delegate: Row {
                                                spacing: 6
                                                CheckBox {
                                                    checked: settingsModel ? (settingsModel.audioTrackMask & (1 << index)) !== 0 : false
                                                    onCheckedChanged: {
                                                        if (settingsModel) {
                                                            var mask = settingsModel.audioTrackMask
                                                            if (checked) {
                                                                mask |= (1 << index)
                                                            } else {
                                                                mask &= ~(1 << index)
                                                            }
                                                            settingsModel.audioTrackMask = mask
                                                        }
                                                    }
                                                }
                                                Text {
                                                    text: (index + 1).toString()
                                                    font.family: "Inter"
                                                    font.pixelSize: 12
                                                    color: "#9cacbf"
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Divider
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // Rescale Output
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "Rescale Output"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    color: "#b8c5d6"
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    ComboBox {
                                        id: rescaleFilterCombo
                                        Layout.fillWidth: true
                                        model: [
                                            "Lanczos (Sharpened scaling, 36 samples)",
                                            "Lanczos (Sharpened scaling, 16 samples)",
                                            "Bicubic",
                                            "Bilinear",
                                            "Nearest Neighbor"
                                        ]
                                        currentIndex: {
                                            if (!settingsModel) return 0
                                            var idx = model.indexOf(settingsModel.rescaleFilter)
                                            return idx >= 0 ? idx : 0
                                        }
                                        onActivated: {
                                            if (settingsModel) settingsModel.rescaleFilter = currentText
                                        }
                                    }

                                    ComboBox {
                                        id: rescaleResolutionCombo
                                        Layout.preferredWidth: 150
                                        model: ["1920x1080", "1280x720", "2560x1440", "3840x2160", "Source"]
                                        currentIndex: {
                                            if (!settingsModel) return 0
                                            var idx = model.indexOf(settingsModel.rescaleResolution)
                                            return idx >= 0 ? idx : 0
                                        }
                                        onActivated: {
                                            if (settingsModel) settingsModel.rescaleResolution = currentText
                                        }
                                    }
                                }
                            }

                            // Divider
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // File Splitting
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "Automatic File Splitting"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    color: "#b8c5d6"
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    CheckBox {
                                        id: autoFileSplittingCheckbox
                                        checked: settingsModel ? settingsModel.autoFileSplitting : false
                                        onCheckedChanged: {
                                            if (settingsModel) settingsModel.autoFileSplitting = checked
                                        }
                                    }

                                    ComboBox {
                                        id: fileSplitModeCombo
                                        Layout.fillWidth: true
                                        enabled: autoFileSplittingCheckbox.checked
                                        model: ["Split by Time", "Split by Size"]
                                        currentIndex: {
                                            if (!settingsModel) return 0
                                            var idx = model.indexOf(settingsModel.fileSplitMode)
                                            return idx >= 0 ? idx : 0
                                        }
                                        onActivated: {
                                            if (settingsModel) settingsModel.fileSplitMode = currentText
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ═══════════════════════════════════════════════════════════
                    // ENCODER SETTINGS CARD
                    // ═══════════════════════════════════════════════════════════
                    Rectangle {
                        Layout.fillWidth: true
                        radius: 10
                        color: "#1a2230"
                        border.color: "#2d3a4d"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 20

                            // Section header
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Rectangle {
                                    width: 4
                                    height: 20
                                    radius: 2
                                    color: "#7bb3ff"
                                }

                                Text {
                                    text: "Encoder Settings"
                                    font.family: "Inter"
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    color: "#e8eef7"
                                }
                            }

                            // Rate Control
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "Rate Control"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    color: "#b8c5d6"
                                }

                                ComboBox {
                                    id: rateControlCombo
                                    Layout.fillWidth: true
                                    model: ["Constant Bitrate", "Variable Bitrate", "Constant QP", "Lossless"]
                                    currentIndex: {
                                        if (!settingsModel) return 0
                                        var idx = model.indexOf(settingsModel.rateControl)
                                        return idx >= 0 ? idx : 0
                                    }
                                    onActivated: {
                                        if (settingsModel) settingsModel.rateControl = currentText
                                    }
                                }
                            }

                            // Divider
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // Bitrate Slider
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    Text {
                                        text: "Bitrate"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: Math.round(bitrateSlider.value / 1000) + " Mbps"
                                        font.family: "Inter"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        color: "#7bb3ff"
                                    }
                                }

                                // Slider with custom styling
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 48
                                    radius: 8
                                    color: "#0f1621"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 16
                                        anchors.rightMargin: 16
                                        spacing: 16

                                        Slider {
                                            id: bitrateSlider
                                            Layout.fillWidth: true
                                            from: 2000
                                            to: 200000
                                            stepSize: 1000
                                            value: settingsModel ? settingsModel.bitrate : 50000
                                            onMoved: {
                                                if (settingsModel) {
                                                    settingsModel.bitrate = Math.round(value)
                                                    bitrateInput.text = Math.round(value).toString()
                                                }
                                            }
                                        }

                                        Rectangle {
                                            width: 110
                                            height: 36
                                            radius: 6
                                            color: "#1a2230"
                                            border.color: bitrateInput.activeFocus ? "#7bb3ff" : "#3b4a5f"
                                            border.width: 1

                                            Behavior on border.color { ColorAnimation { duration: 150 } }

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 10
                                                anchors.rightMargin: 10
                                                spacing: 6

                                                TextInput {
                                                    id: bitrateInput
                                                    Layout.fillWidth: true
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    horizontalAlignment: TextInput.AlignRight
                                                    text: settingsModel ? settingsModel.bitrate.toString() : "50000"
                                                    font.family: "Inter"
                                                    font.pixelSize: 13
                                                    color: "#e8eef7"
                                                    selectByMouse: true
                                                    validator: IntValidator { bottom: 2000; top: 200000 }
                                                    onEditingFinished: {
                                                        if (settingsModel) {
                                                            var val = parseInt(text)
                                                            if (val >= 2000 && val <= 200000) {
                                                                settingsModel.bitrate = val
                                                                bitrateSlider.value = val
                                                            } else {
                                                                text = settingsModel.bitrate.toString()
                                                            }
                                                        }
                                                    }
                                                }

                                                Text {
                                                    text: "Kbps"
                                                    font.family: "Inter"
                                                    font.pixelSize: 11
                                                    color: "#6f7d90"
                                                }
                                            }
                                        }
                                    }
                                }

                                // Range labels
                                RowLayout {
                                    Layout.fillWidth: true

                                    Text {
                                        text: "2 Mbps"
                                        font.family: "Inter"
                                        font.pixelSize: 11
                                        color: "#6f7d90"
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: "200 Mbps"
                                        font.family: "Inter"
                                        font.pixelSize: 11
                                        color: "#6f7d90"
                                    }
                                }
                            }

                            // Divider
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // Keyframe Interval Slider
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    Text {
                                        text: "Keyframe Interval"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: keyframeSlider.value === 0 ? "Auto" : keyframeSlider.value + "s"
                                        font.family: "Inter"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        color: "#7bb3ff"
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 48
                                    radius: 8
                                    color: "#0f1621"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 16
                                        anchors.rightMargin: 16
                                        spacing: 16

                                        Slider {
                                            id: keyframeSlider
                                            Layout.fillWidth: true
                                            from: 0
                                            to: 10
                                            stepSize: 1
                                            value: settingsModel ? settingsModel.keyframeInterval : 0
                                            onMoved: {
                                                if (settingsModel) {
                                                    settingsModel.keyframeInterval = Math.round(value)
                                                    keyframeInput.text = Math.round(value).toString()
                                                }
                                            }
                                        }

                                        Rectangle {
                                            width: 90
                                            height: 36
                                            radius: 6
                                            color: "#1a2230"
                                            border.color: keyframeInput.activeFocus ? "#7bb3ff" : "#3b4a5f"
                                            border.width: 1

                                            Behavior on border.color { ColorAnimation { duration: 150 } }

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 10
                                                anchors.rightMargin: 10
                                                spacing: 6

                                                TextInput {
                                                    id: keyframeInput
                                                    Layout.fillWidth: true
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    horizontalAlignment: TextInput.AlignRight
                                                    text: settingsModel ? settingsModel.keyframeInterval.toString() : "0"
                                                    font.family: "Inter"
                                                    font.pixelSize: 13
                                                    color: "#e8eef7"
                                                    selectByMouse: true
                                                    validator: IntValidator { bottom: 0; top: 10 }
                                                    onEditingFinished: {
                                                        if (settingsModel) {
                                                            var val = parseInt(text)
                                                            if (val >= 0 && val <= 10) {
                                                                settingsModel.keyframeInterval = val
                                                                keyframeSlider.value = val
                                                            } else {
                                                                text = settingsModel.keyframeInterval.toString()
                                                            }
                                                        }
                                                    }
                                                }

                                                Text {
                                                    text: "s"
                                                    font.family: "Inter"
                                                    font.pixelSize: 11
                                                    color: "#6f7d90"
                                                }
                                            }
                                        }
                                    }
                                }

                                Text {
                                    text: "0 = Auto (recommended)"
                                    font.family: "Inter"
                                    font.pixelSize: 11
                                    color: "#6f7d90"
                                }
                            }

                            // Divider
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // Encoder Quality Settings Grid
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 24
                                rowSpacing: 18

                                // Preset
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Preset"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        id: encoderPresetCombo
                                        Layout.fillWidth: true
                                        model: [
                                            "P1: Fastest (Lowest Quality)",
                                            "P2: Faster",
                                            "P3: Fast",
                                            "P4: Medium",
                                            "P5: Slow",
                                            "P6: Slower",
                                            "P7: Slowest (Best Quality)"
                                        ]
                                        currentIndex: {
                                            if (!settingsModel) return 6
                                            var idx = model.indexOf(settingsModel.encoderPreset)
                                            return idx >= 0 ? idx : 6
                                        }
                                        onActivated: {
                                            if (settingsModel) settingsModel.encoderPreset = currentText
                                        }
                                    }
                                }

                                // Tuning
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Tuning"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        id: encoderTuningCombo
                                        Layout.fillWidth: true
                                        model: ["High Quality", "Low Latency", "Ultra Low Latency", "Lossless"]
                                        currentIndex: {
                                            if (!settingsModel) return 0
                                            var idx = model.indexOf(settingsModel.encoderTuning)
                                            return idx >= 0 ? idx : 0
                                        }
                                        onActivated: {
                                            if (settingsModel) settingsModel.encoderTuning = currentText
                                        }
                                    }
                                }

                                // Multipass Mode
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Multipass Mode"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        id: multipassModeCombo
                                        Layout.fillWidth: true
                                        model: [
                                            "Single Pass",
                                            "Two Passes (Quarter Resolution)",
                                            "Two Passes (Full Resolution)"
                                        ]
                                        currentIndex: {
                                            if (!settingsModel) return 1
                                            var idx = model.indexOf(settingsModel.multipassMode)
                                            return idx >= 0 ? idx : 1
                                        }
                                        onActivated: {
                                            if (settingsModel) settingsModel.multipassMode = currentText
                                        }
                                    }
                                }

                                // Profile
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: "Profile"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    ComboBox {
                                        id: encoderProfileCombo
                                        Layout.fillWidth: true
                                        model: ["baseline", "main", "high", "high444"]
                                        currentIndex: {
                                            if (!settingsModel) return 2
                                            var idx = model.indexOf(settingsModel.encoderProfile)
                                            return idx >= 0 ? idx : 2
                                        }
                                        onActivated: {
                                            if (settingsModel) settingsModel.encoderProfile = currentText
                                        }
                                    }
                                }
                            }

                            // Divider
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // B-Frames Slider
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    Text {
                                        text: "B-Frames"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: "#b8c5d6"
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: bFramesSlider.value === 0 ? "Disabled" : bFramesSlider.value.toString()
                                        font.family: "Inter"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        color: "#7bb3ff"
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 48
                                    radius: 8
                                    color: "#0f1621"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 16
                                        anchors.rightMargin: 16
                                        spacing: 16

                                        Slider {
                                            id: bFramesSlider
                                            Layout.fillWidth: true
                                            from: 0
                                            to: 4
                                            stepSize: 1
                                            value: settingsModel ? settingsModel.bFrames : 2
                                            onMoved: {
                                                if (settingsModel) {
                                                    settingsModel.bFrames = Math.round(value)
                                                    bFramesInput.text = Math.round(value).toString()
                                                }
                                            }
                                        }

                                        Rectangle {
                                            width: 70
                                            height: 36
                                            radius: 6
                                            color: "#1a2230"
                                            border.color: bFramesInput.activeFocus ? "#7bb3ff" : "#3b4a5f"
                                            border.width: 1

                                            Behavior on border.color { ColorAnimation { duration: 150 } }

                                            TextInput {
                                                id: bFramesInput
                                                anchors.fill: parent
                                                anchors.leftMargin: 10
                                                anchors.rightMargin: 10
                                                verticalAlignment: TextInput.AlignVCenter
                                                horizontalAlignment: TextInput.AlignHCenter
                                                text: settingsModel ? settingsModel.bFrames.toString() : "2"
                                                font.family: "Inter"
                                                font.pixelSize: 13
                                                color: "#e8eef7"
                                                selectByMouse: true
                                                validator: IntValidator { bottom: 0; top: 4 }
                                                onEditingFinished: {
                                                    if (settingsModel) {
                                                        var val = parseInt(text)
                                                        if (val >= 0 && val <= 4) {
                                                            settingsModel.bFrames = val
                                                            bFramesSlider.value = val
                                                        } else {
                                                            text = settingsModel.bFrames.toString()
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                Text {
                                    text: "0 = Disabled  •  2 = Recommended  •  4 = Maximum"
                                    font.family: "Inter"
                                    font.pixelSize: 11
                                    color: "#6f7d90"
                                }
                            }

                            // Divider
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // Advanced Checkboxes
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: "Advanced Options"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    color: "#b8c5d6"
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 24

                                    CheckBox {
                                        text: "Look-ahead"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        checked: settingsModel ? settingsModel.lookAhead : true
                                        onCheckedChanged: {
                                            if (settingsModel) settingsModel.lookAhead = checked
                                        }
                                    }

                                    CheckBox {
                                        text: "Adaptive Quantization"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        checked: settingsModel ? settingsModel.adaptiveQuantization : true
                                        onCheckedChanged: {
                                            if (settingsModel) settingsModel.adaptiveQuantization = checked
                                        }
                                    }
                                }
                            }

                            // Divider
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#2d3a4d"
                            }

                            // Custom Encoder Options
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "Custom Encoder Options"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    color: "#b8c5d6"
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 38
                                    radius: 6
                                    color: "#0f1621"
                                    border.color: customEncoderInput.activeFocus ? "#7bb3ff" : "#3b4a5f"
                                    border.width: 1

                                    Behavior on border.color { ColorAnimation { duration: 150 } }

                                    TextInput {
                                        id: customEncoderInput
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        verticalAlignment: TextInput.AlignVCenter
                                        text: settingsModel ? settingsModel.customEncoderOptions : ""
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        color: "#e8eef7"
                                        selectByMouse: true
                                        onEditingFinished: {
                                            if (settingsModel) settingsModel.customEncoderOptions = text
                                        }
                                    }

                                    Text {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        verticalAlignment: Text.AlignVCenter
                                        text: "e.g., rc-lookahead=32:aq-mode=3"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        color: "#6f7d90"
                                        visible: customEncoderInput.text.length === 0 && !customEncoderInput.activeFocus
                                    }
                                }
                            }
                        }
                    }

                    Item { height: 32 }
                }
            }

            // Network Settings (placeholder)
            Item {
                Text {
                    anchors.centerIn: parent
                    text: "Network Settings"
                    font.family: "Inter"
                    font.pixelSize: 18
                    color: "#9cacbf"
                }
            }

            // Device Settings (placeholder)
            Item {
                Text {
                    anchors.centerIn: parent
                    text: "Device Settings"
                    font.family: "Inter"
                    font.pixelSize: 18
                    color: "#9cacbf"
                }
            }

            // Hardware Settings (placeholder)
            Item {
                Text {
                    anchors.centerIn: parent
                    text: "Hardware Settings"
                    font.family: "Inter"
                    font.pixelSize: 18
                    color: "#9cacbf"
                }
            }

            // Shortcuts Settings (placeholder)
            Item {
                Text {
                    anchors.centerIn: parent
                    text: "Shortcuts Settings"
                    font.family: "Inter"
                    font.pixelSize: 18
                    color: "#9cacbf"
                }
            }

            // Advanced Settings (placeholder)
            Item {
                Text {
                    anchors.centerIn: parent
                    text: "Advanced Settings"
                    font.family: "Inter"
                    font.pixelSize: 18
                    color: "#9cacbf"
                }
            }
        }
    }

    // Folder Dialog
    FolderDialog {
        id: folderDialog
        title: "Select Recording Folder"
        onAccepted: {
            if (settingsModel) {
                var path = selectedFolder.toString()
                // Remove file:/// prefix
                path = path.replace(/^file:\/\/\//, "")
                settingsModel.recordingFolder = path
            }
        }
    }
}
