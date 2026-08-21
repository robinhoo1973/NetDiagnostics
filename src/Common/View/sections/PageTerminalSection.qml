// PageTerminalSection.qml — DetailPage 终端输出（page-detail.md §2.7；TerminalBlock）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import detail
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    bottomMargin: ThemeEngine.spacing.lg
    contentSpacing: 6
    // 主题自适应（7-5）：暗色=surface，亮色=input——不再硬编码深藏青
    cardColor: ThemeEngine.terminalBg   // 属性（主题切换可响应）

    property var detailData: ({})
    readonly property string _terminalText: detailData.details || detailData.rawOutput || ""
    readonly property int _terminalLines: _terminalText === "" ? 0 : _terminalText.split('\n').length
    active: _terminalLines > 0 && detailData.showTerminal !== false

    ColumnLayout {
        spacing: ThemeEngine.spacing.xs
        Label {
            text: T.tr("detailTerminal")
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.caption
            color: ThemeEngine.colors.onSurfaceVariant
        }
        Loader {
            id: termLoader
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(360, Math.max(72, root._terminalLines * 18 + 24))
            sourceComponent: TerminalBlock {
                // 5WHY (2026-08-22, issue 2): TerminalBlock 的 implicitHeight 跟随
                // 全文行数（长输出可达 1200px+），溢出 Loader 的钳制视口后，内部
                // Flickable 视口=全文高度、interactive=false 永不滚动，末尾行被
                // 后序区块覆盖。必须把组件尺寸显式钉住 = Loader 视口，内部滚动
                // 才接管（独立 harness 验证：viewport 336 → contentY 可达 869=maxY）。
                width: termLoader.width
                height: termLoader.height
                text: root._terminalText
                typewriter: true
            }
        }
    }
}
