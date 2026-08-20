// PagePropertiesSection.qml — DetailPage 属性列表（page-detail.md §2.5）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    bottomMargin: ThemeEngine.spacing.lg
    contentSpacing: 6

    property var detailData: ({})
    readonly property bool _hasProperties: (detailData.properties || []).length > 0
    active: _hasProperties && detailData.showProperties !== false
    property bool _expanded: true
    // C++ 下发的 propLayout 枚举值（AppState 序列化为 int）
    readonly property int _propLayoutGrouped: 1

    ColumnLayout {
        spacing: 0
        CollapsibleSectionHeader {
            Layout.fillWidth: true
            title: T.tr("detailProperties")
            expanded: root._expanded
            onToggleRequested: root._expanded = !root._expanded
        }
        Repeater {
            model: root._expanded ? (detailData.properties || []) : []
            delegate: ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                // 5WHY (2026-08-20 用户诉求 "属性卡一团混乱数据"): 多实例
                // 检测（网卡/接口）在 Kv 模式下平铺无层级；Grouped 模式
                // （C++ propLayout 下发）把每个属性渲染为一个子组：实例名
                // 为组标题（强调色），子字段为对齐的 label/value 行。
                // 5WHY (复核 2026-08-20 Grouped 无子行丢值): 扁平属性
                // （Host Name、gateway→interface、connection 端点、cellular
                // iface→MAC）在 Grouped 下曾只渲染标题——value Label 被整体
                // 隐藏，整行数据丢失。修正：仅当属性确有 children 时按
                // 「组标题」渲染；无 children 时回退标准 kv 行（防御纵深）。
                readonly property bool _isGroup:
                    detailData.propLayout === root._propLayoutGrouped
                    && (modelData.children || []).length > 0
                RowLayout {
                    Layout.fillWidth: true
                    spacing: ThemeEngine.spacing.sm
                    Label {
                        text: modelData.label || ""
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: _isGroup ? ThemeEngine.fontSize.subhead : ThemeEngine.fontSize.body
                        font.weight: _isGroup ? Font.DemiBold : Font.Normal
                        color: _isGroup ? ThemeEngine.colors.tertiary : ThemeEngine.colors.onSurfaceVariant
                        Layout.preferredWidth: _isGroup ? -1 : Math.min(180, implicitWidth)
                        Layout.fillWidth: _isGroup
                        elide: Text.ElideRight
                    }
                    // 5WHY (2026-08-20 用户诉求 "属性卡一团混乱数据"): Grouped
                    // 模式下曾整体隐藏 value——组标题只有接口名（wlan0），
                    // 主值（SSID/首地址/状态）随行丢失，组与组难以区分。
                    // 业界惯例：组标题 = 实例名，主值常显于标题行右端
                    // （弱化色、等宽）——扫描一眼即可区分各实例。
                    Label {
                        text: modelData.value || ""
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: _isGroup ? ThemeEngine.fontSize.caption : ThemeEngine.fontSize.body
                        color: _isGroup ? ThemeEngine.colors.textMuted : ThemeEngine.colors.onSurface
                        Layout.fillWidth: !_isGroup
                        wrapMode: Text.WrapAnywhere
                        visible: !_isGroup || text !== ""
                        horizontalAlignment: _isGroup ? Text.AlignRight : Text.AlignLeft
                    }
                }
                Repeater {
                    model: modelData.children || []
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: detailData.propLayout === 1 ? 0 : ThemeEngine.spacing.lg
                        spacing: ThemeEngine.spacing.sm
                        Label {
                            text: (detailData.propLayout === 1 ? "" : "· ") + (modelData.label || "")
                            font.family: ThemeEngine.fontUi
                            font.pixelSize: detailData.propLayout === 1 ? ThemeEngine.fontSize.body : ThemeEngine.fontSize.caption
                            color: ThemeEngine.colors.textMuted
                            Layout.preferredWidth: detailData.propLayout === 1 ? 140 : -1
                            elide: Text.ElideRight
                        }
                        Label {
                            text: modelData.value || ""
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: detailData.propLayout === 1 ? ThemeEngine.fontSize.body : ThemeEngine.fontSize.caption
                            color: ThemeEngine.colors.onSurfaceVariant
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }
        }
    }
}
