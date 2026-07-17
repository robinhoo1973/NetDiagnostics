import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

// Shared status badge: colored icon + count.  Used by DiagnosticScreen
// (BadgeLabel), DiagGroupPanel (StatusBadge), and DashboardScreen.
// 5WHY: was duplicated inline in 3 files with identical structure.
RowLayout {
    property color accent: ThemeEngine.passGreen
    property string iconName: "badge-info"
    property int count: 0
    spacing: 2
    AppIcon { name: iconName; size: 14; color: accent }
    Label {
        text: ThemeEngine.pad2(count)
        font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.Bold; color: accent
    }
}
