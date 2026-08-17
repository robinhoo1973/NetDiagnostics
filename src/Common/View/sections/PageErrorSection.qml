// PageErrorSection.qml — DetailPage 错误块（page-detail.md §2.4）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    bottomMargin: ThemeEngine.spacing.sm
    cardColor: Qt.alpha(ThemeEngine.colors.fail, 0.06)
    borderColor: Qt.alpha(ThemeEngine.colors.fail, 0.5)

    property var detailData: ({})
    readonly property bool _hasError: (detailData.errorOutput || "") !== ""
    active: _hasError && detailData.showErrorOutput !== false

    Label {
        Layout.fillWidth: true
        text: detailData.errorOutput || ""
        color: ThemeEngine.colors.fail
        font.family: ThemeEngine.fontUi
        font.pixelSize: ThemeEngine.fontSize.body
        wrapMode: Text.WordWrap
    }
}
