// PageTabBarSection.qml — Config G1-G5 TabBar（重设计：圆形组徽标 + 组名文本）
// 设计要点（UI 评审 · 用户方案细化）：
//   · 选项卡 = 标题栏模式：圆形测试组徽标（28px） + 组名文本；
//   · 徽标即全选/全不选开关，颜色表达三态：
//       全不选 = 灰色不起作用（灰弱底 + 灰图标；组在诊断页整体隐藏）
//       部分选 = 正常色彩（主色弱底 + 主色图标）
//       全选中 = 高亮高对比（实心主色 + 反白图标）
//   · 点击循环：全选 → 全不选；其余 → 全选。热区外扩 ≥24px，悬停微缩放。
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import core
import widgets

PageSection {
    id: root
    backgroundStyle: PageSection.Bar
    fixedHeight: 56   // 5WHY UI-1：Bar 区块显式高度契约

    property int currentGroup: 0
    signal tabChanged(int index)

    property int configPollVersion: AppState.stateVersion

    Row {
        id: tabRow
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: ThemeEngine.spacing.lg
        Layout.rightMargin: ThemeEngine.spacing.lg
        spacing: ThemeEngine.spacing.sm

        Repeater {
            model: 5
            delegate: ItemDelegate {
                id: tabBtn
                // 等宽分配：总宽减去 4 个间距后平均（Row 显式宽度，不依赖布局引擎协商）
                width: (tabRow.width - tabRow.spacing * 4) / 5
                height: tabRow.height
                padding: 0
                property int _pv: root.configPollVersion
                readonly property bool activeTab: root.currentGroup === index
                // 选择三态：2=全选 1=部分 0=全不选
                readonly property int _selMode: {
                    let _ = tabBtn._pv
                    const all = AppState.isGroupAllEnabled(index)
                    const any = AppState.isGroupAnyEnabled(index)
                    return all ? 2 : (any ? 1 : 0)
                }
                readonly property string _groupName: {
                    let _ = tabBtn._pv
                    return T.groupName(index)
                }

                onClicked: root.tabChanged(index)
                Accessible.role: Accessible.Button
                Accessible.name: tabBtn._groupName

                contentItem: RowLayout {
                    // 内边距 10px：徽标与文本不会贴选项卡边缘；左对齐使 5 个
                    // 徽标横向对齐成栅格（等宽选项卡 + 固定内边距，UI 评审）
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: ThemeEngine.spacing.sm
                    // 圆形组徽标 = 全选/全不选开关（颜色三态语义）
                    Rectangle {
                        id: selBadge
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        Layout.alignment: Qt.AlignVCenter
                        radius: 14
                        color: tabBtn._selMode === 2 ? ThemeEngine.colors.primary
                             : tabBtn._selMode === 1 ? Qt.alpha(ThemeEngine.colors.primary, 0.16)
                             : Qt.alpha(ThemeEngine.colors.textMuted, 0.14)
                        scale: badgeArea.pressed ? 0.94
                             : (badgeArea.containsMouse ? 1.08 : 1.0)
                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        AppIcon {
                            anchors.centerIn: parent
                            name: ThemeEngine.groupIconName(index)
                            size: 16
                            color: tabBtn._selMode === 2 ? ThemeEngine.colors.onPrimary
                                 : tabBtn._selMode === 1 ? ThemeEngine.colors.primary
                                 : ThemeEngine.colors.textMuted
                        }
                        MouseArea {
                            id: badgeArea
                            anchors.fill: parent
                            anchors.margins: -4   // 热区外扩（≥24px 命中）
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: {
                                // 全选 → 全不选；其余 → 全选
                                AppState.setGroupEnabled(index, tabBtn._selMode !== 2)
                            }
                        }
                        Accessible.role: Accessible.CheckBox
                        Accessible.checkStateMixed: tabBtn._selMode === 1
                        Accessible.checked: tabBtn._selMode === 2
                        Accessible.name: tabBtn._groupName + " — "
                            + (tabBtn._selMode === 2 ? T.tr("selectAll")
                               : tabBtn._selMode === 1 ? T.tr("partialSelected")
                               : T.tr("deselectAll"))
                    }
                    // 组名文本（全不选时灰色弱化，选中选项卡加粗）
                    Label {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        text: tabBtn._groupName
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.caption
                        font.weight: tabBtn.activeTab ? Font.DemiBold : Font.Normal
                        color: tabBtn._selMode === 0 ? ThemeEngine.colors.textMuted
                             : tabBtn.activeTab ? ThemeEngine.colors.onSurface
                             : ThemeEngine.colors.onSurfaceVariant
                        elide: Text.ElideRight
                    }
                }
                background: Rectangle {
                    color: tabBtn.activeTab ? Qt.alpha(ThemeEngine.colors.primary, 0.12)
                                            : (tabBtn.hovered ? Qt.alpha(ThemeEngine.colors.onSurfaceVariant, 0.06)
                                                              : "transparent")
                    radius: ThemeEngine.radius.md
                    border {
                        width: 1
                        color: tabBtn.activeTab ? Qt.alpha(ThemeEngine.colors.primary, 0.35)
                                                : "transparent"
                    }
                }
            }
        }
    }
}
