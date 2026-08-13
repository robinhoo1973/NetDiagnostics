// PageTabBarSection.qml — Config G1-G5 TabBar（page-config.md §2.1）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Bar
    fixedHeight: 44   // 5WHY UI-1：Bar 区块显式高度契约

    property int currentGroup: 0
    signal tabChanged(int index)
    signal groupActiveToggled(int index)

    property int configPollVersion: AppState.stateVersion

    RowLayout {
        Layout.fillWidth: true
        spacing: 4

        Repeater {
            model: 5
            delegate: ItemDelegate {
                id: tabBtn
                Layout.fillWidth: true
                Layout.fillHeight: true
                property int _pv: root.configPollVersion
                readonly property bool activeTab: root.currentGroup === index
                readonly property bool groupActive: {
                    let _ = tabBtn._pv
                    return AppState.isGroupActive(index)
                }

                onClicked: root.tabChanged(index)

                contentItem: RowLayout {
                    spacing: 6
                    // 激活点（大热区；不与相邻 ItemDelegate 重叠误触）
                    Rectangle {
                        id: dot
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        radius: 7
                        color: tabBtn.groupActive ? ThemeEngine.colors.primary : "transparent"
                        border {
                            width: 1
                            color: tabBtn.groupActive ? ThemeEngine.colors.primary
                                                      : ThemeEngine.colors.textMuted
                        }
                        MouseArea {
                            anchors.margins: -17
                            propagateComposedEvents: false
                            onClicked: function(mouse) {
                                mouse.accepted = true
                                root.groupActiveToggled(index)
                            }
                        }
                    }
                    Label {
                        text: {
                            let _ = tabBtn._pv
                            return T.groupPrefix(index)
                        }
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: ThemeEngine.fontSize.body
                        color: tabBtn.activeTab ? ThemeEngine.colors.textPrimary
                                                : ThemeEngine.colors.textSecondary
                    }
                }
                background: Rectangle {
                    color: tabBtn.activeTab ? Qt.alpha(ThemeEngine.colors.primary, 0.12)
                                            : "transparent"
                    radius: ThemeEngine.radius.md
                }
            }
        }
    }
}
