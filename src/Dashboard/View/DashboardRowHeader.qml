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
    // 窄屏两行头注入：进度条占满第二行剩余宽度（宽屏内联保持 48px）
    property bool fillProgress: false
    // H2：行时长 + 4px 彩色进度条（page-dashboard §2.2）
    readonly property bool _hasData: root.stats && (root.stats.total || 0) > 0
    // 5WHY (复核 2026-08-18): cancelled 计入 completed → 全取消的组进度条
    // 100% 绿色"全通过"假象。模型层已暴露 completedExclCancelled 单一推导
    // 字段（AppState::groupStats），此处消费而非手工减法。
    readonly property int _cancelledCount: root.stats && root.stats.cancelled ? root.stats.cancelled : 0
    readonly property real _ratio: _hasData
        ? ((root.stats.completedExclCancelled !== undefined ? root.stats.completedExclCancelled
                                                             : (root.stats.completed || 0) - _cancelledCount)
           / root.stats.total)
        : 0

    // 5WHY (复核 2026-08-18 三处复制收敛): 7 徽标行改用共享簇组件
    // （cancelled 徽标此前缺失——groupStats 把取消项计入 completed，
    // 行统计与徽标数字对不上）。
    StatusBadgeCluster { stats: root.stats; compact: ThemeEngine.isMobile }

    Label {
        visible: (root.stats && root.stats.durationMs || 0) > 0
        text: ThemeEngine.formatDuration(root.stats && root.stats.durationMs || 0)
        font.family: ThemeEngine.monoFont
        font.pixelSize: ThemeEngine.fontSize.caption
        color: ThemeEngine.colors.onSurfaceVariant
    }
    // 4px 进度条（track + fill）；窄屏两行头由 fillProgress 注入撑满第二行
    Rectangle {
        Layout.preferredWidth: 48
        Layout.fillWidth: root.fillProgress
        Layout.preferredHeight: 4
        radius: 2
        visible: root._hasData
        color: Qt.alpha(ThemeEngine.colors.textMuted, 0.2)
        Rectangle {
            anchors { top: parent.top; left: parent.left; bottom: parent.bottom }
            width: parent.width * root._ratio
            radius: 2
            // 5WHY (复核 2026-08-18): 全取消组不得渲染"全通过"绿条。比率已排除
            // cancelled（ratio>=1 必然意味着 cancelled==0），故绿条不需显式
            // 判取消；含取消且未失败的中途组用中性灰而非主色——取消参与时
            // 蓝色进度条会误读为"仍在正常运行"。
            color: root._ratio >= 1 ? ThemeEngine.colors.success
                 : root.stats && (root.stats.fail || 0) > 0 ? ThemeEngine.colors.fail
                 : root._cancelledCount > 0 ? ThemeEngine.colors.textMuted
                 : ThemeEngine.colors.primary
        }
    }
}
