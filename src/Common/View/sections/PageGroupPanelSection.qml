// PageGroupPanelSection.qml — 组面板（§2.4 核心：组头 + 进度 + 瓦片墙）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import theme
import widgets
import "../widgets/StatsUtil.js" as W   // 直接 JS 导入（qmldir 模块目录不可相对导入）
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    bottomMargin: ThemeEngine.spacing.sm

    property int groupIndex: 0
    signal detailRequested(var data)
    // 5WHY (复核 2026-08-19 效率): StackView 隐藏屏不销毁面板——离屏时每个
    // 信号仍驱动 5 面板 × (list 重建 + groupStats 扫描)（事件总工作量的
    // ~60% 空转）。屏幕注入可见性：Connections.enabled 整体禁用（与
    // StatsBridge/两屏同一机制），重新可见无条件 _reload 补刷（面板只
    // 在屏可见时创建，创建期成本即该量级）。默认 true 保持独立使用兼容。
    // 5WHY (复核 2026-08-19 viewport 门控): 屏幕 Flickable 下传瓦片——
    // 滚动出视口的运行瓦片停动画（与 screenVisible 两级门控）。
    property var viewportItem: null
    onScreenVisibleChanged: if (screenVisible) _reload()

    // Dashboard 行变体注入（UI-5：依赖方向页面→Common）
    property Component rowHeaderDelegate: null   // 行头徽标（DashboardRowHeader 注入）
    property bool showOnlyCompleted: false       // Dashboard：结束后只显示已完成项
    property bool compactTiles: ThemeEngine.isMobile

    property var itemsModel: []
    // 5WHY (复核 2026-08-18): _modelVersion 只写不读（Repeater 绑定 itemsModel
    // 身份而非版本 key）——_activeKey 签名门控已完全取代它，删除。
    // 5WHY (复核 2026-08-19 单写收敛): _total/_completed 曾与 _statsObj 双写
    // （命令式三赋值）——仅剩路径不写其一时静默分叉（X/Y 与徽标矛盾）。
    // 归一化结果经 _statsObj 身份替换驱动，typed 只读绑定消费同一来源。
    readonly property int _total: _statsObj.total
    readonly property int _completed: _statsObj.completed
    property bool _userToggled: false
    property bool _userExpanded: true
    // 5WHY (复核 2026-08-19 单层直通): 曾以 8 个 typed int 重建
    // W.normalize 已返回的同键对象——直赋归一化结果（新身份每刷一次，
    // 徽标簇/注入行头的 stats 绑定照常重估）。completedExclCancelled 键
    // 随 normalize 直达注入行头——曾在此掉键，DashboardRowHeader 回落
    // 手工减法（completed-cancelled），与模型单一推导点脱钩。
    property var _statsObj: W.normalize(null)

    readonly property bool isRunning: AppState.runStatus === 1 && AppState.currentRunningGroup === groupIndex
    readonly property bool expanded: _userToggled ? _userExpanded : (isRunning || _completed > 0)
    // 45 图标全彩常显：组色调（G1..G5）——组头条/图标随组着色
    // 经 ThemeEngine.groupHue 单一映射（5WHY review 2026-08-17：消除与
    // DashboardScreen 的守卫三元复制，回退策略只在一处维护）
    readonly property color _groupHue: ThemeEngine.groupHue(groupIndex)
    // 窄头（竖屏手机）：统计徽标/时长/进度条移到第二行（5WHY review 2026-08-17：
    // PageSection 重构丢弃了 DiagGroupPanel 的 compact 两行头——竖屏下单行
    // 固定内容超宽，组名 elide 消失、徽标被裁剪。断点与 tab 前缀共用
    // ThemeEngine.compactUiWidth，同时覆盖分屏窗口）
    readonly property bool _headerCompact: ThemeEngine.isCompactUi(root.width)

    function reloadModel() {
        var newModel = root.showOnlyCompleted ? AppState.resultsForGroup(groupIndex)
                                              : AppState.allDiagsForGroup(groupIndex)
        // 5WHY (复核 2026-08-18): progress tick 无条件替换数组身份 → Repeater
        // 销毁重建全部瓦片 → 运行中瓦片的本地计时清零（圆点闪烁显示 0/1）。
        // 身份门控：仅当瓦片集合 (id:status) 实际变化时替换模型；无变化发射
        // （run 启动边界、suiteFinished 收尾）不再重建委托。
        // 5WHY (复核 2026-09-05 /simplify 语义身份): 滚动哈希（和折叠→33 进制
        // 双步→长度并入→null 哨兵）曾四轮补丁，每条边（int32 环绕、交换
        // 碰撞、未来键遗漏折叠）都是完整修复轮。逐元素比较是同 O(n)、无
        // 分配、无碰撞类、无哨兵，且新模型键自动纳入比较——W.sameTiles
        // 单一来源。
        if (W.sameTiles(_activeModel, newModel)) return
        _activeModel = newModel
        itemsModel = newModel
    }
    property var _activeModel: null
    // 5WHY (simplify 2026-09-05 每 tick 双扫): statsEqual 门付出了全量
    // groupStats 重建（44 项 C++ 双遍 + normalize 11 键分配）才发现相等。
    // AppState.statsVersion 于结果插入/清屏/scheme 变更递增——版本未变
    // 整跳（连 C++ 重建都不发生）；版本变了再走 statsEqual（组边界仍可能
    // 同值，身份保持）。
    property int _lastStatsVersion: -1
    function _refreshStats() {   // UI-2：命令式赋值，绑定不调 Q_INVOKABLE
        // 5WHY (复核 2026-08-18 Reuse C3): 键归一化经 StatsUtil.js 单一来源；
        // 5WHY (复核 2026-08-19): 仅替换 _statsObj 身份——_total/_completed
        // 为只读绑定，随身份替换自动重估（单写点，无双写分叉）。
        if (AppState.statsVersion === _lastStatsVersion) return
        _lastStatsVersion = AppState.statsVersion
        var s = W.normalize(AppState.groupStats(groupIndex))
        if (W.statsEqual(_statsObj, s)) return
        _statsObj = s
    }
    // 5WHY (复核 2026-08-18 单一入口): reloadModel()+_refreshStats() 对曾以
    // 5 处逐字复制出现（runStatus/currentRunningGroup/target/stateVersion/
    // onCompleted）——刷新序列收敛为 _reload()，新触发源只改一处。
    function _reload() { reloadModel(); _refreshStats() }
    Connections {
        target: AppState
        // 5WHY (复核 2026-08-19 机制统一): enabled 门控取代逐处理器守卫 +
        // _dirty 状态（曾与其余消费方的 Connections.enabled 机制并存两套）。
        // 折叠偏好重置拆出独立未门控块（8-18 语义须在任何可见性下生效）。
        enabled: root.screenVisible
        function onProgressChanged() {
            // UI-3 scope-gate：仅运行中的组逐条刷新（瓦片墙 + 统计）——
            // 已结束组的统计冻结，其边界由 runStatus/currentRunningGroup
            // 处理器全量刷新（5WHY 复核 2026-08-19：曾 5 面板每事件全扫）。
            // 5WHY (复核 2026-08-19 取消排水): cancel 后 currentGroup 已 -1
            // 而池内探针的迟到 Cancelled 结果仍逐一落库并发射——scope-gate
            // 会全量丢弃，瓦片冻结在取消前快照（状态头却计入）。取消态
            // （runStatus 3 且 current=-1）下全面板随迟到结果排水刷新。
            if (groupIndex === AppState.currentRunningGroup
                || (AppState.runStatus === 3 && AppState.currentRunningGroup === -1)) {
                reloadModel()
                _refreshStats()
            }
        }
        function onRunStatusChanged() {
            _reload()   // B1：运行边界全量刷新
        }
        // 8-16：组开始（currentRunningGroup 切换）即加载瓦片墙——
        // 瓦片与组标题同步出现，而非等首条结果/组结束。
        // 5WHY (复核 2026-08-19 scope-gate): 曾 5 面板全量 _reload——组推进
        // 边界只有新当前组的模型/统计会变（已结束组刚被结果落地事件刷新，
        // 未开始组冻结）。仅刷新新当前组；完成时 current=-1 全跳过，由随后
        // 的 runStatusChanged 全量 _reload 覆盖（onSuiteFinished 顺序保证）。
        function onCurrentRunningGroupChanged() {
            if (groupIndex === AppState.currentRunningGroup) _reload()
        }
        // 5WHY (复核 2026-08-18 语义信号): 换 target scheme 不重跑只发
        // filteredDataChanged——groupStats 按 runnableFor(scheme) 过滤，瓦片墙
        // （allDiagsForGroup/resultsForGroup 同源过滤）也会变。旧实现接
        // targetChanged+stateVersionChanged：同轮双发双刷（5 面板 × 2），
        // host 逐键编辑与凭据/语言变更也误触发。语义信号单次驱动。
        function onFilteredDataChanged() { _reload() }
    }
    // 8-18：新 run 开始（Idle→Running）重置折叠偏好——上一轮的折叠/展开
    // 状态不得延续到本轮（用户要求按下 Run 即重置显示界面）。独立未门控块。
    Connections {
        target: AppState
        function onRunStatusChanged() {
            if (AppState.runStatus === 1) {
                _userToggled = false
                _userExpanded = true
            }
        }
    }
    // 5WHY (复核 2026-08-18 创建期零统计): 归档 DiagGroupPanel 的 _gstat 是
    // 绑定（创建即求值）；重构改为命令式 _refreshStats 后 Component.onCompleted
    // 只调 reloadModel——仪表盘页在运行结束后才创建的组面板（Dashboard 切页、
    // 应用重启后无新 progress 事件）统计永久停留 0/0。补创建期刷新。
    Component.onCompleted: _reload()

    // 内建统计簇（5WHY review round 3: 计数+三徽标曾逐字复制两份，加徽标/
    // 改格式必须双处同步——现为单一内联组件）
    // 5WHY (复核 2026-08-18, 用户诉求): 仅 pass/warn/fail 三徽标是回归——归档
    // DiagGroupPanel 是 6 徽标（0/5/1/2/3/4）；补全 7 状态（含 cancelled=6）。
    // 5WHY (2026-08-19 用户诉求 "X/Y 第一行、状态图标第二行"): X/Y 计数已
    // 上移到组头首行 Label，徽标簇只呈现 7 状态图标（showLabel 删除）。
    component BuiltinStats: StatusBadgeCluster {
        compact: ThemeEngine.isMobile
    }
    Component {
        id: builtinStatsComp
        BuiltinStats { stats: root._statsObj }
    }

    // 注入行头（宽/窄两行共用的 Loader+Binding 脚手架；5WHY review round 4:
    // 曾逐字复制两份——stats Binding、fillProgress 回传、active 条件各写一次）
    component RowHeaderInjected: Loader {
        id: rowHeaderInjectedLoader
        property var _stats: ({})
        property var injectedDelegate: null
        property bool loaderActive: false
        property bool fillProgress: false
        active: loaderActive && injectedDelegate !== null
        sourceComponent: injectedDelegate
        Binding {
            target: rowHeaderInjectedLoader.item
            property: "stats"
            value: rowHeaderInjectedLoader._stats
            when: rowHeaderInjectedLoader.item !== null && "stats" in rowHeaderInjectedLoader.item
        }
        Binding {
            target: rowHeaderInjectedLoader.item
            property: "fillProgress"
            value: rowHeaderInjectedLoader.fillProgress
            when: rowHeaderInjectedLoader.item !== null
                  && "fillProgress" in rowHeaderInjectedLoader.item
        }
    }

    ColumnLayout {
        spacing: ThemeEngine.spacing.sm

        // ── 组头（2026-08-19：诊断页恒两行——首行组识别+X/Y 计数，
        //    第二行状态徽标；Dashboard 宽屏单行内联行头 / 窄屏两行进度条）──
        Item {
            id: headerBox
            Layout.fillWidth: true
            implicitHeight: headerCol.implicitHeight
            ColumnLayout {
                id: headerCol
                anchors.fill: parent
                spacing: 0
                // ── 第一行：组识别（名称/运行中标签）+ X/Y 计数 + 宽屏内联行头 ──
                // 5WHY (2026-08-19 用户诉求 "5/5 显示在第一行"): X/Y 曾与 7
                // 徽标同簇（StatusBadgeCluster.showLabel）——宽屏与组名挤在
                // 一行、窄屏落在第二行，位置随断点漂移。计数是组级主指标，
                // 按业界惯例（层级：主指标→明细）固定于首行行尾。
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    spacing: ThemeEngine.spacing.sm
                    // accent bar
                    Rectangle {
                        Layout.preferredWidth: 4
                        Layout.fillHeight: true
                        color: root.isRunning ? ThemeEngine.colors.primary : root._groupHue
                        radius: 2
                    }
                    AppIcon {
                        name: ThemeEngine.groupIconName(groupIndex)
                        size: 24
                        color: root._groupHue
                    }
                    Label {
                        // M8：组前缀 "G1:"（归档语义，折叠态也能定位第几组）
                        text: (T.groupPrefix(groupIndex) ? T.groupPrefix(groupIndex) + ": " : "")
                              + (T.groupName(groupIndex) || T.tr("groupGeneric"))
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.subhead
                        font.weight: Font.DemiBold
                        color: ThemeEngine.colors.onSurface
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    // 行头徽标注入（Dashboard：DashboardRowHeader；宽屏内联）
                    RowHeaderInjected {
                        injectedDelegate: root.rowHeaderDelegate
                        loaderActive: !root._headerCompact
                        _stats: root._statsObj
                    }
                    Label {
                        visible: root.isRunning
                        text: AppState.currentDiagLabel || ""
                        color: ThemeEngine.colors.primary
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.caption
                        elide: Text.ElideRight
                    }
                    // X/Y 计数（诊断页首行；Dashboard 行头已含徽标不重复显示）
                    Label {
                        visible: root.rowHeaderDelegate === null && root._total > 0
                        text: ThemeEngine.xyLabel(root._completed, root._total)
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: ThemeEngine.fontSize.body
                        font.weight: Font.Bold
                        color: ThemeEngine.colors.onSurfaceVariant
                    }
                    AppIcon {
                        name: "chevron-down"; size: 14
                        color: ThemeEngine.colors.textMuted
                        rotation: expanded ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: 150 } }
                    }
                }
                // ── 第二行（诊断页=状态图标簇；Dashboard 窄屏=进度条行）──
                // 5WHY (2026-08-19 用户诉求 "状态图标在第二行"): 徽标曾宽屏
                // 内联在组名行（被推到最右）、窄屏与进度条同行——两个断点
                // 两种位置。统一为独立第二行：首条结果落地后出现，左对齐
                // 对位首行图标列。
                // 5WHY (复核 2026-08-19 脚手架合一): 两行曾各自复制
                // RowLayout+fillWidth+spacing+4px 对位垫脚手——内容条件互斥
                // （rowHeaderDelegate 注入与否），合并为单行、OR 门控，内容
                // 各自可见性/loaderActive 门控；对位规格只在一处维护。
                RowLayout {
                    visible: (root.rowHeaderDelegate === null && root._completed > 0)
                          || (root._headerCompact && root.rowHeaderDelegate !== null)
                    Layout.fillWidth: true
                    spacing: ThemeEngine.spacing.sm
                    // 5WHY (复核 2026-08-18 对位目标): 徽标左对齐的对位目标是
                    // 首行【内容列】（accent bar 4 + spacing.sm 8 = 12px 处的
                    // 组图标），不是装饰条左缘——归档 11px 与重构 12px 均对齐
                    // 图标列。保留 4px Item（行 spacing 8 补齐 12px）。
                    Item { Layout.preferredWidth: 4 }
                    // 5WHY (复核 2026-08-18 效率): 非活动内容的 Loader 门控
                    // 完全不构造（7 徽标 × 5 面板 ≈ 70 对象常驻的问题）；首条
                    // 结果落地前（_completed===0）同样不构造。
                    Loader {
                        active: root.rowHeaderDelegate === null && root._completed > 0
                        sourceComponent: builtinStatsComp
                    }
                    RowHeaderInjected {
                        Layout.fillWidth: true   // fillProgress 进度条占满剩余宽度的前提
                        injectedDelegate: root.rowHeaderDelegate
                        loaderActive: root._headerCompact && root.rowHeaderDelegate !== null
                        fillProgress: true
                        _stats: root._statsObj
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { root._userToggled = true; root._userExpanded = !root._userExpanded }
                // NEW-22：折叠头可聚焦/读屏可达
                Accessible.role: Accessible.Button
                Accessible.name: (T.groupPrefix(groupIndex) ? T.groupPrefix(groupIndex) + ": " : "")
                                 + (T.groupName(groupIndex) || T.tr("groupGeneric"))
            }
        }

        // ── 瓦片墙 ──
        PageTileGridSection {
            Layout.fillWidth: true
            visible: root.expanded
            model: root.itemsModel
            compact: root.compactTiles
            usePerItemRunning: true
            groupRunning: root.isRunning
            screenVisible: root.screenVisible
            viewportItem: root.viewportItem
            onTileClicked: function(data) { root.detailRequested(data) }
        }
    }
}
