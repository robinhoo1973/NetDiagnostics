// =============================================================================
// DashboardScreen.qml — PageDisplay 子类装配（page-dashboard.md §3）
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import core
import sections as S
import theme

PageDisplay {
    id: page
    objectName: "dashboard"

    // Dashboard 特定组件（经属性注入 —— UI-5：Common 不依赖页面模块）
    Component { id: dashboardSummaryComp; DashboardSummaryComp { } }
    Component { id: dashboardRowHeaderComp; DashboardRowHeader { } }

    readonly property bool hasData: AppState.hasData

    // _activeGroups 缓存（UI-2：绑定不调 visibleGroups()）
    property var _activeGroups: []
    function _refreshGroups() {
        _activeGroups = AppState.visibleGroups()
    }
    Connections {
        target: AppState
        function onStateVersionChanged() { page._refreshGroups() }
        function onRunStatusChanged() { page._refreshGroups() }
    }
    Component.onCompleted: _refreshGroups()

    // ── Toast（NEW-7：页面自持）──
    property string toastText: ""
    function showToast(msg) {
        toastText = msg
        toastTimer.restart()
    }
    Timer {
        id: toastTimer
        interval: ThemeEngine.toastDurationMs
        onTriggered: page.toastText = ""
    }

    function dashboardOpenDetail(diagId) {
        // UI-10：经 sectionAction 路由，AppContent 推入 DetailPage
        page.emitSectionAction("dashboard", "openDetail", { diagId: diagId })
    }

    headerContent: [
        S.PageHeaderSection { iconName: "dashboard"; title: T.tr("dashboard") }
    ]

    bodyContent: [
        S.PageEmptyStateSection { },
        S.PageStatusHeaderSection {
            headerExtra: dashboardSummaryComp
            onShareRequested: {
                AppState.copyReportToClipboard()
                page.showToast(T.tr("detailCopied"))
            }
        },
        Repeater {
            model: page._activeGroups
            delegate: S.PageGroupPanelSection {
                Layout.fillWidth: true
                active: page.hasData            // NEW-10：无数据时整组行隐藏
                groupIndex: modelData
                showOnlyCompleted: true
                compactTiles: true
                rowHeaderDelegate: dashboardRowHeaderComp
                onDetailRequested: function(d) { page.dashboardOpenDetail(d.diagId) }
            }
        }
    ]

    floatingContent: [
        S.PageToastSection { toastText: page.toastText }
    ]

    onSectionAction: function(scope, action, payload) {
        if (action === "share" || action === "previewShare") {
            AppState.copyReportToClipboard()
            page.showToast(T.tr("detailCopied"))
        }
    }
}
