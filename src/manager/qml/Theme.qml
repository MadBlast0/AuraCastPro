// =============================================================================
// Theme.qml -- Soft dark design system
// =============================================================================
pragma Singleton
import QtQuick 2.15

QtObject {
    readonly property color bgBase:        "#0f141b"
    readonly property color bgPanel:       "#151c25"
    readonly property color bgCard:        "#1c2531"
    readonly property color bgHover:       "#263243"
    readonly property color bgInput:       "#202a38"
    readonly property color bgElevated:    "#223044"

    readonly property color textPrimary:   "#e8eef7"
    readonly property color textSecondary: "#9cacbf"
    readonly property color textDisabled:  "#6f7d90"
    readonly property color textInverse:   "#0f141b"

    readonly property color borderSubtle:  "#2b3645"
    readonly property color borderNormal:  "#3b4a5f"
    readonly property color borderActive:  "#7bb3ff"

    readonly property color accent:        "#7bb3ff"
    readonly property color accentDim:     "#5276a8"
    readonly property color accentGreen:   "#7dd7b0"
    readonly property color accentYellow:  "#e6c37a"
    readonly property color accentRed:     "#f28b82"
    readonly property color accentCyan:    "#7ecfe0"

    readonly property int spacingSM: 6
    readonly property int spacingMD: 12
    readonly property int spacingLG: 20
    readonly property int spacingXL: 32

    readonly property int radiusSM: 8
    readonly property int radiusMD: 14
    readonly property int radiusLG: 22

    readonly property int borderThin: 1
    readonly property int borderWidthNormal: 1
    readonly property int borderThick: 1

    readonly property int shadowOffX: 0
    readonly property int shadowOffY: 10
    readonly property color shadowColor: "#24000000"

    readonly property string fontSans: qsTr("Inter")
    readonly property string fontMono: qsTr("Inter")

    readonly property int fontSizeXS: 10
    readonly property int fontSizeSM: 12
    readonly property int fontSizeMD: 14
    readonly property int fontSizeLG: 16
    readonly property int fontSizeXL: 20
    readonly property int fontSizeH1: 32
    readonly property int fontSizeH2: 22

    readonly property int animFast: 60
    readonly property int animNormal: 120
    readonly property int animSlow: 250

    readonly property color bgPrimary: bgBase
    readonly property color bgSecondary: bgPanel
    readonly property color accentPurple: "#7b8cff"
    readonly property color accentBlue: "#7bb3ff"

    function latencyColour(ms) {
        if (ms <= 40) return accentGreen
        if (ms <= 70) return accentYellow
        return accentRed
    }

    function bitrateColour(kbps) {
        if (kbps >= 10000) return accentGreen
        if (kbps >= 3000) return accentYellow
        return accentRed
    }

    function formatBitrate(kbps) {
        if (kbps >= 1000) return (kbps / 1000).toFixed(1) + " Mbps"
        return kbps.toFixed(0) + " kbps"
    }

    function statusColour(status) {
        if (status === "mirroring" || status === "connected") return accentGreen
        if (status === "error") return accentRed
        if (status === "connecting") return accentYellow
        return textSecondary
    }
}
