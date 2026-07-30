// =============================================================================
// ThemeEngine.qml — Runtime theme controller (singleton)
//
// STRICT RULE: minimize QML property count.  Too many properties (~97) in a
// singleton crashes the QML engine in static/cross-compiled builds.  JS objects
// are used for palette storage (2 properties vs 46 QML properties).
//
// Theme switching: JS assignment (NOT QML bindings) via applyTheme().
// Gated behind _ready flag to prevent cascading during singleton init.
//
// CANONICAL COLOR ACCESS: ThemeEngine.colors.xxx
//   All colors are accessed through the `colors` JS object.  The legacy
//   direct properties (bgDark, passGreen, etc.) below are deprecated
//   convenience aliases kept for backward compatibility.  New code MUST
//   use ThemeEngine.colors.xxx exclusively.
// =============================================================================
pragma Singleton
import QtQuick
import "Palette.js" as Palette

QtObject {
    readonly property int litMode: 1
    readonly property int drkMode: 2
    property int mode: drkMode
    readonly property bool isDark: mode !== litMode
    readonly property bool isMobile: Qt.platform.os === "ios" || Qt.platform.os === "android"

    // ── Palette reference (via Palette.js singleton, inlined to reduce
    // property count — ThemeEngine's own header warns ~97 props can crash
    // the QML engine in static builds.  Every non-essential property adds
    // to this count; two single-use routing properties don't justify the risk.)

    // 5WHY: All 23 deprecated readonly alias properties were removed in
    // Cycle 28.  Every consumer (607 references) now uses the canonical
    // ThemeEngine.colors.xxx pattern.  Removing these reduces the QML
    // property count from ~85 to ~62 — safer against the ~97-property
    // crash threshold in static/cross-compiled builds.

    // ── Theme switching (imperative JS — gated to skip init) ──────────
    property bool _ready: false
    Component.onCompleted: {
        if (typeof appState !== 'undefined' && appState && appState.themeMode !== undefined) {
            mode = appState.themeMode
        }
        _ready = true
        applyTheme()
    }

    // 5WHY: QML singletons can construct before C++ context properties are
    // registered.  If appState wasn't available at Component.onCompleted,
    // mode stays at default drkMode even if QSettings stored light mode (1).
    // The user sees a dark UI despite having saved light-theme preference.
    //
    // Fix: fire a zero-interval Timer — after the event loop starts, C++
    // context properties are guaranteed registered.  If the saved theme
    // differs from the default, mode assignment triggers onModeChanged →
    // applyTheme() (guarded by _ready=true) to correct the palette.
    Timer {
        interval: 0; running: true; repeat: false
        onTriggered: {
            if (typeof appState !== 'undefined' && appState && appState.themeMode !== undefined) {
                if (mode !== appState.themeMode) {
                    mode = appState.themeMode
                }
            }
        }
    }

    function applyTheme() {
        var p = (mode === litMode) ? Palette.Light : Palette.Dark

        // 5WHY: identity guard — Palette.Dark and Palette.Light are frozen
        // via Object.freeze() (Palette.js:97-98), so `colors === p` is a
        // fast reference-identity check.  Without this guard, every startup
        // (and every no-op theme re-selection in SettingsScreen) creates a
        // redundant Object.assign copy and triggers a full binding-graph
        // re-evaluation cascade (~600 ThemeEngine.colors.xxx consumers +
        // statusColors array rebuild) for zero net color change.
        if (colors !== p) {
            // Object.assign({}, p) creates a new object with all palette
            // properties copied — QML's binding engine detects the reference
            // change and re-evaluates all ThemeEngine.colors.xxx bindings.
            // This replaces the old 23-line manual enumeration which was a
            // maintenance burden: adding a color to Palette.js required also
            // adding it here, with no compile-time check for omissions.
            colors = Object.assign({}, p)
        }
    }
    onModeChanged: { if (_ready) applyTheme() }

    // ── Canonical color access — single source of truth ────────────────
    // 5WHY: colors was initialized as empty {} — the deprecated readonly
    // aliases (bgDark: colors.surface) resolved to undefined between
    // singleton construction and Component.onCompleted→applyTheme().
    // Initializing with Palette.Dark ensures all color properties are
    // valid from t=0.
    property var colors: Palette.Dark

    // 5WHY: Centralized status color array shared by DiagResultItem,
    // DashboardScreen, and any QML widget mapping DiagStatus values
    // (Pass=0, Warning=1, Fail=2, Skipped=3, Error=4, Info=5) to colors.
    // Binding expression directly references colors.xxx so QML tracking
    // detects theme switches and re-evaluates the array automatically.
    // 5WHY: Centralized status mappings — single source of truth for
    // DiagStatus (Pass=0, Warning=1, Fail=2, Skipped=3, Error=4, Info=5)
    // → color and icon name.  Previously duplicated across DiagResultItem,
    // DashboardScreen, and DiagId.h.
    // 5WHY: Error(4) shared failRed with Fail(2) — visually indistinguishable.
    // Now uses dedicated colors.errorRed (rose/magenta) so users can tell
    // infrastructure failure (Error) from assertion failure (Fail) at a glance.
    readonly property var statusColors: [
        colors.passGreen,   colors.warnYellow,  colors.failRed,
        colors.skipGray,    colors.errorRed,    colors.infoBlue
    ]
    readonly property var statusIconNames: [
        "badge-check",      // 0: Pass
        "badge-warning",    // 1: Warning
        "badge-close",      // 2: Fail
        "badge-skip",       // 3: Skipped
        "badge-error",      // 4: Error
        "badge-info"        // 5: Info
    ]

    readonly property int toastDurationMs: 3500
    readonly property var radius: ({ xs: 4, sm: 6, md: 8, lg: 12, xl: 16, full: 9999 })
    readonly property string fontMono: "JetBrains Mono"
    readonly property string monoFont: fontMono

    function pad2(n) { return (n < 10 ? " " : "") + n }
}
