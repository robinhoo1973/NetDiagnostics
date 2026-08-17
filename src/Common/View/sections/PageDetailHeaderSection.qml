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
        // 头部操作按钮：≥44px 命中区 + 键盘可达（共享 IconActionButton；
        // 5WHY simplify 2026-08-17：该脚手架曾在两处复制、第三处漏键盘支持）
        IconActionButton {
            id: backBtn
            Layout.preferredWidth: 44
            Layout.fillHeight: true
            iconName: "chevron-right"
            iconSize: 20
            iconMirror: true   // 返回箭头
            Accessible.name: T.tr("accCloseDetails")
            onActivated: root.backRequested()
        }
        Label {
            text: root.detail.diagId !== undefined
                  ? (T.diagName(root.detail.diagId) || root.title)
                  : root.title
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.subhead
            font.weight: Font.DemiBold
            color: ThemeEngine.colors.onSurface
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        IconActionButton {
            id: copyBtn
            Layout.preferredWidth: 44
            Layout.fillHeight: true
            iconName: "clipboard"
            iconSize: 18
            Accessible.name: T.tr("detailCopy")
            onActivated: root.copyRequested()
        }
    }
}
