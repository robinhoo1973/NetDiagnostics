// PageDetailHeaderSection.qml — DetailPage 头（返回+标题+复制）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Bar
    fixedHeight: 56   // 8-16：Material 顶栏最佳实践 56px（原 implicit 约 36 过矮）

    property string title: ""
    property string iconName: "circle"
    property var detail: ({})               // DetailPage 注入（diagId → T.diagName）
    signal backRequested()
    signal copyRequested()

    RowLayout {
        id: headerRow
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: ThemeEngine.spacing.sm
        Layout.rightMargin: ThemeEngine.spacing.sm
        spacing: ThemeEngine.spacing.sm
        AppIcon {
            name: "chevron-right"; size: 20
            color: ThemeEngine.colors.textSecondary
            mirror: true   // 返回箭头
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.backRequested()
            }
            Accessible.role: Accessible.Button
            Accessible.name: T.tr("accCloseDetails")
        }
        AppIcon { name: root.iconName; size: 18; color: ThemeEngine.colors.cyan }
        Label {
            text: root.detail.diagId !== undefined
                  ? (T.diagName(root.detail.diagId) || root.title)
                  : root.title
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.subhead
            font.weight: Font.DemiBold
            color: ThemeEngine.colors.textPrimary
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        AppIcon {
            name: "clipboard"; size: 18
            color: ThemeEngine.colors.textSecondary
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.copyRequested()
            }
            Accessible.role: Accessible.Button
            Accessible.name: T.tr("detailCopy")
        }
    }
}
