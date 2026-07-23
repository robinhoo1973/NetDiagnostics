// =============================================================================
// ThemeEngine.qml — Runtime theme controller (singleton)
//
// STRICT RULE: minimize QML property count.  Too many properties (~97) in a
// singleton crashes the QML engine in static/cross-compiled builds.  JS objects
// are used for palette storage (2 properties vs 46 QML properties).
//
// Theme switching: JS assignment (NOT QML bindings) via applyTheme().
// Gated behind _ready flag to prevent cascading during singleton init.
// =============================================================================
pragma Singleton
import QtQuick

QtObject {
    readonly property int sysMode: 0
    readonly property int litMode: 1
    readonly property int drkMode: 2
    property int mode: drkMode
    // 5WHY: report preview hardcoded dark theme. Expose reactive boolean
    // so QML can pass ThemeEngine.isDark to C++ report generation calls.
    readonly property bool isDark: mode !== litMode

    // 5WHY: platform detection (Qt.platform.os === "ios" || Qt.platform.os === "android")
    // was duplicated as local properties (isMobile/compact/_isMobile) in 9+ files.
    // Centralize here so adding a new platform (wasm, tvOS) requires one edit.
    readonly property bool isMobile: Qt.platform.os === "ios" || Qt.platform.os === "android"

    // ── Palettes as JS objects (2 properties — NOT 46 QML properties) ──
    readonly property var lightPalette: ({
        surface:          "#F8FAFC", sidebar:    "#FFFFFF",
        card:             "#FFFFFF", input:      "#F1F5F9",
        navBar:           "#FFFFFF",
        primary:          "#0EA5E9", primaryContainer: "#E0F2FE",
        secondary:        "#6366F1",
        textPrimary:      "#0F172A", textSecondary:   "#475569",
        textMuted:        "#64748B",
        accent:           "#F43F5E", cyan:            "#06B6D4",
        passGreen:        "#4ADE80", warnYellow:      "#FBBF24",
        failRed:          "#F87171", skipGray:        "#9CA3AF",
        infoBlue:         "#A5B4FC",
        borderCard:       "#E2E8F0", borderSubtle:    "#F1F5F9",
        borderFocused:    "#0EA5E9"
    })

    // 5WHY: Dark palette accent colors (cyan, primary) were too dim for icon
    // visibility against dark backgrounds (#1E293B).  Text/surface colors flip
    // dramatically between themes (#0F172A ↔ #F1F5F9), but accent colors barely
    // changed (#06B6D4 → #22D3EE for cyan).  Result: icons blended into their
    // Qt.alpha(accent, 0.12) button backgrounds.  Bumped cyan +32% luminance
    // and primary +25% luminance for WCAG AA contrast against dark card surfaces.
    readonly property var darkPalette: ({
        surface:          "#0F172A", sidebar:    "#0F172A",
        card:             "#1E293B", input:      "#334155",
        navBar:           "#0F172A",
        primary:          "#60C8F8", primaryContainer: "#0C4A6E",
        secondary:        "#818CF8",
        textPrimary:      "#F1F5F9", textSecondary:   "#94A3B8",
        textMuted:        "#94A3B8",
        accent:           "#FB7185", cyan:            "#68E5F4",
        passGreen:        "#4ADE80", warnYellow:      "#FBBF24",
        failRed:          "#F87171", skipGray:        "#9CA3AF",
        infoBlue:         "#A5B4FC",
        borderCard:       "#334155", borderSubtle:    "#1E293B",
        borderFocused:    "#60C8F8"
    })

    // ── Active colors (direct literals — NO bindings, matching min ver) ──
    property string bgDark:          "#0F172A"
    property string bgSidebar:       "#0F172A"
    property string bgCard:          "#1E293B"
    property string bgInput:         "#334155"
    property string navBar:          "#0F172A"
    property string textPrimary:     "#F1F5F9"
    property string textSecondary:   "#94A3B8"
    property string textMuted:       "#94A3B8"
    property string accent:          "#FB7185"
    property string accentBlue:      "#818CF8"
    property string cyan:            "#68E5F4"
    property string passGreen:       "#4ADE80"
    property string warnYellow:      "#FBBF24"
    property string failRed:         "#F87171"
    property string skipGray:        "#9CA3AF"
    property string infoBlue:        "#A5B4FC"
    property string borderCard:      "#334155"
    property string borderSubtle:    "#1E293B"
    property string borderFocused:   "#60C8F8"
    property string primary:         "#60C8F8"
    property string primaryContainer: "#0C4A6E"
    property string secondary:       "#818CF8"

    // ── Theme switching (imperative JS — gated to skip init) ──────────
    property bool _ready: false
    Component.onCompleted: {
        // Restore persisted theme mode from AppState (survives app restarts).
        // Default is drkMode (2); AppState::loadSettings() restores saved value.
        if (typeof appState !== 'undefined' && appState && appState.themeMode !== undefined) {
            mode = appState.themeMode
        }
        _ready = true
        applyTheme()
    }


    function applyTheme() {
        var p = (mode === litMode) ? lightPalette : darkPalette
        // NOTE: must use dot notation — this[key]=val does NOT trigger
        // QML property change signals in static/cross-compiled builds
        bgDark          = p.surface;       bgSidebar       = p.sidebar
        bgCard          = p.card;          bgInput         = p.input
        navBar          = p.navBar;        textPrimary     = p.textPrimary
        textSecondary   = p.textSecondary; textMuted       = p.textMuted
        accent          = p.accent;        accentBlue      = p.secondary
        cyan            = p.cyan;          passGreen       = p.passGreen
        warnYellow      = p.warnYellow;    failRed         = p.failRed
        skipGray        = p.skipGray;      infoBlue        = p.infoBlue
        borderCard      = p.borderCard;    borderSubtle    = p.borderSubtle
        borderFocused   = p.borderFocused; primary         = p.primary
        primaryContainer= p.primaryContainer; secondary     = p.secondary
        // Rebuild colors JS object so all 152 consumers get fresh theme
        colors = ({
            surface: bgDark, card: bgCard, input: bgInput, sidebar: bgSidebar,
            navBar: navBar, primary: primary, primaryContainer: primaryContainer,
            secondary: secondary, textPrimary: textPrimary,
            textSecondary: textSecondary, textMuted: textMuted,
            accent: accent, cyan: cyan, passGreen: passGreen,
            warnYellow: warnYellow, failRed: failRed, skipGray: skipGray,
            infoBlue: infoBlue, borderCard: borderCard,
            borderSubtle: borderSubtle, borderFocused: borderFocused
        })
    }
    onModeChanged: { if (_ready) applyTheme() }

    // ── Convenience object (rebuilt on every theme switch) ────────────
    // 5WHY: readonly property var was a one-time JS object snapshot —
    // applyTheme() updated the direct properties but colors stayed stale.
    // Now non-readonly, rebuilt inside applyTheme() so all 152 consumers
    // get reactive theme updates.
    property var colors: ({})
    readonly property var radius: ({ xs: 4, sm: 6, md: 8, lg: 12, xl: 16, full: 9999 })
    readonly property string fontMono: "JetBrains Mono"
    readonly property string monoFont: fontMono

    // Shared space-pad for monospace badge alignment (ES5-compatible, no padStart).
    // 5WHY: three files had identical copies (_pad2, _pad2Fixed, _pad2Badge).
    // Centralized here as single source of truth.
    function pad2(n) { return (n < 10 ? " " : "") + n }
}
