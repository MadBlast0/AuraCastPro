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

    FolderDialog {
        id: folderDialog
        currentFolder: settingsModel ? "file:///" + settingsModel.recordingFolder : ""
        onAccepted: {
            if (settingsModel) {
                var path = selectedFolder.toString()
                path = path.replace(/^file:\/\/\//, "")
                settingsModel.recordingFolder = path
                recordingPathInput.text = path
            }
        }
    }

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
                            { label: "Mirroring", page: 0 },
                            { label: "Recording", page: 1 }
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

            // Mirroring Settings
            Item {
                ScrollView {
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: Math.min(parent.width - 56, 900)
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 20

                        Item { height: 12 }

                        Text {
                            Layout.fillWidth: true
                            text: "Mirroring Settings"
                            font.family: "Inter"
                            font.pixelSize: 16
                            font.weight: Font.Bold
                            color: "#e8eef7"
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Configure video quality, resolution, and encoding settings for screen mirroring."
                            font.family: "Inter"
                            font.pixelSize: 13
                            color: "#9cacbf"
                            wrapMode: Text.WordWrap
                        }

                        // Mirroring Settings Card
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: mirrorContent.implicitHeight + 48
                            radius: 10
                            color: "#1a2230"
                            border.color: "#2d3a4d"
                            border.width: 1

                            ColumnLayout {
                                id: mirrorContent
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
                                        text: "Video Quality & Encoding"
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
                                            currentIndex: settingsModel ? settingsModel.maxResolutionIndex : 0
                                            onActivated: {
                                                if (settingsModel) settingsModel.maxResolutionIndex = currentIndex
                                            }
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
                                            currentIndex: settingsModel ? settingsModel.fpsCapIndex : 0
                                            onActivated: {
                                                if (settingsModel) settingsModel.fpsCapIndex = currentIndex
                                            }
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
                                            currentIndex: {
                                                if (!settingsModel) return 0
                                                var idx = model.indexOf(settingsModel.colorSpace)
                                                return idx >= 0 ? idx : 0
                                            }
                                            onActivated: {
                                                if (settingsModel) settingsModel.colorSpace = currentText
                                            }
                                        }
                                    }

                                    // Video Codec (Decoder)
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "Video Codec (Decoder)"
                                            font.family: "Inter"
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                            color: "#b8c5d6"
                                        }

                                        ComboBox {
                                            Layout.fillWidth: true
                                            model: ["H.264 (Hardware - NVIDIA/AMD/Intel)", "H.265/HEVC (Hardware - NVIDIA/AMD/Intel)"]
                                            currentIndex: {
                                                if (!settingsModel) return 0
                                                var codec = settingsModel.preferredCodec
                                                if (codec === "h265" || codec === "hevc") return 1
                                                return 0
                                            }
                                            onActivated: {
                                                if (settingsModel) {
                                                    settingsModel.preferredCodec = (currentIndex === 0) ? "h264" : "h265"
                                                }
                                            }
                                        }
                                    }
                                }

                                // Renderer Info
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: rendererInfo.implicitHeight + 16
                                    radius: 6
                                    color: "#0f1419"
                                    border.color: "#2d3a4d"
                                    border.width: 1

                                    RowLayout {
                                        id: rendererInfo
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 10

                                        Text {
                                            text: "ℹ️"
                                            font.pixelSize: 14
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Renderer: Direct3D 12 (Hardware - GPU)"
                                            font.family: "Inter"
                                            font.pixelSize: 12
                                            color: "#9cacbf"
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }

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

                                        Text {
                                            text: "Bitrate"
                                            font.family: "Inter"
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                            color: "#b8c5d6"
                                        }

                                        Item { Layout.fillWidth: true }

                                        Text {
                                            text: Math.round(mirrorBitrateSlider.value) + " MB/s"
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
                                            id: mirrorBitrateSlider
                                            anchors.fill: parent
                                            anchors.leftMargin: 16
                                            anchors.rightMargin: 16
                                            from: 2
                                            to: 200
                                            stepSize: 1
                                            value: settingsModel ? settingsModel.maxBitrateMbps : 20
                                            onMoved: {
                                                if (settingsModel) {
                                                    settingsModel.maxBitrateKbps = Math.round(value * 1000)
                                                }
                                            }
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
            }

            // Recording Settings
            Item {
                ScrollView {
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: Math.min(parent.width - 56, 900)
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 20

                        Item { height: 12 }

                        // Output Settings Card
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: outputContent.implicitHeight + 48
                            radius: 10
                            color: "#1a2230"
                            border.color: "#2d3a4d"
                            border.width: 1

                            ColumnLayout {
                                id: outputContent
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
                                            text: "Video Encoder (Recording)"
                                            font.family: "Inter"
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                            color: "#b8c5d6"
                                        }

                                        ComboBox {
                                            id: videoEncoderCombo
                                            Layout.fillWidth: true
                                            model: [
                                                "NVIDIA NVENC H.264 (Hardware - GPU)",
                                                "NVIDIA NVENC H.265 (Hardware - GPU)",
                                                "AMD AMF H.264 (Hardware - GPU)",
                                                "Intel QSV H.264 (Hardware - GPU)",
                                                "x264 (Software - CPU)",
                                                "x265 (Software - CPU)"
                                            ]
                                            currentIndex: {
                                                if (!settingsModel) return 0
                                                var encoder = settingsModel.videoEncoder
                                                // Map old values to new labels
                                                if (encoder.indexOf("NVIDIA NVENC H.264") >= 0) return 0
                                                if (encoder.indexOf("NVIDIA NVENC H.265") >= 0) return 1
                                                if (encoder.indexOf("AMD AMF H.264") >= 0) return 2
                                                if (encoder.indexOf("Intel QSV H.264") >= 0) return 3
                                                if (encoder.indexOf("x264") >= 0) return 4
                                                if (encoder.indexOf("x265") >= 0) return 5
                                                return 0
                                            }
                                            onActivated: {
                                                if (settingsModel) {
                                                    // Strip the hardware label before saving
                                                    var cleanName = currentText.split(" (")[0]
                                                    settingsModel.videoEncoder = cleanName
                                                }
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

                        // Encoder Settings Card
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: encoderContent.implicitHeight + 48
                            radius: 10
                            color: "#1a2230"
                            border.color: "#2d3a4d"
                            border.width: 1

                            ColumnLayout {
                                id: encoderContent
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
                            }
                        }

                        Item { height: 32 }
                    }
                }
            }
        }
    }
}
