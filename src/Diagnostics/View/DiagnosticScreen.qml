// =============================================================================
// DiagnosticScreen.qml — PageDisplay 子类装配（page-diagnostic.md §3）
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import core
import sections as S
import widgets
import theme

PageDisplay {
    id: page
    objectName: "diagnostic"

    // visibleGroups：仅激活组（UI-2：命令式刷新，绑定不调 visibleGroups()）
    property var _groups: []
    function _refreshGroups() { _groups = AppState.visibleGroups() }
    Connections {
        target: AppState
        function onStateVersionChanged() { page._refreshGroups() }
        function onRunStatusChanged() { page._refreshGroups() }
    }
    Component.onCompleted: _refreshGroups()

    // ── Toast 状态（NEW-7：页面自持）──
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

    // ── 详情浮层状态（P0：内联展示；完整 DetailPage 后续里程碑接入）──
    property var _detail: null
    property bool _detailVisible: false
    overlayVisible: _detailVisible

    headerContent: [
        S.PageHeaderSection { title: T.tr("appName"); iconName: "compass" },
        S.PageToolbarSection {
            onRunRequested: AppState.runDiagnostics()
            onCancelRequested: AppState.cancel()
        }
    ]

    bodyContent: [
        S.PageStatusHeaderSection {
            onShareRequested: {
                AppState.copyReportToClipboard()
                page.showToast(T.tr("detailCopied"))
            }
        },
        Repeater {
            model: page._groups
            delegate: S.PageGroupPanelSection {
                Layout.fillWidth: true
                groupIndex: modelData
                onDetailRequested: function(d) {
                    page._detail = AppState.resultFor(d.diagId)
                    page._detailVisible = page._detail !== null && Object.keys(page._detail).length > 0
                    if (!page._detailVisible)
                        page.showToast(T.tr("reportRunFirst"))
                }
            }
        },
        S.PageEmptyStateSection { }
    ]

    floatingContent: [
        S.PageToastSection { toastText: page.toastText },
        S.PageOverlaySection {
            visible: page._detailVisible
            onVisibleChanged: if (!visible) page._detailVisible = false
            PageDetailSheet {
                anchors.centerIn: parent
                width: Math.min(parent.width - 32, 480)
                detailData: page._detail
                onBackRequested: page._detailVisible = false
            }
        }
    ]

    onSectionAction: function(scope, action, payload) {
        if (action === "run") AppState.runDiagnostics()
        else if (action === "cancel") AppState.cancel()
    }
}
