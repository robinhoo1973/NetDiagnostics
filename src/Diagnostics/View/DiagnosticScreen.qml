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
import dialogs
// 5WHY (复核 2026-08-19 深度回归): ../widgets/ 曾少一级——相对导入按文档
// URL 目录解析：qrc:/qt/qml/Diagnostics/View/ 的 ../ 是 Diagnostics/（未注册
// 的 qrc 路径），编译期导入失败沿通用崩溃链炸掉整文档。正确深度与
// DashboardSummaryComp 同为 ../../widgets/。
import "../../widgets/StatsUtil.js" as W   // qrc:/qt/qml/Diagnostics/View/ → ../../widgets/

PageDisplay {
    id: page
    objectName: "diagnostic"

    // visibleGroups：仅激活组（UI-2：命令式刷新，绑定不调 visibleGroups()）
    // 5WHY (复核 2026-08-19 效率): 内容比较——组集合未变不替换数组身份，
    // 避免 Repeater 全量销毁重建面板/瓦片墙（同 DashboardScreen）。
    property var _groups: []
    function _refreshGroups() {
        // 5WHY (复核 2026-08-19 效率): 数组身份门控经 StatsUtil.assignIfChanged
        // 收敛——内容未变不替换身份（Repeater 不重建面板/瓦片墙）。
        _groups = W.assignIfChanged(_groups, AppState.visibleGroups())
    }
    // 5WHY (复核 2026-08-19 效率 + 揭示自愈): 离屏（切到 Dashboard/Config）
    // 时跳过信号扫描；重新可见时全量补刷新，自愈隐藏期间的遗漏事件。
    onVisibleChanged: if (visible) _refreshGroups()
    Connections {
        target: AppState
        // 5WHY (复核 2026-08-18 语义信号): visibleGroups 按激活组过滤——
        // 激活组变更经 filteredDataChanged 刷新（旧接 stateVersionChanged：
        // 语言/凭据等无关 bump 也误触发）。
        // 5WHY (复核 2026-08-19): enabled 门控取代逐处理器 if（离屏整体
        // 禁用）；重新可见由 onVisibleChanged 补刷新。
        enabled: page.visible
        function onFilteredDataChanged() { page._refreshGroups() }
        function onRunStatusChanged() { page._refreshGroups() }
        // 8-15：渐进呈现
        function onCurrentRunningGroupChanged() { page._refreshGroups() }
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
    // OverlayHost 契约（5WHY simplify 2026-08-17）：浮层页面统一实现
    // closeOverlay()，AppContent 单一入口关闭，不再探测页面私有状态。
    // 5WHY (review round 3 修复)：此前仅关详情 sheet——cellular 警告浮层
    // 打开时 dock 点击被 navBlocked 吞掉且什么也不关闭（死点击）；
    // 警告走 dismiss 语义（不确认不启动），与遮罩点击一致。
    function closeOverlay() {
        if (page._detailVisible) page._detailVisible = false
        else if (AppState.cellularWarnVisible) AppState.dismissCellularWarn()
    }

    headerContent: [
        S.PageHeaderSection { title: T.tr("appName"); iconName: "compass" },
        S.PageToolbarSection {
            onRunRequested: AppState.runDiagnostics()
            onCancelRequested: AppState.cancel()
        }
    ]

    bodyContent: [
        S.PageStatusHeaderSection {
            screenVisible: page.visible   // 5WHY 2026-08-19：离屏停扫
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
                screenVisible: page.visible   // 5WHY 2026-08-19：离屏面板跳过刷新
                viewportItem: page.contentFlickable   // 滚动视口门控
                onDetailRequested: function(d) {
                    // 8-18：双保险——未完成的检测不允许激活详情页（瓦片层已禁用
                    // MouseArea，此处再按 isPending 拦截一次）。
                    if (d && d.isPending === true) return
                    page._detail = AppState.resultFor(d.diagId)
                    page._detailVisible = page._detail !== null && Object.keys(page._detail).length > 0
                    if (!page._detailVisible)
                        page.showToast(T.tr("reportRunFirst"))
                }
            }
        },
        S.PageEmptyStateSection { screenVisible: page.visible; errorState: AppState.runStatus === 4 }
    ]

    floatingContent: [
        S.PageToastSection { toastText: page.toastText },
        S.PageOverlaySection {
            visible: page._detailVisible
            onVisibleChanged: if (!visible) page._detailVisible = false
            onCloseRequested: page._detailVisible = false   // 遮罩点击关闭（绑定不破）
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
            // 5WHY (review round 3): 遮罩点击仅关闭警告（dismiss 语义）——
            // 误触遮罩不得触发 continueAfterCellularWarn 的整轮大流量启动
            onCloseRequested: AppState.dismissCellularWarn()
            // 5WHY (2026-08-19 用户诉求 "流量弹窗与整体界面不匹配"): 全仓浮层
            // 均已按 M3 模式（图标垫 + 居中标题/正文 + 等宽主次按钮）设计，
            // 唯此弹窗仍用默认 QQC2 控件裸拼——字号/配色/按钮样式全部脱离
            // 设计系统。补齐同一模式。
            // 5WHY (复核 2026-08-19 壳抽取): 卡片镀铬（双向钳制/radius/底色/
            // 边框/边距/RTL 镜像）已与 PremiumDialog 同源收敛进 DialogCard。
            DialogCard {
                maxWidth: 400
                // 身份图标：蜂窝数据检测项图标（IconPad 光晕垫，tint 自
                // iconName 派生——与 PremiumDialog 的 zap 图标同一位置规格）
                IconPad {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    iconName: "nd-diag-g1-cellular"
                    iconSize: 36
                    iconColor: ThemeEngine.colors.iconInk
                }
                Label {
                    Layout.fillWidth: true
                    text: T.tr("cellularWarnTitle")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.title
                    font.weight: Font.Bold
                    color: ThemeEngine.colors.onSurface
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    text: T.tr("cellularWarnBody")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.body
                    color: ThemeEngine.colors.onSurfaceVariant
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
                // M3 对话框惯例：次要动作（取消）弱化、主动作（继续）
                // 填充强调，两钮等宽。5WHY (复核 2026-08-20): 操作行入
                // DialogCard footer 槽——高钳制滚屏时钉底恒可见（主操作
                // 不再被滚出首屏）。
                footer: RowLayout {
                    Layout.fillWidth: true
                    spacing: ThemeEngine.spacing.sm
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        text: T.tr("cellularCancel")
                        flat: true
                        onClicked: AppState.cancel()
                    }
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        text: T.tr("cellularContinue")
                        onClicked: AppState.continueAfterCellularWarn()
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
