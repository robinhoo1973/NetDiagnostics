// PageEmptyStateSection.qml — 空态（§2.7）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Plain

    active: AppState.totalCompleted === 0 && AppState.runStatus !== 1

    ColumnLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: ThemeEngine.spacing.md
        AppIcon {
            Layout.alignment: Qt.AlignHCenter
            name: "compass"
            size: 48
            color: ThemeEngine.colors.textMuted
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: T.tr("noData")
            color: ThemeEngine.colors.textSecondary
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.body
            horizontalAlignment: Text.AlignHCenter
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            visible: AppState.errorMessage !== ""
            text: T.trMsg(AppState.errorMessage)
            color: ThemeEngine.colors.failRed
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.caption
        }
    }
}
