// PageEmptyStateSection.qml — 空态/错误态（§2.7 + M2 错误恢复引导）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
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
    property int _completed: 0
    function _refresh() { root._completed = (AppState.groupStats(-1).completed || 0) }
    Connections {
        target: AppState
        function onProgressChanged() { root._refresh() }
        function onRunStatusChanged() { root._refresh() }
    }
    Component.onCompleted: _refresh()

    // 5WHY (复核 2026-08-18 与状态头互斥分区): 头部已覆盖全部终态（2/3/4）——
    // 空态仅在 Idle(0) 或 Error(4)（零结果运行失败，errorState 由调用方置位）
    // 显示，Running/Cancelled/Completed 由头部/运行信息卡呈现。
    active: root._completed === 0 && (AppState.runStatus === 0 || AppState.runStatus === 4)

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
