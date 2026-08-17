// PageCardSection.qml — 通用可折叠卡（title+icon+content）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    bottomMargin: ThemeEngine.spacing.sm

    property string cardIcon: ""
    property string cardTitle: ""
    property bool collapsible: true
    property bool showHeader: true     // 归档恢复：无头卡（Run Info 等）
    property bool _userToggled: false
    property bool _userExpanded: true
    readonly property bool expanded: _userToggled ? _userExpanded : true
    signal headerClicked()

    ColumnLayout {
        spacing: 0
        // 折叠头
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            visible: root.showHeader
            RowLayout {
                anchors.fill: parent
                spacing: ThemeEngine.spacing.sm
                AppIcon {
                    visible: root.cardIcon !== ""
                    name: root.cardIcon; size: 16
                    color: ThemeEngine.colors.tertiary
                }
                Label {
                    text: root.cardTitle
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.subhead
                    font.weight: Font.DemiBold
                    color: ThemeEngine.colors.onSurface
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                AppIcon {
                    visible: root.collapsible
                    name: "chevron-down"; size: 14
                    color: ThemeEngine.colors.textMuted
                    rotation: expanded ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: 150 } }
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: root.collapsible ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: {
                    if (!root.collapsible) return
                    root._userToggled = true
                    root._userExpanded = !root._userExpanded
                    root.headerClicked()
                }
            }
        }
        // 内容（用户子项经 default property 进入）
        ColumnLayout {
            id: contentCol
            Layout.fillWidth: true
            visible: root.expanded
            spacing: root.contentSpacing
        }
    }
    default property alias content: contentCol.data
}
