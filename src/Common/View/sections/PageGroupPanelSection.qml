// PageGroupPanelSection.qml — 组面板（§2.4 核心：组头 + 进度 + 瓦片墙）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import theme
import widgets
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
    property int _modelVersion: 0
    property int _total: 0
    property int _completed: 0
    property int _pass: 0
    property int _warn: 0
    property int _fail: 0
    property int _skip: 0
    property int _info: 0
    property int _error: 0
    property int _durationMs: 0
    property bool _userToggled: false
    property bool _userExpanded: true
    readonly property var _statsObj: ({ pass: _pass, warn: _warn, fail: _fail, skip: _skip, info: _info, error: _error,
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
    readonly property bool _headerCompact: ThemeEngine.isMobile && root.width < ThemeEngine.compactUiWidth

    function reloadModel() {
        itemsModel = root.showOnlyCompleted ? AppState.resultsForGroup(groupIndex)
                                            : AppState.allDiagsForGroup(groupIndex)
        _modelVersion++
    }
    function _refreshStats() {   // UI-2：命令式赋值，绑定不调 Q_INVOKABLE
        var s = AppState.groupStats(groupIndex)
        _total = s.total || 0
        _completed = s.completed || 0
        _pass = s.pass || 0
        _warn = s.warn || 0
        _fail = s.fail || 0
        _skip = s.skip || 0
        _info = s.info || 0
        _error = s.error || 0
        _durationMs = s.durationMs || 0   // H2：行时长注入 DashboardRowHeader
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
    component BuiltinStats: RowLayout {
        property bool _visible: true
        property int _total: 0
        property int _completed: 0
        property int _pass: 0
        property int _warn: 0
        property int _fail: 0
        Label {
            visible: parent._visible
            text: parent._completed + "/" + parent._total
            font.family: ThemeEngine.monoFont
            font.pixelSize: ThemeEngine.fontSize.body
            color: ThemeEngine.colors.onSurfaceVariant
        }
        StatusBadge { visible: parent._visible; statusCode: 0; count: parent._pass }
        StatusBadge { visible: parent._visible; statusCode: 1; count: parent._warn }
        StatusBadge { visible: parent._visible; statusCode: 2; count: parent._fail }
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
                    Loader {
                        id: rowHeaderLoaderWide
                        active: root.rowHeaderDelegate !== null && !root._headerCompact
                        sourceComponent: root.rowHeaderDelegate
                    }
                    Binding {
                        target: rowHeaderLoaderWide.item
                        property: "stats"
                        value: root._statsObj
                        when: rowHeaderLoaderWide.item !== null && "stats" in rowHeaderLoaderWide.item
                    }
                    Label {
                        visible: root.isRunning
                        text: AppState.currentDiagLabel || ""
                        color: ThemeEngine.colors.primary
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.caption
                        elide: Text.ElideRight
                    }
                    BuiltinStats {
                        _visible: root.rowHeaderDelegate === null && !root._headerCompact
                        _total: root._total; _completed: root._completed
                        _pass: root._pass; _warn: root._warn; _fail: root._fail
                    }
                    AppIcon {
                        name: "chevron-down"; size: 14
                        color: ThemeEngine.colors.textMuted
                        rotation: expanded ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: 150 } }
                    }
                }
                // ── 第二行（窄屏）：统计徽标/时长/进度条 ──
                RowLayout {
                    visible: root._headerCompact
                    Layout.fillWidth: true
                    spacing: ThemeEngine.spacing.sm
                    Item { Layout.preferredWidth: 4 }   // 缩进=accent bar(4)+spacing.sm(8)=12（5WHY review round 3: 原 11 与首行几何脱节）
                    Loader {
                        id: rowHeaderLoaderCompact
                        // 5WHY (review round 3): 缺 _headerCompact 门限时桌面端每个
                        // 组面板多实例化一份隐形 DashboardRowHeader（绑定照跑）
                        active: root.rowHeaderDelegate !== null && root._headerCompact
                        sourceComponent: root.rowHeaderDelegate
                    }
                    Binding {
                        target: rowHeaderLoaderCompact.item
                        property: "stats"
                        value: root._statsObj
                        when: rowHeaderLoaderCompact.item !== null && "stats" in rowHeaderLoaderCompact.item
                    }
                    Binding {
                        // 窄屏第二行：进度条占满剩余宽度（归档全宽行为）
                        target: rowHeaderLoaderCompact.item
                        property: "fillProgress"
                        value: true
                        when: rowHeaderLoaderCompact.item !== null && "fillProgress" in rowHeaderLoaderCompact.item
                    }
                    BuiltinStats {
                        _visible: root.rowHeaderDelegate === null
                        _total: root._total; _completed: root._completed
                        _pass: root._pass; _warn: root._warn; _fail: root._fail
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
