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
    property bool _userToggled: false
    property bool _userExpanded: true
    readonly property var _statsObj: ({ pass: _pass, warn: _warn, fail: _fail, skip: _skip, info: _info, error: _error })

    readonly property bool isRunning: AppState.runStatus === 1 && AppState.currentRunningGroup === groupIndex
    readonly property bool expanded: _userToggled ? _userExpanded : (isRunning || _completed > 0)

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
    }
    Connections {
        target: AppState
        function onProgressChanged() {
            // UI-3 scope-gate：仅刷新运行中的组；非运行面板用 per-item 绑定。
            if (groupIndex === AppState.currentRunningGroup) reloadModel()
            _refreshStats()
        }
        function onRunStatusChanged() { reloadModel(); _refreshStats() }   // B1：运行边界全量刷新
    }
    Component.onCompleted: reloadModel()

    ColumnLayout {
        spacing: ThemeEngine.spacing.sm

        // ── 组头 ──
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            RowLayout {
                anchors.fill: parent
                spacing: ThemeEngine.spacing.sm
                // accent bar
                Rectangle {
                    Layout.preferredWidth: 4
                    Layout.fillHeight: true
                    color: root.isRunning ? ThemeEngine.colors.primary : ThemeEngine.colors.secondary
                    radius: 2
                }
                AppIcon {
                    name: ["network-card", "ethernet", "cloud-shield", "compass", "config"][groupIndex] || "circle"
                    size: 20
                    color: ThemeEngine.colors.cyan
                }
                Label {
                    text: T.groupName(groupIndex) || qsTr("Group")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.subhead
                    font.weight: Font.DemiBold
                    color: ThemeEngine.colors.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                // 行头徽标注入（Dashboard：DashboardBadge×6）
                Loader {
                    id: rowHeaderLoader
                    active: root.rowHeaderDelegate !== null
                    sourceComponent: root.rowHeaderDelegate
                }
                Binding {
                    target: rowHeaderLoader.item
                    property: "stats"
                    value: root._statsObj
                    when: rowHeaderLoader.item !== null && "stats" in rowHeaderLoader.item
                }
                Label {
                    visible: root.isRunning
                    text: AppState.currentDiagLabel || ""
                    color: ThemeEngine.colors.primary
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.caption
                    elide: Text.ElideRight
                }
                Label {
                    text: root._completed + "/" + root._total
                    font.family: ThemeEngine.monoFont
                    font.pixelSize: ThemeEngine.fontSize.body
                    color: ThemeEngine.colors.textSecondary
                }
                StatusBadge { statusCode: 0; count: _pass }
                StatusBadge { statusCode: 1; count: _warn }
                StatusBadge { statusCode: 2; count: _fail }
                AppIcon {
                    name: "chevron-down"; size: 14
                    color: ThemeEngine.colors.textMuted
                    rotation: expanded ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: 150 } }
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { root._userToggled = true; root._userExpanded = !root._userExpanded }
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
