// =============================================================================
// PageSummarySection.qml — DetailPage 摘要卡（结论 + 依据叙述，page-detail §2.2b）
//
// 5WHY (2026-08-20 用户诉求 "摘要卡文字叙述"): 详情页只有 hero 单行 summary，
// 检测推导逻辑（如何判断 DNS 劫持/污染、如何推断 VPN、测速过程与可达性结论）
// 只存在于探针代码，用户无法看到「为什么是这个结论」。本卡呈现 C++ 探针
// 就地生成的 narrative（结论 + 依据多行叙述），置于 Hero 与 Metric 之间
// （决策：结论优先）。数据单一来源 = DiagnosticResult.narrative。
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    topMargin: ThemeEngine.spacing.sm
    bottomMargin: ThemeEngine.spacing.sm

    property var detailData: ({})
    readonly property string _narrative: detailData.narrative || ""
    // 5WHY (2026-08-23 详情页信息前置): meta.summaryOutline 静态声明的过程
    // 概览（阶段序列）渲染于结论行之上——用户展开 terminal 前即知「测了什么、
    // 按什么顺序」；空表 = 未迁移探针，完全回退纯 narrative。
    readonly property var _outline: detailData.summaryOutline || []
    active: _narrative !== "" || _outline.length > 0

    ColumnLayout {
        Layout.fillWidth: true
        spacing: ThemeEngine.spacing.xs

        Label {
            text: T.tr("summary")
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.caption
            color: ThemeEngine.colors.onSurfaceVariant
        }
        // 过程概览行：序号徽标 + 阶段描述（caption 弱化色）
        Repeater {
            model: root._outline
            delegate: RowLayout {
                Layout.fillWidth: true
                spacing: ThemeEngine.spacing.sm
                Rectangle {
                    width: 16; height: 16; radius: 8
                    color: Qt.alpha(ThemeEngine.colors.primary, 0.14)
                    Label {
                        anchors.centerIn: parent
                        text: index + 1
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: ThemeEngine.fontSize.micro
                        color: ThemeEngine.colors.primary
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: modelData
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.caption
                    color: ThemeEngine.colors.onSurfaceVariant
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
            }
        }
        Label {
            // 概览与结论之间的分隔线（仅双段齐备时出现）
            Layout.fillWidth: true
            height: 1
            color: Qt.alpha(ThemeEngine.colors.outlineVariant, 0.6)
            visible: root._outline.length > 0 && root._narrative !== ""
        }
        Label {
            Layout.fillWidth: true
            text: root._narrative
            visible: root._narrative !== ""
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.body
            color: ThemeEngine.colors.onSurface
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            lineHeight: 1.35
        }
    }
}
