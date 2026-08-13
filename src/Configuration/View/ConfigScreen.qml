// =============================================================================
// ConfigScreen.qml — PageDisplay 子类装配（page-config.md §3）
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import core
import sections as S
import theme
import widgets

PageDisplay {
    id: page
    objectName: "config"

    property int currentGroup: 0
    property int configPollVersion: AppState.stateVersion

    headerContent: [
        S.PageHeaderSection { iconName: "config"; title: T.tr("config") },
        S.PageTabBarSection {
            currentGroup: page.currentGroup
            onTabChanged: function(i) { page.currentGroup = i }
            onGroupActiveToggled: function(i) {
                AppState.setGroupActive(i, !AppState.isGroupActive(i))
            }
        },
        S.PageActionBarSection {
            currentGroup: page.currentGroup
            onSelectAllRequested: AppState.setGroupEnabled(page.currentGroup, true)
            onDeselectAllRequested: AppState.setGroupEnabled(page.currentGroup, false)
        }
    ]

    bodyContent: [
        S.PageListSection {
            model: {
                let _ = page.configPollVersion
                return AppState.allDiagIdsForGroup(page.currentGroup)
            }
            delegate: rowDelegate
        }
    ]

    // ── 行委托：AppIcon + 标题/副标题 + Switch + 分隔线（原 Config ListView 行）──
    Component {
        id: rowDelegate
        Rectangle {
            id: row
            width: ListView.view.width
            height: 56
            color: "transparent"

            property int _pv: page.configPollVersion
            readonly property bool enabled: {
                let _ = row._pv
                return AppState.isDiagEnabled(modelData)
            }

            RowLayout {
                anchors {
                    fill: parent
                    leftMargin: ThemeEngine.spacing.md
                    rightMargin: ThemeEngine.spacing.md
                }
                spacing: ThemeEngine.spacing.sm

                AppIcon {
                    name: row.enabled ? "badge-check" : "circle"
                    size: 18
                    color: row.enabled ? ThemeEngine.colors.primary
                                       : ThemeEngine.colors.textMuted
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: {
                            let _ = row._pv
                            return T.diagName(modelData) || (T.tr("testIdPrefix") + " " + modelData)
                        }
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.body
                        color: ThemeEngine.colors.textPrimary
                        elide: Text.ElideRight
                    }
                    Label {
                        visible: text !== ""
                        text: {
                            let _ = row._pv
                            return T.diagDesc(modelData) || ""
                        }
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.caption
                        color: ThemeEngine.colors.textSecondary
                        elide: Text.ElideRight
                    }
                }
                Switch {
                    checked: row.enabled
                    onToggled: AppState.setDiagEnabled(modelData, checked)
                }
            }

            Rectangle {   // 分隔线
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 1
                color: ThemeEngine.colors.borderCard
            }
        }
    }
}
