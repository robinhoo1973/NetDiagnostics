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
                // 等宽分配：总宽减去 4 个间距后平均（Row 显式宽度，不依赖布局引擎协商）。
                // 5WHY (2026-08-22 首帧异常): 首个委托创建时 tabRow 尚未完成布局
                // （tabRow.width 仍为 0），(0-间距)/5 得负宽——负几何驱动位置器
                // 错位、首个 tab 图标瞬移。Math.max 钳制非负，布局完成后绑定随
                // tabRow.width 重算，首帧即稳定。
                width: Math.max(0, (tabRow.width - tabRow.spacing * 4) / 5)
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
                    // 窄屏纯图标：徽标两侧等宽弹性垫片（fillWidth 平分富余），
                    // 徽标水平居中（5WHY verify 2026-08-17: RowLayout 非填充
                    // 项的单元格=自身宽度，Layout.alignment 沿主轴是空操作——
                    // 徽标曾停在 x=10 处、右侧 24px 空当，纯图标 tab 失衡）
                    Item {
                        visible: ThemeEngine.isCompactUi(root.width)
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                    }
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
                                 : tabBtn._selMode === 1 ? ThemeEngine.colors.iconInk
                                 : ThemeEngine.colors.textMuted
                        }
                        MouseArea {
                            id: badgeArea
                            anchors.fill: parent
                            anchors.margins: -4   // 热区外扩（≥24px 命中）
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: {
                                // 5WHY (2026-08-22 非选中 tab 徽标误触): 徽标
                                // 即全选开关，但点击非当前组的徽标曾直接改写
                                // 其他组的选择状态（未选中 tab 时用户预期是
                                // 切换到该组）。限定：仅当 tab 已选中且正展示
                                // 该组检测项时才切换全选/全不选；否则先切换。
                                if (!tabBtn.activeTab) {
                                    root.tabChanged(index)
                                    return
                                }
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
                    // 组名文本（全不选时灰色弱化，选中选项卡加粗）。
                    // 5WHY (review 2026-08-17, 用户诉求"单纯图标"): 等宽五等分
                    // 在手机竖屏下每 tab 仅 ~59px——完整组名 elide 成 "…"，
                    // 短前缀 "G1".."G5" 仍是文字，均不满足"纯图标"要求。
                    // 窄屏直接隐藏文本、徽标水平居中；完整名称仍由
                    // Accessible.name 供读屏。
                    Label {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        visible: !ThemeEngine.isCompactUi(root.width)
                        text: tabBtn._groupName
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.caption
                        font.weight: tabBtn.activeTab ? Font.DemiBold : Font.Normal
                        color: tabBtn._selMode === 0 ? ThemeEngine.colors.textMuted
                             : tabBtn.activeTab ? ThemeEngine.colors.onSurface
                             : ThemeEngine.colors.onSurfaceVariant
                        elide: Text.ElideRight
                    }
                    // 窄屏纯图标：与左侧垫片对称（fillWidth 平分富余 → 居中）
                    Item {
                        visible: ThemeEngine.isCompactUi(root.width)
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
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
