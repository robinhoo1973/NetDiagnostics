import QtQuick

QtObject {
    // ── Background layers (exact Flutter AppTheme) ─────────────────────
    readonly property string bgDark:          "#1E1E2E"
    readonly property string bgSidebar:       "#252538"
    readonly property string bgCard:          "#16213E"
    readonly property string bgInput:         "#2A2A4A"

    // ── Text ───────────────────────────────────────────────────────────
    readonly property string textPrimary:     "#E0E0E0"
    readonly property string textSecondary:   "#A0A0B8"
    readonly property string textMuted:       "#606080"

    // ── Accent / Status colors (exact Flutter) ─────────────────────────
    readonly property string accent:          "#E94560"
    readonly property string accentBlue:      "#0078D4"
    readonly property string cyan:            "#00BCD4"
    readonly property string passGreen:       "#4ADE80"
    readonly property string warnYellow:      "#FACC15"
    readonly property string failRed:         "#EF4444"
    readonly property string skipGray:        "#888888"
    readonly property string infoBlue:        "#0078D4"

    // ── Borders ────────────────────────────────────────────────────────
    readonly property string borderCard:      "#3A3A5A"
    readonly property string borderSubtle:    "#2A2A4A"
    readonly property string borderFocused:   "#0078D4"

    // ── Dimensions ─────────────────────────────────────────────────────
    readonly property real radiusCard:       12
    readonly property real radiusButton:     8
    readonly property real radiusSmall:      6
    readonly property real sidebarWidth:     260

    // ── Typography ────────────────────────────────────────────────────────
    readonly property string monoFont: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"
}
