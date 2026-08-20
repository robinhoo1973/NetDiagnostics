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
    active: _narrative !== ""

    ColumnLayout {
        Layout.fillWidth: true
        spacing: ThemeEngine.spacing.xs

        Label {
            text: T.tr("summary")
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.caption
            color: ThemeEngine.colors.onSurfaceVariant
        }
        Label {
            Layout.fillWidth: true
            text: root._narrative
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.body
            color: ThemeEngine.colors.onSurface
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            lineHeight: 1.35
        }
    }
}
