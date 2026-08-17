// PageStatusHeaderSection.qml — 状态/徽标/分享头部（§2.3）
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
    bottomMargin: ThemeEngine.spacing.sm

    // 页面注入的额外头部组件（Dashboard：摘要统计层）—— UI-5：依赖方向页面→Common
    property Component headerExtra: null
    // 状态呈现单次求值（5WHY review round 4: icon/color/label 各自调用
    // runStatusInfo——同一次变化最多 4 次对象构造；单一属性共享）
    readonly property var _statusInfo: ThemeEngine.runStatusInfo(AppState.runStatus)

    // UI-2：聚合统计命令式刷新（progressChanged/runStatusChanged 处理器赋值），
    // 绑定中不调用 groupStats(-1)。
    property var _agg: ({ pass: 0, warn: 0, fail: 0, skip: 0, info: 0, error: 0, total: 0 })
    function _refreshAgg() {
        var s = AppState.groupStats(-1)
        _agg = { pass: s.pass || 0, warn: s.warn || 0, fail: s.fail || 0,
                 skip: s.skip || 0, info: s.info || 0, error: s.error || 0,
                 total: s.total || 0 }
    }
    Connections {
        target: AppState
        function onProgressChanged() { root._refreshAgg() }
        function onRunStatusChanged() { root._refreshAgg() }
    }
    Component.onCompleted: _refreshAgg()

    active: AppState.totalCompleted > 0 || AppState.runStatus === 1
    signal shareRequested(string fmt)

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: ThemeEngine.spacing.md
        Layout.rightMargin: ThemeEngine.spacing.md
        spacing: ThemeEngine.spacing.sm

        // 运行指示（spinner 或状态点）
        Item {
            Layout.preferredWidth: 16; Layout.preferredHeight: 16
            BusyIndicator {
                visible: AppState.runStatus === 1
                anchors.fill: parent
                running: AppState.runStatus === 1
            }
            AppIcon {
                visible: AppState.runStatus !== 1
                anchors.fill: parent
                // 5WHY (review round 3): 取消/错误态曾硬编码绿色 check——
                // 与同行的 runStatusInfo 标签/文字色脱节（橙字配绿勾）
                // 5WHY (review round 4): 曾误写 root._statusInfo——本组件
                // 根 id 是 root（page 未定义 → 绑定求值 ReferenceError，
                // 状态图标/标签永远损坏）
                name: (root._statusInfo ? root._statusInfo.iconName : "badge-check"); size: 16
                color: (root._statusInfo ? root._statusInfo.color : ThemeEngine.colors.success)
            }
        }
        Label {
            // M1：完成/取消/错误三态词 + X/Y 进度计数（归档语义）
            text: {
                if (AppState.runStatus === 1)
                    return (AppState.currentDiagLabel || T.tr("running"))
                           + (_agg.total > 0 ? " · " + AppState.totalCompleted + "/" + _agg.total : "")
                if (root._statusInfo) return T.tr(root._statusInfo.labelKey)
                return (_agg.total > 0
                    ? AppState.totalCompleted + "/" + _agg.total
                    : AppState.totalCompleted) + " " + T.tr("completed")
            }
            color: (root._statusInfo ? root._statusInfo.color
                                       : ThemeEngine.colors.onSurfaceVariant)
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.body
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        StatusBadge { statusCode: 0; count: _agg.pass }
        StatusBadge { statusCode: 1; count: _agg.warn }
        StatusBadge { statusCode: 2; count: _agg.fail }
        StatusBadge { statusCode: 3; count: _agg.skip }
        StatusBadge { statusCode: 5; count: _agg.info }
        StatusBadge { statusCode: 4; count: _agg.error }
        // 8-2：分享按钮从状态头移除（文件图标不该出现在标题栏右侧；
        // 分享入口归位到 Dashboard 报告预览卡）
    }

    // 注入的摘要层（Dashboard headerExtra）
    Loader {
        Layout.fillWidth: true
        active: root.headerExtra !== null
        sourceComponent: root.headerExtra
    }
}
