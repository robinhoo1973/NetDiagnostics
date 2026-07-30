import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

// Shared status badge: colored icon + count.  Used by DiagnosticScreen
// (BadgeLabel), DiagGroupPanel (StatusBadge), and DashboardScreen.
// 5WHY: was duplicated inline in 3 files with identical structure.
//
// 5WHY (2026-07-30): Every call site hardcoded accent + iconName explicitly
// (e.g. StatusBadge { accent: ThemeEngine.colors.passGreen; iconName: "badge-check" }),
// bypassing the centralized ThemeEngine.statusColors[] and statusIconNames[]
// arrays.  Adding a 7th DiagStatus would require updating ~15 call sites
// across 3 files instead of ONE array in ThemeEngine.qml.
//
// Fix: statusCode (DiagStatus enum: 0=Pass,1=Warning,2=Fail,3=Skipped,
// 4=Error,5=Info) drives accent and iconName from the centralized arrays.
// Callers that pass statusCode get automatic theme-aware color + icon.
// Explicit accent/iconName overrides still work for non-standard badges
// (e.g. accent: ThemeEngine.colors.cyan for a running-indicator badge).
RowLayout {
    // ── New API: statusCode drives color + icon from centralized arrays ──
    // Set statusCode to a DiagStatus value (0-5) — accent and iconName are
    // automatically resolved from ThemeEngine.statusColors[] and
    // ThemeEngine.statusIconNames[].  These bindings re-evaluate on both
    // statusCode changes AND theme switches because the binding expressions
    // directly reference ThemeEngine.colors.xxx (QML-tracked dependencies).
    property int statusCode: -1

    // ── Derived defaults (overridable by explicit caller assignment) ────
    // accent: driven by statusCode → ThemeEngine.statusColors[] lookup.
    // When statusCode is out of range or unset (-1), falls back to skipGray.
    // Callers can still override with an explicit accent binding for
    // non-DiagStatus badges (e.g. a cyan "running" badge).
    property color accent: statusCode >= 0 && statusCode < ThemeEngine.statusColors.length
        ? ThemeEngine.statusColors[statusCode]
        : ThemeEngine.colors.skipGray

    // iconName: driven by statusCode → ThemeEngine.statusIconNames[] lookup.
    // Falls back to "badge-info" for out-of-range or unset statusCode.
    property string iconName: statusCode >= 0 && statusCode < ThemeEngine.statusIconNames.length
        ? ThemeEngine.statusIconNames[statusCode]
        : "badge-info"

    property int count: 0
    spacing: 2
    AppIcon { name: iconName; size: 14; color: accent }
    Label {
        text: ThemeEngine.pad2(count)
        font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.Bold; color: accent
    }
}
