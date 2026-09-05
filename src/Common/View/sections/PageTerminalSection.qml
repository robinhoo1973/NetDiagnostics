// PageTerminalSection.qml — DetailPage 终端输出（page-detail.md §2.7；TerminalBlock）
//
// 5WHY (2026-08-23 详情页信息前置, review/ui-ux-audit-plan-2026-08-23.md §5):
// terminal 曾默认展开且打字机自动播放至 360px——最原始的信息占据视觉黄金位，
// Summary/Properties 的结构化结论被推后。定位反转：terminal = 证据库（折叠
// 手风琴，与 Properties/Charts 同一 CollapsibleSectionHeader 模式），首次展开
// 才创建 Loader（延迟构建 + 打字机只播一次），重展开显示全文不再回放。
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import detail
import core
import widgets
import "../widgets/StatsUtil.js" as W   // lineCount（行数计数单一来源）

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    bottomMargin: ThemeEngine.spacing.lg
    contentSpacing: 6
    // 主题自适应（7-5）：暗色=surface，亮色=input——不再硬编码深藏青
    cardColor: ThemeEngine.terminalBg   // 属性（主题切换可响应）

    property var detailData: ({})
    readonly property string _terminalText: detailData.details || detailData.rawOutput || ""
    // (simplify 2026-09-05: W.lineCount 与 TerminalBlock 打字机计数同源，
    // 空串语义内建——曾与 split('\n').length 双实现漂移)
    readonly property int _terminalLines: W.lineCount(_terminalText)
    active: _terminalLines > 0 && detailData.showTerminal !== false

    // 折叠状态：默认收起；切换详情对象时复位（每个检测从收起开始）
    property bool _expanded: false
    property bool _everExpanded: false
    onDetailDataChanged: {
        // Qt.callLater 防御：detailData 可能在同栈内被整体替换（反模式 #4 同源），
        // 延迟一帧避免绑定求值期改写自身依赖。
        Qt.callLater(function () { root._expanded = false; root._everExpanded = false; })
    }

    ColumnLayout {
        spacing: ThemeEngine.spacing.xs

        CollapsibleSectionHeader {
            Layout.fillWidth: true
            // 5WHY (review 2026-08-23): " lines" 曾硬编码英文——15 语言下
            // 行数后缀漏译。改经 lineCountSuffix 键。
            title: T.tr("detailTerminal")
                + (root._expanded ? "" : "  ·  " + root._terminalLines + " " + T.tr("lineCountSuffix"))
            expanded: root._expanded
            onToggleRequested: {
                root._expanded = !root._expanded
                if (root._expanded) root._everExpanded = true
            }
        }

        Loader {
            id: termLoader
            Layout.fillWidth: true
            visible: root._expanded
            // 延迟构建：首次展开才实例化 TerminalBlock（打字机自然只播一次；
            // 收起再展开时组件仍存活、显示全文不回放）。
            active: root._everExpanded
            Layout.preferredHeight: visible ? Math.min(360, Math.max(72, root._terminalLines * 18 + 24)) : 0
            sourceComponent: TerminalBlock {
                // 5WHY (2026-08-22, issue 2): TerminalBlock 的 implicitHeight 跟随
                // 全文行数（长输出可达 1200px+），溢出 Loader 的钳制视口后，内部
                // Flickable 视口=全文高度、interactive=false 永不滚动，末尾行被
                // 后序区块覆盖。必须把组件尺寸显式钉住 = Loader 视口，内部滚动
                // 才接管（独立 harness 验证：viewport 336 → contentY 可达 869=maxY）。
                width: termLoader.width
                height: termLoader.height
                text: root._terminalText
                // 5WHY (2026-09-05): 曾硬编码 true——meta 的 terminalTypewriter
                // 契约（仅 sysTT/sysGroupedTT 条目开启）被无视，所有检测的
                // 长输出首展都打字机回放。按 C++ 下发的契约值消费。
                typewriter: root.detailData.terminalTypewriter === true
            }
        }
    }
}
