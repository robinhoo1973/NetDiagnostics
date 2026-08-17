// =============================================================================
// DashboardSummaryComp.qml — 摘要统计层（归档 SummaryCards 迁移）
//
// 归档布局：Summary + Total 小标题行，6 类结果彩色行（Pass/Info/Warning/
// Fail/Skipped/Error），空态单行提示。数据源：AppState.groupStats(-1) 聚合。
// UI-2：命令式刷新（绑定不调 Q_INVOKABLE）。
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets

Item {
    id: root
    implicitHeight: sumCol.implicitHeight

    property var _s: ({ pass: 0, warn: 0, fail: 0, skip: 0, info: 0, error: 0, total: 0 })
    readonly property int _count: _s.pass + _s.warn + _s.fail + _s.skip + _s.info + _s.error

    function _refresh() {
        var a = AppState.groupStats(-1)
        _s = { pass: a.pass || 0, warn: a.warn || 0, fail: a.fail || 0,
               skip: a.skip || 0, info: a.info || 0, error: a.error || 0,
               total: a.total || 0 }
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

        // 6 类结果彩色行（5WHY simplify 2026-08-17：六行仅 accent/icon/label/
        // count 键不同，且与 ThemeEngine.statusColors/statusIconNames 1:1
        // 对应——一张表驱动，状态映射不再双份维护）
        Repeater {
            model: [
                { code: 0, labelKey: "summaryPass",    countKey: "pass" },
                { code: 5, labelKey: "summaryInfo",    countKey: "info" },
                { code: 1, labelKey: "summaryWarning", countKey: "warn" },
                { code: 2, labelKey: "summaryFail",    countKey: "fail" },
                { code: 3, labelKey: "summarySkipped", countKey: "skip" },
                { code: 4, labelKey: "summaryError",   countKey: "error" }
            ]
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
