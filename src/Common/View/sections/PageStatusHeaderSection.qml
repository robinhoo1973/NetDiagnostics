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

    // UI-2：聚合统计命令式刷新（progressChanged/runStatusChanged 处理器赋值），
    // 绑定中不调用 groupStats(-1)。
    property var _agg: ({ pass: 0, warn: 0, fail: 0, skip: 0, info: 0, error: 0 })
    function _refreshAgg() {
        var s = AppState.groupStats(-1)
        _agg = { pass: s.pass || 0, warn: s.warn || 0, fail: s.fail || 0,
                 skip: s.skip || 0, info: s.info || 0, error: s.error || 0 }
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
                name: "badge-check"; size: 16
                color: ThemeEngine.colors.passGreen
            }
        }
        Label {
            text: AppState.runStatus === 1 ? (AppState.currentDiagLabel || T.tr("running"))
                                           : AppState.totalCompleted + " " + T.tr("completed")
            color: ThemeEngine.colors.textSecondary
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
        AppIcon {
            name: "clipboard"
            size: 18
            color: ThemeEngine.colors.textSecondary
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.shareRequested("text")
            }
            Accessible.role: Accessible.Button
            Accessible.name: T.tr("shareBtn")
        }
    }

    // 注入的摘要层（Dashboard headerExtra）
    Loader {
        Layout.fillWidth: true
        active: root.headerExtra !== null
        sourceComponent: root.headerExtra
    }
}
