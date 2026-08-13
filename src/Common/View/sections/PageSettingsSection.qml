// PageSettingsSection.qml — Settings 分组（SectionHeader+卡；page-settings.md §2.1）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    bottomMargin: ThemeEngine.spacing.xl   // 页面装配可覆写

    property string iconName: ""
    property string title: ""

    // 行1：SectionHeader（icon 块 + 标题）
    RowLayout {
        Layout.fillWidth: true
        Rectangle {
            implicitWidth: 30; implicitHeight: 30
            radius: 8
            color: Qt.alpha(ThemeEngine.colors.primary, 0.1)
            AppIcon {
                anchors.centerIn: parent
                name: root.iconName; size: 18
                color: ThemeEngine.colors.textPrimary
            }
        }
        Item { Layout.preferredWidth: ThemeEngine.spacing.md }
        Label {
            text: root.title
            font.family: ThemeEngine.fontUi
            font.pixelSize: 16
            font.weight: Font.DemiBold
            color: ThemeEngine.colors.textPrimary
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
    }
    Item { Layout.preferredHeight: ThemeEngine.spacing.md }
    // 行2：卡内容（Layout 边距；基类 Card paddingH=12 → 4+12=16px 有效边距）
    ColumnLayout {
        id: cardBody
        Layout.fillWidth: true
        Layout.leftMargin: 4
        Layout.rightMargin: 4
        spacing: 0
    }
    default property alias content: cardBody.data
}
