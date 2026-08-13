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
        function onCurrentRunningGroupChanged() { page._refreshGroups() }   // 8-15：渐进呈现
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
    // NEW-8 + H1：浮层（详情 sheet / cellular 警告）打开时阻断导航
    overlayVisible: _detailVisible || AppState.cellularWarnVisible

    headerContent: [
        S.PageHeaderSection { title: T.tr("appName"); iconName: "compass" },
        S.PageToolbarSection {
            onRunRequested: AppState.runDiagnostics()
            onCancelRequested: AppState.cancel()
        }
    ]

    bodyContent: [
        S.PageStatusHeaderSection {
            onShareRequested: function(format) {
                if (format === "locked") {
                    page.showToast(T.tr("premiumRequiredMsg"))
                    return
                }
                AppState.shareReportFile(format)
                page.showToast(T.tr("reportCopied"))
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
        S.PageEmptyStateSection { errorState: AppState.runStatus === 4 }
    ]

    floatingContent: [
        S.PageToastSection { toastText: page.toastText },
        S.PageOverlaySection {
            visible: page._detailVisible
            onVisibleChanged: if (!visible) page._detailVisible = false
            PageDetailSheet {
                // 7-5：详情铺满整个主窗口（原居中 480px 卡片改为全窗）
                anchors.fill: parent
                detailData: page._detail
                onBackRequested: page._detailVisible = false
            }
        },
        // H1（page-diagnostic §3）：移动数据警告——G3 起大流量探测前暂停确认
        S.PageOverlaySection {
            visible: AppState.cellularWarnVisible
            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 400)
                height: warnCol.implicitHeight + 40
                radius: ThemeEngine.radius.xl
                color: ThemeEngine.colors.card
                border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout {
                    id: warnCol
                    anchors { fill: parent; leftMargin: 20; rightMargin: 20; topMargin: 20; bottomMargin: 20 }
                    spacing: ThemeEngine.spacing.md
                    Label {
                        Layout.fillWidth: true
                        text: T.tr("cellularWarnTitle")
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.subhead
                        font.weight: Font.Bold
                        color: ThemeEngine.colors.textPrimary
                    }
                    Label {
                        Layout.fillWidth: true
                        text: T.tr("cellularWarnBody")
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.body
                        color: ThemeEngine.colors.textSecondary
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: ThemeEngine.spacing.sm
                        Button {
                            text: T.tr("cellularCancel")
                            onClicked: AppState.cancel()
                        }
                        Button {
                            text: T.tr("cellularContinue")
                            onClicked: AppState.continueAfterCellularWarn()
                        }
                    }
                }
            }
        }
    ]

    onSectionAction: function(scope, action, payload) {
        if (action === "run") AppState.runDiagnostics()
        else if (action === "cancel") AppState.cancel()
    }
}
