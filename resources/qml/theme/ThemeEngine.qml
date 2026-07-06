// =============================================================================
// ThemeEngine.qml — Runtime theme controller (singleton)
//
// Minimal stable version.  The full MD3 light/dark palette + system detection
// will be re-enabled after crash root cause is identified.
// =============================================================================
pragma Singleton
import QtQuick

QtObject {
    property int mode: 2  // 0=System, 1=Light, 2=Dark

    // ── Active colors (single palette, dark by default) ────────────────
    readonly property string bgDark:          "#0F172A"
    readonly property string bgSidebar:       "#0F172A"
    readonly property string bgCard:          "#1E293B"
    readonly property string bgInput:         "#334155"
    readonly property string textPrimary:     "#F1F5F9"
    readonly property string textSecondary:   "#94A3B8"
    readonly property string textMuted:       "#64748B"
    readonly property string accent:          "#FB7185"
    readonly property string accentBlue:      "#818CF8"
    readonly property string cyan:            "#22D3EE"
    readonly property string passGreen:       "#4ADE80"
    readonly property string warnYellow:      "#FBBF24"
    readonly property string failRed:         "#F87171"
    readonly property string skipGray:        "#9CA3AF"
    readonly property string infoBlue:        "#60A5FA"
    readonly property string borderCard:      "#334155"
    readonly property string borderSubtle:    "#1E293B"
    readonly property string borderFocused:   "#38BDF8"
    readonly property string monoFont:        "JetBrains Mono"
    readonly property real   radiusCard:      12
    readonly property real   radiusButton:    8
    readonly property real   radiusSmall:     6
    readonly property real   sidebarWidth:    260

    // ── Placeholder for future theme switching ─────────────────────────
    readonly property var colors: ({ surface: bgDark, card: bgCard, input: bgInput,
        sidebar: bgSidebar, navBar: "#0F172A", primary: cyan, secondary: accentBlue,
        primaryContainer: "#0C4A6E", textPrimary: textPrimary, textSecondary: textSecondary,
        textMuted: textMuted, accent: accent, cyan: cyan, passGreen: passGreen,
        warnYellow: warnYellow, failRed: failRed, skipGray: skipGray, infoBlue: infoBlue,
        borderCard: borderCard, borderSubtle: borderSubtle, borderFocused: borderFocused })
    readonly property var radius: ({ xs: 4, sm: 6, md: 8, lg: 12, xl: 16, full: 9999 })
    readonly property string fontMono: monoFont
}
