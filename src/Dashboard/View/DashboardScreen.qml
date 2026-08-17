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
import widgets

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
    // 总耗时（Run Info 卡；命令式刷新 UI-2）
    property string _timeText: "—"
    // 归档布局迁移：完成时间 + 总览卡数据（命令式刷新）
    property string _completedAt: ""
    property int _totDiags: 0
    property int _totCompleted: 0
    property var _layers: []
    function _refreshTime() {
        var s = AppState.groupStats(-1)
        // 8-15：总耗时改用墙钟（runDurationMs），与诊断运行时间一致
        _timeText = ThemeEngine.formatDuration(AppState.runDurationMs())
        _totDiags = s.total || 0
        _totCompleted = s.completed || 0
        var layers = []
        for (var i = 0; i < 5; ++i) {
            var gs = AppState.groupStats(i)
            layers.push({ index: i, ms: gs.durationMs || 0 })
        }
        _layers = layers
    }
    Connections {
        target: AppState
        function onStateVersionChanged() { page._refreshGroups() }
        function onProgressChanged() { page._refreshTime(); page._refreshGroups() }
        function onRunElapsedChanged() { page._refreshTime() }
        function onCurrentRunningGroupChanged() { page._refreshGroups() }
        function onRunStatusChanged() {
            page._refreshTime(); page._refreshGroups()
            if (AppState.runStatus === 2) {
                var now = new Date()
                page._completedAt = ("0" + now.getHours()).slice(-2) + ":"
                    + ("0" + now.getMinutes()).slice(-2) + ":"
                    + ("0" + now.getSeconds()).slice(-2)
            }
        }
    }
    Component.onCompleted: {
        _refreshGroups()
        _refreshTime()
    }

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

    // ── 报告预览浮层（ReportEngine 图片回退 + 分享按钮）──
    property bool previewVisible: false
    property string previewImagePath: ""
    property int _previewGen: 0
    overlayVisible: previewVisible
    // OverlayHost 契约（5WHY simplify 2026-08-17）：见 DiagnosticScreen
    function closeOverlay() { page.previewVisible = false }

    // 运行状态呈现单次求值（5WHY review round 3 修复：标题色绑定曾一行内
    // 两次调用 runStatusInfo——同一表达式重复构造状态对象）
    readonly property var _runInfo: ThemeEngine.runStatusInfo(AppState.runStatus)
    // 预览关闭钮尺寸（5WHY review round 4: 曾声明在浮层内嵌对象里——
    // readonly 属性嵌套声明违反 B.2 根级规则）
    readonly property int _closeBtnSz: ThemeEngine.isMobile ? 48 : 34

    function openPreview() {
        if (!page.hasData) return
        previewVisible = true
        previewImagePath = ""
        _previewGen++
        var gen = _previewGen
        // 5WHY：buildReportHtml + renderPreviewImage 同步重渲染会卡 UI——
        // callLater 分帧 + generation 防抖（只最新请求生效）。
        Qt.callLater(function() {
            if (gen !== page._previewGen || !page.previewVisible) return
            var imgPath = AppState.renderPreviewImage(ThemeEngine.isMobile ? 480 : 960)
            page.previewImagePath = imgPath || ""
        })
    }

    headerContent: [
        S.PageHeaderSection {
            iconName: "dashboard"
            title: T.tr("dashboard")
            // 8-15：移除右上角 file-html 报告预览入口——与主窗口关闭钮同位
            // 易误触，且预览入口已由底部"Review Report"卡片承担。
        }
    ]

    bodyContent: [
        S.PageEmptyStateSection {
            hintText: T.tr("runFromDiag")     // M2：新用户引导
        },

        // ── Run Info 卡（归档恢复：状态 + 目标 + 总耗时）──
        S.PageCardSection {
            active: page.hasData
            showHeader: false
            bottomMargin: ThemeEngine.spacing.sm
            RowLayout {
                Layout.fillWidth: true
                spacing: ThemeEngine.spacing.md
                Item {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    BusyIndicator {
                        visible: AppState.runStatus === 1
                        anchors.fill: parent
                        running: AppState.runStatus === 1
                    }
                    AppIcon {
                        visible: AppState.runStatus !== 1
                        anchors.fill: parent
                        name: ThemeEngine.runStatusIcon(AppState.runStatus, "check")
                        size: 28
                        color: ThemeEngine.runStatusColor(AppState.runStatus,
                                                          ThemeEngine.colors.success)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label {
                        text: {
                            if (AppState.runStatus === 1) return T.tr("runningDots")
                            var info = ThemeEngine.runStatusInfo(AppState.runStatus)
                            return info ? T.tr(info.labelKey) : T.tr("diagRunComplete")
                        }
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.title
                        font.weight: Font.DemiBold
                        color: (page._runInfo && page._runInfo.dimmed)
                             ? ThemeEngine.colors.onSurfaceVariant
                             : ThemeEngine.colors.onSurface
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        spacing: 4
                        AppIcon { name: "compass"; size: 12; color: ThemeEngine.colors.textMuted }
                        Label {
                            text: T.tr("targetLabel") + (AppState.targetHost + AppState.targetPath || T.tr("naLabel"))
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: ThemeEngine.fontSize.caption
                            color: ThemeEngine.colors.onSurfaceVariant
                            elide: Text.ElideRight
                        }
                    }
                    RowLayout {
                        spacing: 4
                        AppIcon { name: "activity"; size: 12; color: ThemeEngine.colors.textMuted }
                        Label {
                            // 归档：完成时刻（运行中显示总耗时）
                            text: AppState.runStatus === 2 && page._completedAt !== ""
                                  ? page._completedAt : T.tr("totalTimeLabel") + ": " + page._timeText
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: ThemeEngine.fontSize.caption
                            color: ThemeEngine.colors.onSurfaceVariant
                        }
                    }
                }
            }
        },

        // ── 摘要统计卡（归档 SummaryCards：Summary + Total + 6 类结果行）──
        S.PageCardSection {
            active: page.hasData
            showHeader: false
            bottomMargin: ThemeEngine.spacing.sm
            DashboardSummaryComp { Layout.fillWidth: true }
        },

        // ── 分组结果标签（归档恢复）──
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: ThemeEngine.spacing.md
            Layout.topMargin: ThemeEngine.spacing.md
            visible: page.hasData
            text: T.tr("perGroup")
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.subhead
            font.weight: Font.DemiBold
            color: ThemeEngine.colors.onSurface
            elide: Text.ElideRight
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
        },

        // ── 总览卡（归档：3 项统计 + 分层计时）──
        S.PageCardSection {
            active: page.hasData
            cardTitle: T.tr("summary")
            bottomMargin: ThemeEngine.spacing.sm
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                SummaryStat {
                    Layout.fillWidth: true
                    statIcon: "badge-check"; clr: ThemeEngine.colors.tertiary
                    statVal: String(page._totDiags); lbl: T.tr("totalDiagsLabel")
                }
                SummaryStat {
                    Layout.fillWidth: true
                    statIcon: "activity"; clr: ThemeEngine.colors.secondary
                    statVal: page._timeText; lbl: T.tr("totalTimeLabel")
                }
                SummaryStat {
                    Layout.fillWidth: true
                    statIcon: "check"; clr: ThemeEngine.colors.success
                    statVal: String(page._totCompleted); lbl: T.tr("completedLabel")
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: ThemeEngine.colors.outlineVariant
                    visible: page._totCompleted > 0
                }
                Item { Layout.preferredHeight: 4; visible: page._totCompleted > 0 }
                Label {
                    visible: page._totCompleted > 0
                    text: T.tr("layerTimings")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.caption
                    font.weight: Font.DemiBold
                    color: ThemeEngine.colors.onSurfaceVariant
                }
                Repeater {
                    model: page._layers
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        visible: page._totCompleted > 0
                        spacing: ThemeEngine.spacing.sm
                        AppIcon {
                            name: ThemeEngine.groupIconName(modelData.index)
                            size: 14
                            color: ThemeEngine.groupHue(modelData.index)
                        }
                        Label {
                            Layout.fillWidth: true
                            text: T.groupName(modelData.index)
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: ThemeEngine.fontSize.caption
                            color: ThemeEngine.colors.onSurface
                            elide: Text.ElideRight
                        }
                        Label {
                            text: ThemeEngine.formatDuration(modelData.ms || 0)
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: ThemeEngine.fontSize.caption
                            color: ThemeEngine.colors.onSurfaceVariant
                        }
                    }
                }
            }
        },

        // ── 报告预览卡（归档：标题 + 提示 + 主色按钮）──
        S.PageCardSection {
            active: page.hasData && AppState.runStatus !== 1
            cardTitle: T.tr("report")
            bottomMargin: ThemeEngine.spacing.lg
            ColumnLayout {
                Layout.fillWidth: true
                spacing: ThemeEngine.spacing.md
                Label {
                    Layout.fillWidth: true
                    text: T.tr("reportExportHint")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.body
                    color: ThemeEngine.colors.onSurfaceVariant
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 48
                    radius: 10
                    color: Qt.alpha(ThemeEngine.colors.tertiary, 0.10)
                    border { width: 1; color: Qt.alpha(ThemeEngine.colors.tertiary, 0.35) }
                    RowLayout {
                        anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                        AppIcon { name: "file-html"; size: 18; color: ThemeEngine.colors.tertiary }
                        Item { width: 12 }
                        Label {
                            Layout.fillWidth: true
                            text: T.tr("reportReviewBtn")
                            color: ThemeEngine.colors.onSurface
                            font.family: ThemeEngine.fontUi
                            font.pixelSize: ThemeEngine.fontSize.body
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: page.openPreview()
                    }
                }
            }
        }
    ]

    floatingContent: [
        S.PageToastSection { toastText: page.toastText },
        // 报告预览浮层（归档恢复：全窗 + 头部条 + 缩放 + 双格式分享）
        S.PageOverlaySection {
            visible: page.previewVisible
            onCloseRequested: page.previewVisible = false   // 遮罩点击关闭（绑定不破）
            Rectangle {
                anchors { fill: parent; margins: ThemeEngine.isMobile ? 0 : 8 }
                radius: ThemeEngine.isMobile ? 0 : 12
                color: ThemeEngine.colors.surfaceContainerLow
                clip: true
                border { width: ThemeEngine.isMobile ? 0 : 2; color: ThemeEngine.colors.primary }
                ColumnLayout {
                    anchors { fill: parent; margins: 12 }
                    spacing: 10
                    // 头部条
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        radius: 8
                        color: Qt.alpha(ThemeEngine.colors.tertiary, 0.08)
                        RowLayout {
                            anchors { fill: parent; margins: 8 }
                            AppIcon { name: "file-html"; size: 20; color: ThemeEngine.colors.tertiary }
                            Item { width: 8 }
                            Label {
                                Layout.fillWidth: true
                                text: T.tr("reportReviewBtn")
                                font.family: ThemeEngine.fontUi
                                font.pixelSize: ThemeEngine.fontSize.subhead
                                font.weight: Font.Bold
                                color: ThemeEngine.colors.onSurface
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                id: closeBtn
                                implicitWidth: page._closeBtnSz; implicitHeight: page._closeBtnSz; radius: page._closeBtnSz / 2
                                color: closeHover.containsMouse ? Qt.alpha(ThemeEngine.colors.fail, 0.35)
                                                                : Qt.alpha(ThemeEngine.colors.fail, 0.15)
                                // 5WHY (review round 3): 此处曾是手写按钮脚手架的第 4 份
                                // 拷贝（无键盘支持）——命中/键盘/a11y 统一走 IconActionButton，
                                // 圆形底色由外层 Rectangle 提供
                                IconActionButton {
                                    anchors.fill: parent
                                    iconName: "close"; iconSize: 14
                                    iconColor: ThemeEngine.colors.fail
                                    Accessible.name: T.tr("dialogCancel")
                                    onActivated: page.previewVisible = false
                                }
                                MouseArea {
                                    id: closeHover
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton   // 仅驱动悬停态
                                }
                            }
                        }
                    }
                    // 可缩放图片区
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 8
                        clip: true
                        color: ThemeEngine.colors.surface
                        border { width: 1; color: ThemeEngine.colors.outlineVariant }
                        Flickable {
                            id: previewFlick
                            anchors { fill: parent; margins: 14 }
                            clip: true
                            contentWidth: previewImg.width * previewFlick.previewScale
                            contentHeight: previewImg.height * previewFlick.previewScale
                            property real previewScale: 1.0
                            Image {
                                id: previewImg
                                x: Math.max(0, (previewFlick.width - width * previewFlick.previewScale) / 2)
                                y: Math.max(0, (previewFlick.height - height * previewFlick.previewScale) / 2)
                                width: sourceSize.width * previewFlick.previewScale
                                height: sourceSize.height * previewFlick.previewScale
                                source: page.previewImagePath !== "" ? "file://" + page.previewImagePath : ""
                                fillMode: Image.PreserveFit
                                cache: false
                                smooth: true
                            }
                        }
                        ZoomBar {
                            anchors { bottom: parent.bottom; right: parent.right; margins: 8 }
                            zoomLevel: previewFlick.previewScale
                            onZoomLevelChanged: previewFlick.previewScale = zoomLevel
                        }
                    }
                    // 分享按钮（labeled）
                    ShareButtons {
                        Layout.fillWidth: true
                        mode: "labeled"
                        pdfAccent: ThemeEngine.colors.tertiary
                        htmlAccent: ThemeEngine.colors.primary
                        onShareRequested: function(format) {
                            if (format === "locked") { page.showToast(T.tr("premiumRequiredMsg")); return }
                            AppState.shareReportFile(format)
                            page.showToast(T.tr("reportCopied"))
                        }
                        onPremiumRequired: page.showToast(T.tr("premiumRequiredMsg"))
                    }
                }
            }
        }
    ]

    onSectionAction: function(scope, action, payload) {
        if (action === "share" || action === "previewShare") {
            AppState.shareReportFile("text")
            page.showToast(T.tr("reportCopied"))
        }
        if (action === "preview") page.openPreview()
    }

    // ── 归档 SummaryStat（图标 + 标签 + 大数值）──
    component SummaryStat: RowLayout {
        property string statIcon: ""
        property color clr: ThemeEngine.colors.tertiary
        property string statVal: ""
        property string lbl: ""
        spacing: 10
        AppIcon {
            name: statIcon
            size: 16
            color: clr
            Layout.alignment: Qt.AlignVCenter
        }
        Label {
            Layout.fillWidth: true
            text: lbl
            font.family: ThemeEngine.monoFont
            font.pixelSize: ThemeEngine.fontSize.caption
            color: ThemeEngine.colors.onSurfaceVariant
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        Label {
            text: statVal
            font.family: ThemeEngine.monoFont
            font.pixelSize: ThemeEngine.fontSize.subhead
            font.weight: Font.Bold
            color: clr
            verticalAlignment: Text.AlignVCenter
        }
    }
}
