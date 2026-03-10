// =============================================================================
// Theme.qml — Neo-brutalist Black / Grey / White design system
// =============================================================================
pragma Singleton
import QtQuick 2.15

QtObject {
    // ── Core Palette — BLACK, GREY, WHITE only ────────────────────────────────
    readonly property color bgBase:       "#000000"   // pure black background
    readonly property color bgPanel:      "#0D0D0D"   // panels / sidebar
    readonly property color bgCard:       "#161616"   // card surfaces
    readonly property color bgHover:      "#242424"   // hover / active state
    readonly property color bgInput:      "#1A1A1A"   // input fields

    readonly property color textPrimary:  "#FFFFFF"   // pure white
    readonly property color textSecondary:"#888888"   // mid-grey labels
    readonly property color textDisabled: "#444444"   // disabled
    readonly property color textInverse:  "#000000"   // text on white bg

    readonly property color borderSubtle: "#2A2A2A"   // subtle dividers
    readonly property color borderNormal: "#555555"   // card outlines
    readonly property color borderActive: "#FFFFFF"   // focused / active — WHITE

    // Brutalist accent — white only for primary actions
    readonly property color accent:       "#FFFFFF"
    readonly property color accentDim:    "#666666"

    // Status indicators — minimal colour, desaturated as much as possible
    readonly property color accentGreen:  "#AAFFAA"   // success (soft)
    readonly property color accentYellow: "#DDDDAA"   // warning (desaturated)
    readonly property color accentRed:    "#FF6666"   // error (muted)
    readonly property color accentCyan:   "#AADDDD"   // streaming (muted)

    // ── Spacing ───────────────────────────────────────────────────────────────
    readonly property int spacingSM: 6
    readonly property int spacingMD: 12
    readonly property int spacingLG: 20
    readonly property int spacingXL: 32

    // ── Neo-brutalist: ZERO or near-zero radius. Hard edges. ─────────────────
    readonly property int radiusSM: 0
    readonly property int radiusMD: 0
    readonly property int radiusLG: 2

    // ── Borders — thick, hard, white. Core of the brutalist aesthetic ─────────
    readonly property int borderThin:   1
    readonly property int borderWidthNormal: 2   // preferred: use borderNormal for color, borderWidthNormal for width
    readonly property int borderThick:  3

    // ── Hard shadow offset (instead of blur shadows) ──────────────────────────
    readonly property int shadowOffX: 4
    readonly property int shadowOffY: 4
    readonly property color shadowColor: "#FFFFFF"   // white shadow on black bg

    // ── Typography ────────────────────────────────────────────────────────────
    // Mono font for ALL text — brutalist, raw, functional aesthetic
    readonly property string fontSans: qsTr("Roboto Mono")
    readonly property string fontMono: qsTr("Roboto Mono")

    readonly property int fontSizeXS:  10
    readonly property int fontSizeSM:  12
    readonly property int fontSizeMD:  14
    readonly property int fontSizeLG:  16
    readonly property int fontSizeXL:  20
    readonly property int fontSizeH1:  32
    readonly property int fontSizeH2:  22

    // ── Animation — snappy, no easing fluff ───────────────────────────────────
    readonly property int animFast:   60
    readonly property int animNormal: 120
    readonly property int animSlow:   250

    // ── Task 180: Compatibility aliases (task spec used these names) ──────────
    // The design uses a neo-brutalist monochrome palette but exports the purple-
    // theme aliases so that any QML referencing the spec names still compiles.
    readonly property color bgPrimary:    bgBase       // "#000000"
    readonly property color bgSecondary:  bgPanel      // "#0D0D0D"
    readonly property color accentPurple: "#6C63FF"    // spec purple (used on upgrade badges)
    readonly property color accentBlue:   "#4A9EFF"    // info highlights

    // ── Helper functions ──────────────────────────────────────────────────────
    function latencyColour(ms) {
        if (ms <= 40)  return accentGreen
        if (ms <= 70)  return accentYellow
        return accentRed
    }

    function bitrateColour(kbps) {
        if (kbps >= 10000) return accentGreen
        if (kbps >= 3000)  return accentYellow
        return accentRed
    }

    function formatBitrate(kbps) {
        if (kbps >= 1000) return (kbps / 1000).toFixed(1) + " Mbps"
        return kbps.toFixed(0) + " kbps"
    }

    function statusColour(status) {
        if (status === "mirroring" || status === "connected") return accentGreen
        if (status === "error")    return accentRed
        if (status === "connecting") return accentYellow
        return textSecondary
    }
}
