// =============================================================================
// DashboardRowHeader.qml — 行头徽标行（page-dashboard.md §2.5）
//
// 注入 PageGroupPanelSection.rowHeaderDelegate；stats 由父级 Binding 注入。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import theme
import widgets

RowLayout {
    id: root
    spacing: ThemeEngine.spacing.xs

    property var stats: ({})

    StatusBadge { statusCode: 0; count: (root.stats && root.stats.pass) || 0 }
    StatusBadge { statusCode: 1; count: (root.stats && root.stats.warn) || 0 }
    StatusBadge { statusCode: 2; count: (root.stats && root.stats.fail) || 0 }
    StatusBadge { statusCode: 3; count: (root.stats && root.stats.skip) || 0 }
    StatusBadge { statusCode: 5; count: (root.stats && root.stats.info) || 0 }
    StatusBadge { statusCode: 4; count: (root.stats && root.stats.error) || 0 }
}
