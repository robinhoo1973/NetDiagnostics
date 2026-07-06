// =============================================================================
// ThemeEngine.qml — Runtime theme controller (singleton)
//
// Uses imperative JS assignment for theme switching (NOT declarative bindings).
// QML singleton bindings with multi-level dependency chains cause crashes in
// static/cross-compiled builds.  JS assignment avoids binding evaluation during
// module import — all colors start as dark literals, updated on mode change.
// =============================================================================
pragma Singleton
import QtQuick

QtObject {
    // ── Theme mode ────────────────────────────────────────────────────
    readonly property int System: 0
    readonly property int Light:  1
    readonly property int Dark:   2

    property int mode: Dark

    // ── Light palette (readonly literals, never change) ──────────────
    readonly property string lSurface:          "#F8FAFC"
    readonly property string lSidebar:          "#FFFFFF"
    readonly property string lCard:             "#FFFFFF"
    readonly property string lInput:            "#F1F5F9"
    readonly property string lNavBar:           "#FFFFFF"
    readonly property string lPrimary:          "#0EA5E9"
    readonly property string lPrimaryContainer: "#E0F2FE"
    readonly property string lSecondary:        "#6366F1"
    readonly property string lTextPrimary:      "#0F172A"
    readonly property string lTextSecondary:    "#475569"
    readonly property string lTextMuted:        "#94A3B8"
    readonly property string lAccent:           "#F43F5E"
    readonly property string lCyan:             "#06B6D4"
    readonly property string lPass:             "#10B981"
    readonly property string lWarn:             "#F59E0B"
    readonly property string lFail:             "#EF4444"
    readonly property string lSkip:             "#9CA3AF"
    readonly property string lInfo:             "#3B82F6"
    readonly property string lBorderCard:       "#E2E8F0"
    readonly property string lBorderSubtle:     "#F1F5F9"
    readonly property string lBorderFocused:    "#0EA5E9"

    // ── Dark palette (readonly literals, never change) ───────────────
    readonly property string dSurface:          "#0F172A"
    readonly property string dSidebar:          "#0F172A"
    readonly property string dCard:             "#1E293B"
    readonly property string dInput:            "#334155"
    readonly property string dNavBar:           "#0F172A"
    readonly property string dPrimary:          "#38BDF8"
    readonly property string dPrimaryContainer: "#0C4A6E"
    readonly property string dSecondary:        "#818CF8"
    readonly property string dTextPrimary:      "#F1F5F9"
    readonly property string dTextSecondary:    "#94A3B8"
    readonly property string dTextMuted:        "#64748B"
    readonly property string dAccent:           "#FB7185"
    readonly property string dCyan:             "#22D3EE"
    readonly property string dPass:             "#4ADE80"
    readonly property string dWarn:             "#FBBF24"
    readonly property string dFail:             "#F87171"
    readonly property string dSkip:             "#9CA3AF"
    readonly property string dInfo:             "#60A5FA"
    readonly property string dBorderCard:       "#334155"
    readonly property string dBorderSubtle:     "#1E293B"
    readonly property string dBorderFocused:    "#38BDF8"

    // ── Active colors — initialised to dark, updated via applyTheme() ─
    property string bgDark:          dSurface
    property string bgSidebar:       dSidebar
    property string bgCard:          dCard
    property string bgInput:         dInput
    property string navBar:          dNavBar
    property string textPrimary:     dTextPrimary
    property string textSecondary:   dTextSecondary
    property string textMuted:       dTextMuted
    property string accent:          dAccent
    property string accentBlue:      dSecondary
    property string cyan:            dCyan
    property string passGreen:       dPass
    property string warnYellow:      dWarn
    property string failRed:         dFail
    property string skipGray:        dSkip
    property string infoBlue:        dInfo
    property string borderCard:      dBorderCard
    property string borderSubtle:    dBorderSubtle
    property string borderFocused:   dBorderFocused
    property string primary:         dPrimary
    property string primaryContainer: dPrimaryContainer
    property string secondary:       dSecondary

    // ── Theme switching (imperative JS — NO QML bindings on active colors)
    function applyTheme() {
        var lt = (mode === Light)
        bgDark          = lt ? lSurface       : dSurface
        bgSidebar       = lt ? lSidebar       : dSidebar
        bgCard          = lt ? lCard          : dCard
        bgInput         = lt ? lInput         : dInput
        navBar          = lt ? lNavBar        : dNavBar
        textPrimary     = lt ? lTextPrimary   : dTextPrimary
        textSecondary   = lt ? lTextSecondary : dTextSecondary
        textMuted       = lt ? lTextMuted     : dTextMuted
        accent          = lt ? lAccent        : dAccent
        accentBlue      = lt ? lSecondary     : dSecondary
        cyan            = lt ? lCyan          : dCyan
        passGreen       = lt ? lPass          : dPass
        warnYellow      = lt ? lWarn          : dWarn
        failRed         = lt ? lFail          : dFail
        skipGray        = lt ? lSkip          : dSkip
        infoBlue        = lt ? lInfo          : dInfo
        borderCard      = lt ? lBorderCard    : dBorderCard
        borderSubtle    = lt ? lBorderSubtle  : dBorderSubtle
        borderFocused   = lt ? lBorderFocused : dBorderFocused
        primary         = lt ? lPrimary       : dPrimary
        primaryContainer= lt ? lPrimaryContainer : dPrimaryContainer
        secondary       = lt ? lSecondary     : dSecondary
    }

    // React to theme mode changes
    onModeChanged: applyTheme()

    // ── Convenience objects (reference active properties — updated by applyTheme)
    readonly property var colors: ({
        surface: bgDark, card: bgCard, input: bgInput, sidebar: bgSidebar,
        navBar: navBar, primary: primary, primaryContainer: primaryContainer,
        secondary: secondary, textPrimary: textPrimary,
        textSecondary: textSecondary, textMuted: textMuted,
        accent: accent, cyan: cyan, passGreen: passGreen,
        warnYellow: warnYellow, failRed: failRed, skipGray: skipGray,
        infoBlue: infoBlue, borderCard: borderCard,
        borderSubtle: borderSubtle, borderFocused: borderFocused
    })
    readonly property var radius: ({ xs: 4, sm: 6, md: 8, lg: 12, xl: 16, full: 9999 })
    readonly property string fontMono: "JetBrains Mono"
    readonly property string monoFont: fontMono
}
