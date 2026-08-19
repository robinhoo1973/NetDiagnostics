// PageEmptyStateSection.qml — 空态/错误态（§2.7 + M2 错误恢复引导）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
// 统计订阅经 StatsBridge（5WHY 复核 2026-08-19）——StatsUtil.js 不再直接导入
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Plain

    // M2：错误态与空态差异化——errorState 显示 badge-error + errorCheck/
    // errorRecoveryHint；hintText 供页面注入引导文案（Dashboard：runFromDiag）
    property bool errorState: false
    property string hintText: ""

    // 5WHY (复核 2026-08-18 X/Y 同源): 空态门控曾读 AppState.totalCompleted
    // （m_results.size()，含换 scheme/停用后保留的旧结果）——与状态头/徽标的
    // 过滤后统计分叉：旧结果残留时空态与头部互相矛盾。统一读 groupStats 的
    // completed（UI-2：命令式刷新，绑定不调 Q_INVOKABLE）。
    // 5WHY (复核 2026-08-18 单一守卫): 经 StatsBridge 归一化——与其余三个
    // 消费方同源；内联 `|| 0` 只防 undefined，不防键重命名/零填充保证移除。
    // 5WHY (复核 2026-08-19 单一订阅点): 订阅/归一化/离屏门控/揭示自愈收敛
    // 进 StatsBridge；_completed 变纯绑定（读桥的 JS 值，无 Q_INVOKABLE）。
    StatsBridge {
        id: stats
        screenVisible: root.screenVisible
    }
    property int _completed: stats._s.completed

    // 5WHY (复核 2026-08-18 与状态头互斥分区): 头部已覆盖全部终态（2/3/4）——
    // 空态仅在 Idle(0) 或 Error(4)（零结果运行失败，errorState 由调用方置位）
    // 显示，Running/Cancelled/Completed 由头部/运行信息卡呈现。
    // 5WHY (复核 2026-08-19 Dashboard 空白回归): Dashboard 没有状态头、其
    // 运行信息卡以 hasData 门控——零结果取消(3)/完成(2)时整页无任何呈现。
    // includeTerminalEmpty（Dashboard 注入 true）放宽终态 2/3 的零结果空态。
    property bool includeTerminalEmpty: false
    active: root._completed === 0 && (AppState.runStatus === 0 || AppState.runStatus === 4
        || (root.includeTerminalEmpty && (AppState.runStatus === 2 || AppState.runStatus === 3)))

    ColumnLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: ThemeEngine.spacing.md
        AppIcon {
            Layout.alignment: Qt.AlignHCenter
            name: root.errorState ? "badge-error" : "compass"
            size: root.errorState ? 80 : 48
            color: root.errorState ? ThemeEngine.colors.fail : ThemeEngine.colors.textMuted
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.errorState ? T.tr("errorCheck") : T.tr("noData")
            color: root.errorState ? ThemeEngine.colors.fail : ThemeEngine.colors.onSurfaceVariant
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.body
            font.weight: root.errorState ? Font.DemiBold : Font.Normal
            horizontalAlignment: Text.AlignHCenter
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 320
            visible: root.errorState || root.hintText !== ""
            text: root.errorState
                ? (AppState.errorMessage !== "" ? T.trMsg(AppState.errorMessage) : T.tr("errorRecoveryHint"))
                : root.hintText
            color: ThemeEngine.colors.onSurfaceVariant
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.caption
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
