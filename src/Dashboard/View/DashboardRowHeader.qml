// =============================================================================
// DashboardRowHeader.qml — 行头徽标行（page-dashboard.md §2.5）
//
// 注入 PageGroupPanelSection.rowHeaderDelegate；stats 由父级 Binding 注入。
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets

RowLayout {
    id: root
    spacing: ThemeEngine.spacing.xs

    property var stats: ({})
    // H2：行时长 + 4px 彩色进度条（page-dashboard §2.2）
    readonly property bool _hasData: root.stats && (root.stats.total || 0) > 0
    readonly property real _ratio: _hasData ? Math.min(1, (root.stats.completed || 0) / root.stats.total) : 0

    StatusBadge { statusCode: 0; count: (root.stats && root.stats.pass) || 0 }
    StatusBadge { statusCode: 1; count: (root.stats && root.stats.warn) || 0 }
    StatusBadge { statusCode: 2; count: (root.stats && root.stats.fail) || 0 }
    StatusBadge { statusCode: 3; count: (root.stats && root.stats.skip) || 0 }
    StatusBadge { statusCode: 5; count: (root.stats && root.stats.info) || 0 }
    StatusBadge { statusCode: 4; count: (root.stats && root.stats.error) || 0 }

    Label {
        visible: (root.stats && root.stats.durationMs || 0) > 0
        text: ThemeEngine.formatDuration(root.stats && root.stats.durationMs || 0)
        font.family: ThemeEngine.monoFont
        font.pixelSize: ThemeEngine.fontSize.caption
        color: ThemeEngine.colors.onSurfaceVariant
    }
    // 4px 进度条（track + fill）
    Rectangle {
        Layout.preferredWidth: 48
        Layout.preferredHeight: 4
        radius: 2
        visible: root._hasData
        color: Qt.alpha(ThemeEngine.colors.textMuted, 0.2)
        Rectangle {
            anchors { top: parent.top; left: parent.left; bottom: parent.bottom }
            width: parent.width * root._ratio
            radius: 2
            color: root._ratio >= 1 ? ThemeEngine.colors.success
                 : root.stats && (root.stats.fail || 0) > 0 ? ThemeEngine.colors.fail
                 : ThemeEngine.colors.primary
        }
    }
}
