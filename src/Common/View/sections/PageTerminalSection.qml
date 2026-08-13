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
    // NEW-18：terminalSurface 为已知硬编码例外（现状迁移保留）
    cardColor: ThemeEngine.isDark ? ThemeEngine.colors.surface : "#1E293B"

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
            color: ThemeEngine.colors.textSecondary
        }
        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(360, Math.max(72, root._terminalLines * 18 + 24))
            sourceComponent: TerminalBlock {
                text: root._terminalText
                typewriter: true
                terminalColor: ThemeEngine.isDark ? "#D4E3F5" : "#0F172A"
            }
        }
    }
}
