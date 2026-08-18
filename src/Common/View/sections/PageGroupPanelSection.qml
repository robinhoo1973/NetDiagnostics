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

    // Dashboard 行变体注入（UI-5：依赖方向页面→Common）
    property Component rowHeaderDelegate: null   // 行头徽标（DashboardRowHeader 注入）
    property bool showOnlyCompleted: false       // Dashboard：结束后只显示已完成项
    property bool compactTiles: ThemeEngine.isMobile

    property var itemsModel: []
    // 5WHY (复核 2026-08-18): _modelVersion 只写不读（Repeater 绑定 itemsModel
    // 身份而非版本 key）——_activeKey 签名门控已完全取代它，删除。
    property int _total: 0
    property int _completed: 0
    property int _pass: 0
    property int _warn: 0
    property int _fail: 0
    property int _skip: 0
    property int _info: 0
    property int _error: 0
    property int _cancelled: 0
    property int _durationMs: 0
    property bool _userToggled: false
    property bool _userExpanded: true
    // 5WHY (复核 2026-08-18): cancelled 缺 key——groupStats 把取消项计入
    // completed，出现 "X/X 完成但徽标数字对不上"；补全 7 状态。
    readonly property var _statsObj: ({ pass: _pass, warn: _warn, fail: _fail, skip: _skip, info: _info, error: _error, cancelled: _cancelled,
                                         total: _total, completed: _completed, durationMs: _durationMs })

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
        // 签名门控：仅当瓦片集合 (id:status) 实际变化时替换模型；无变化发射
        // （run 启动边界、suiteFinished 收尾）不再重建委托。
        // 5WHY (复核 2026-08-18 效率): 字符串拼接签名是 O(n²) 拷贝——改用
        // 无分配滚动哈希（31 进制）+ 长度预检（长度变则签名必变）。
        if (newModel.length !== _activeLen) {
            _activeLen = newModel.length
            _activeHash = -1
        }
        var h = 0
        for (var i = 0; i < newModel.length; ++i)
            h = (h * 31 + newModel[i].diagId + (newModel[i].status !== undefined ? newModel[i].status : -1)) | 0
        if (h === _activeHash && _activeLen === newModel.length) return
        _activeHash = h
        itemsModel = newModel
    }
    property int _activeHash: -1
    property int _activeLen: -1
    function _refreshStats() {   // UI-2：命令式赋值，绑定不调 Q_INVOKABLE
        // 5WHY (复核 2026-08-18 Reuse C3): 键归一化经 StatsUtil.js 单一来源；
        // 本组件保留 typed int 属性（绑定追踪需要）。
        var s = W.StatsUtil.normalize(AppState.groupStats(groupIndex))
        _total = s.total
        _completed = s.completed
        _pass = s.pass
        _warn = s.warn
        _fail = s.fail
        _skip = s.skip
        _info = s.info
        _error = s.error
        _cancelled = s.cancelled
        _durationMs = s.durationMs   // H2：行时长注入 DashboardRowHeader
    }
    Connections {
        target: AppState
        function onProgressChanged() {
            // UI-3 scope-gate：仅刷新运行中的组；非运行面板用 per-item 绑定。
            if (groupIndex === AppState.currentRunningGroup) reloadModel()
            _refreshStats()
        }
        function onRunStatusChanged() {
            // 8-18：新 run 开始（Idle→Running）重置折叠偏好——上一轮的折叠/展开
            // 状态不得延续到本轮（用户要求按下 Run 即重置显示界面）。
            if (AppState.runStatus === 1) {
                _userToggled = false
                _userExpanded = true
            }
            reloadModel(); _refreshStats()   // B1：运行边界全量刷新
        }
        // 8-16：组开始（currentRunningGroup 切换）即加载瓦片墙——
        // 瓦片与组标题同步出现，而非等首条结果/组结束。
        function onCurrentRunningGroupChanged() { reloadModel(); _refreshStats() }
    }
    Component.onCompleted: reloadModel()

    // 内建统计簇（宽/窄两行共用；5WHY review round 3: 计数+三徽标曾逐字复制
    // 两份，加徽标/改格式必须双处同步——现为单一内联组件）
    // 5WHY (复核 2026-08-18, 用户诉求): 仅 pass/warn/fail 三徽标是回归——归档
    // DiagGroupPanel 是 6 徽标（0/5/1/2/3/4）；补全 7 状态（含 cancelled=6）。
    // 5WHY (复核 2026-08-18 三处复制收敛 + Loader 门控): 单一 Component 供两行
    // Loader 复用，stats 在构造时绑定注入。
    component BuiltinStats: StatusBadgeCluster {
        showLabel: true
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

        // ── 组头（宽屏单行 / 窄屏两行——统计移到第二行，归档 DiagGroupPanel compact 行为）──
        Item {
            id: headerBox
            Layout.fillWidth: true
            implicitHeight: headerCol.implicitHeight
            ColumnLayout {
                id: headerCol
                anchors.fill: parent
                spacing: 0
                // ── 第一行：组识别（名称/运行中标签）+ 宽屏内联统计 ──
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
                              + (T.groupName(groupIndex) || qsTr("Group"))
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
                    // 5WHY (复核 2026-08-18 效率): 宽/窄两行的簇各实例化一份
                    // （7 徽标 × 5 面板 ≈ 70 对象常驻），visible 切换不卸载。
                    // Loader 门控：非活动行的簇完全不构造。
                    Loader {
                        active: root.rowHeaderDelegate === null && !root._headerCompact
                        sourceComponent: builtinStatsComp
                    }
                    AppIcon {
                        name: "chevron-down"; size: 14
                        color: ThemeEngine.colors.textMuted
                        rotation: expanded ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: 150 } }
                    }
                }
                // ── 第二行（窄屏）：统计徽标/时长/进度条 ──
                // 5WHY (复核 2026-08-18, 用户诉求): 徽标曾在行尾（被 fillWidth
                // 行头推到最右）；归档 compact 第二行徽标左对齐——调序为先
                // 徽标簇、后行头（进度条仍占满剩余宽度）。
                RowLayout {
                    visible: root._headerCompact
                    Layout.fillWidth: true
                    spacing: ThemeEngine.spacing.sm
                    // 5WHY (复核 2026-08-18 对位目标): 徽标左对齐的对位目标是
                    // 首行【内容列】（accent bar 4 + spacing.sm 8 = 12px 处的
                    // 组图标），不是装饰条左缘——归档 11px 与重构 12px 均对齐
                    // 图标列。保留 4px Item（行 spacing 8 补齐 12px）。
                    Item { Layout.preferredWidth: 4 }
                    Loader {
                        active: root.rowHeaderDelegate === null
                        sourceComponent: builtinStatsComp
                    }
                    RowHeaderInjected {
                        Layout.fillWidth: true   // fillProgress 进度条占满剩余宽度的前提
                        injectedDelegate: root.rowHeaderDelegate
                        loaderActive: root._headerCompact
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
                                 + (T.groupName(groupIndex) || qsTr("Group"))
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
            onTileClicked: function(data) { root.detailRequested(data) }
        }
    }
}
