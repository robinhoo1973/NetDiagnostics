// PageEmptyStateSection.qml — 空态/错误态（§2.7 + M2 错误恢复引导）
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

    // M2：错误态与空态差异化——errorState 显示 badge-error + errorCheck/
    // errorRecoveryHint；hintText 供页面注入引导文案（Dashboard：runFromDiag）
    property bool errorState: false
    property string hintText: ""

    active: AppState.totalCompleted === 0 && AppState.runStatus !== 1

    ColumnLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: ThemeEngine.spacing.md
        AppIcon {
            Layout.alignment: Qt.AlignHCenter
            name: root.errorState ? "badge-error" : "compass"
            size: root.errorState ? 80 : 48
            color: root.errorState ? ThemeEngine.colors.failRed : ThemeEngine.colors.textMuted
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.errorState ? T.tr("errorCheck") : T.tr("noData")
            color: root.errorState ? ThemeEngine.colors.failRed : ThemeEngine.colors.textSecondary
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.body
            font.weight: root.errorState ? Font.DemiBold : Font.Normal
            horizontalAlignment: Text.AlignHCenter
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 320
            visible: root.errorState || root.hintText !== ""
            text: root.errorState
                ? (AppState.errorMessage !== "" ? T.trMsg(AppState.errorMessage) : T.tr("errorRecoveryHint"))
                : root.hintText
            color: ThemeEngine.colors.textSecondary
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.caption
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
