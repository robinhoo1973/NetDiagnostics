// =============================================================================
// DashboardSummaryComp.qml — 摘要统计层（归档 SummaryCards 迁移）
//
// 归档布局：Summary + Total 小标题行，7 类结果彩色行（Pass/Info/Warning/
// Fail/Skipped/Error/Cancelled），空态单行提示。数据源：AppState.groupStats(-1) 聚合。
// 5WHY (复核 2026-08-18): 头部注释曾滞留"6 类"——模型已含 7 类（含 Cancelled），
// 头部更新防维护者误判 Cancelled 仍缺失而重开已修复的 bug。
// UI-2：命令式刷新（绑定不调 Q_INVOKABLE）。
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import "../../widgets/StatsUtil.js" as W   // qrc:/qt/qml/Dashboard/View/ → ../../widgets/ 直接 JS 导入

Item {
    id: root
    implicitHeight: sumCol.implicitHeight

    // 5WHY (复核 2026-08-18 Reuse C3): 键集合归一化上移到 StatsUtil.js。
    property var _s: W.StatsUtil.normalize(null)
    readonly property int _count: _s.pass + _s.warn + _s.fail + _s.skip + _s.info + _s.error + _s.cancelled

    function _refresh() {
        _s = W.StatsUtil.normalize(AppState.groupStats(-1))
    }
    Connections {
        target: AppState
        function onProgressChanged() { root._refresh() }
        function onRunStatusChanged() { root._refresh() }
    }
    Component.onCompleted: _refresh()

    ColumnLayout {
        id: sumCol
        anchors.fill: parent
        spacing: 0

        // Header: "Summary" + "Total: N"
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: T.tr("summary")
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.caption
                font.weight: Font.DemiBold
                color: ThemeEngine.colors.onSurfaceVariant
                elide: Text.ElideRight
            }
            Label {
                text: T.tr("totalDiagsLabel") + ": " + root._s.total
                font.family: ThemeEngine.monoFont
                font.pixelSize: ThemeEngine.fontSize.caption
                color: ThemeEngine.colors.onSurfaceVariant
            }
        }
        Item { Layout.preferredHeight: 6 }

        // 空态：单行提示（归档行为）
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 4
            visible: root._count === 0
            text: T.tr("runFromDiag")
            font.family: ThemeEngine.monoFont
            font.pixelSize: ThemeEngine.fontSize.caption
            color: Qt.alpha(ThemeEngine.colors.onSurfaceVariant, 0.5)
            horizontalAlignment: Text.AlignHCenter
        }

        // 7 类结果彩色行（5WHY simplify 2026-08-17：各行仅 accent/icon/label/
        // count 键不同，且与 ThemeEngine.statusColors/statusIconNames 1:1
        // 对应——一张表驱动，状态映射不再双份维护）
        // 5WHY (复核 2026-08-18 五表漂移): 表上移到 ThemeEngine.statusRows
        // 单一来源，与 StatusBadgeCluster 同源消费。
        Repeater {
            model: ThemeEngine.statusRows
            SummaryCard {
                Layout.fillWidth: true
                accent: ThemeEngine.statusColors[modelData.code] || ThemeEngine.colors.skip
                iconName: ThemeEngine.statusIconNames[modelData.code] || "badge-info"
                label: T.tr(modelData.labelKey)
                count: root._s[modelData.countKey] || 0
                visible: root._count > 0
            }
        }
    }

    component SummaryCard: Rectangle {
        property color accent: ThemeEngine.colors.success
        property string label: ""
        property string iconName: "badge-info"
        property int count: 0
        implicitHeight: 32
        radius: 6
        Layout.topMargin: 2
        color: Qt.alpha(accent, 0.06)
        border { width: 1; color: Qt.alpha(accent, 0.2) }

        RowLayout {
            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
            AppIcon {
                name: iconName
                size: 14
                color: accent
            }
            Item { Layout.fillWidth: true }
            Label {
                text: label
                font.family: ThemeEngine.monoFont
                font.pixelSize: ThemeEngine.fontSize.caption
                font.weight: Font.Medium
                color: ThemeEngine.colors.onSurfaceVariant
            }
            Item { width: 8 }
            Label {
                text: count
                font.family: ThemeEngine.monoFont
                font.pixelSize: ThemeEngine.fontSize.body
                font.weight: Font.Bold
                color: accent
            }
        }
    }
}
