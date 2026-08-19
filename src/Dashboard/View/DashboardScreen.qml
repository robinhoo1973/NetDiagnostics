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
// 5WHY (复核 2026-08-19 缺失回归): assignIfChanged 曾在无 W 导入下被调用——
// 每次刷新抛 ReferenceError、_activeGroups 永不更新（面板墙空屏）。补与
// 同目录 DashboardSummaryComp 相同的导入深度。
import "../../widgets/StatsUtil.js" as W

PageDisplay {
    id: page
    objectName: "dashboard"

    // Dashboard 特定组件（经属性注入 —— UI-5：Common 不依赖页面模块）
    // 5WHY (2026-08-19): dashboardSummaryComp 声明后从未被任何 Loader/
    // createObject 消费（摘要卡直接实例化 DashboardSummaryComp）——删除。
    Component { id: dashboardRowHeaderComp; DashboardRowHeader { } }

    readonly property bool hasData: AppState.hasData

    // _activeGroups 缓存（UI-2：绑定不调 visibleGroups()）
    // 5WHY (复核 2026-08-19 效率): 每次事件都赋新数组身份 → Repeater 全量
    // 销毁重建 5 面板 + ~40 瓦片（连隐藏屏也重建）。内容比较：只有组集合
    // 实际变化才替换（可见组仅在运行边界/激活组变更时变化）。
    property var _activeGroups: []
    function _refreshGroups() {
        // 5WHY (复核 2026-08-19 效率): 数组身份门控经 StatsUtil.assignIfChanged
        // 收敛——内容未变不替换身份（Repeater 不重建面板/瓦片墙）。
        _activeGroups = W.assignIfChanged(_activeGroups, AppState.visibleGroups())
    }
    // 5WHY (复核 2026-08-19 效率 + 揭示自愈): StackView 隐藏页不销毁——信号
    // 处理在离屏页上照跑（空耗 ~60% 事件工作量）。可见才刷新；重新可见时
    // 补一次全量刷新（自愈隐藏期间的任何遗漏事件）。
    onVisibleChanged: if (visible) { _refreshGroups(); _refreshTime() }
    // 总耗时（Run Info 卡；命令式刷新 UI-2）
    property string _timeText: "—"
    // 归档布局迁移：完成时间（命令式刷新）
    property string _completedAt: ""
    // 5WHY (复核 2026-08-19 单一归属): _layers 与 5×groupStats 循环随摘要卡
    // 合并迁入 DashboardSummaryComp._refresh()——本屏不再为摘要卡扫描；
    // _refreshTime 只维护 Run Info 卡的墙钟（runDurationMs 零扫描）。
    function _refreshTime() {
        // 8-15：总耗时改用墙钟（runDurationMs），与诊断运行时间一致
        _timeText = ThemeEngine.formatDuration(AppState.runDurationMs())
    }
    Connections {
        target: AppState
        // 5WHY (复核 2026-08-18 漏接的第四消费方): 语义信号轮次给三个
        // 区块补了刷新触发，却漏了同屏的 Run Info 卡——_refreshTime() 读
        // groupStats(-1)/groupStats(i)（按 scheme 过滤），换 scheme 后
        // 总诊断数/已完成/分层时长停留旧计数，与相邻摘要卡数字矛盾。
        // filteredDataChanged 一次驱动组列表 + 时间卡。
        // 5WHY (复核 2026-08-19): enabled 门控取代逐处理器 if（离屏整体
        // 禁用）；重新可见由 onVisibleChanged 补刷新。
        enabled: page.visible
        function onFilteredDataChanged() { page._refreshTime(); page._refreshGroups() }
        function onProgressChanged() { page._refreshTime(); page._refreshGroups() }
        function onRunElapsedChanged() { page._refreshTime() }
        function onCurrentRunningGroupChanged() { page._refreshGroups() }
    }
    // 5WHY (复核 2026-08-19 单处处理): runStatusChanged 曾拆在两个块
    // （门控块刷新 + 未门控块戳章）——同一信号两处处理难保序。合并到
    // 未门控块：runStatus 只在运行边界变化（每轮至多次），离屏执行成本
    // 可忽略；完成时刻戳不依赖可见性（揭示时需已定格的值）。
    Connections {
        target: AppState
        function onRunStatusChanged() {
            if (AppState.runStatus === 2) {
                var now = new Date()
                page._completedAt = ("0" + now.getHours()).slice(-2) + ":"
                    + ("0" + now.getMinutes()).slice(-2) + ":"
                    + ("0" + now.getSeconds()).slice(-2)
            }
            page._refreshTime(); page._refreshGroups()
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
            screenVisible: page.visible       // 5WHY 2026-08-19：离屏停扫
            hintText: T.tr("runFromDiag")     // M2：新用户引导
            // 5WHY (复核 2026-08-19): 本屏无状态头——零结果终态 2/3 由空态呈现
            includeTerminalEmpty: true
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
                        // 5WHY (复核 2026-08-18 用户诉求 "孤立成功图标" 平行站点):
                        // 状态头修复后此处仍 `runStatus !== 1`——Idle 或 Completed
                        // 态走回退 "check"+成功绿，与状态头同款孤立绿勾。统一为
                        // 终态(2/3/4)才显示状态图标（runStatusInfo 全 5 态表）。
                        visible: ThemeEngine.isTerminalRunStatus(AppState.runStatus)
                        anchors.fill: parent
                        name: (page._runInfo ? page._runInfo.iconName : "check")
                        size: 28
                        color: (page._runInfo ? page._runInfo.color : ThemeEngine.colors.success)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label {
                        text: {
                            if (AppState.runStatus === 1) return T.tr("runningDots")
                            // 5WHY (复核 2026-08-18): runStatusInfo 的 Completed(2)
                            // 表项 labelKey 为空——保持 diagRunComplete 文案。
                            return (page._runInfo && page._runInfo.labelKey)
                                ? T.tr(page._runInfo.labelKey) : T.tr("diagRunComplete")
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

        // ── 摘要统计卡（5WHY 2026-08-19 用户诉求 "两个 Summary 合一"）──
        // 原 "Summary+Total 头+7 结果行" 与 "总览卡（3 统计+分层计时）"
        // 两张卡合并为单卡：聚合统计 → 7 类结果行（零计数隐藏）→ 分层计时。
        S.PageCardSection {
            active: page.hasData
            cardTitle: T.tr("summary")
            bottomMargin: ThemeEngine.spacing.sm
            DashboardSummaryComp {
                Layout.fillWidth: true
                screenVisible: page.visible   // 5WHY 2026-08-19：离屏停扫
            }
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
                screenVisible: page.visible     // 5WHY 2026-08-19：离屏面板跳过刷新
                viewportItem: page.contentFlickable   // 滚动视口门控
                onDetailRequested: function(d) { page.dashboardOpenDetail(d.diagId) }
            }
        },

        // 总览卡已并入上方摘要统计卡（5WHY 2026-08-19 两个 Summary 合一）

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
                                // 5WHY (review round 4): hovered 别名取代叠加 NoButton
                                // MouseArea 的脆弱路由；关闭收敛为 closeOverlay()
                                color: iconBtn.hovered ? Qt.alpha(ThemeEngine.colors.fail, 0.35)
                                                       : Qt.alpha(ThemeEngine.colors.fail, 0.15)
                                IconActionButton {
                                    id: iconBtn
                                    anchors.fill: parent
                                    iconName: "close"; iconSize: 14
                                    iconColor: ThemeEngine.colors.fail
                                    Accessible.name: T.tr("dialogCancel")
                                    onActivated: page.closeOverlay()
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
                                fillMode: Image.PreserveAspectFit
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

    // SummaryStat 组件已随摘要卡合并上移 DashboardSummaryComp（5WHY 2026-08-19）
}
