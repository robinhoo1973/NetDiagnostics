// PageActionBarSection.qml — Config 动作条（page-config.md §2.2）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    fixedHeight: 60   // 5WHY UI-1

    property int currentGroup: 0
    signal selectAllRequested()
    signal deselectAllRequested()

    property int configPollVersion: AppState.stateVersion

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: ThemeEngine.spacing.md
        Layout.rightMargin: ThemeEngine.spacing.md
        spacing: ThemeEngine.spacing.sm

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                text: {
                    let _ = root.configPollVersion
                    return T.groupName(root.currentGroup)
                }
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.subhead
                font.weight: Font.DemiBold
                color: ThemeEngine.colors.textPrimary
                elide: Text.ElideRight
            }
            Label {
                text: {
                    let _ = root.configPollVersion
                    return AppState.diagCountForGroup(root.currentGroup) + " " + T.tr("diagsSuffix")
                }
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.caption
                color: ThemeEngine.colors.textSecondary
            }
        }
        Button {
            text: T.tr("selectAll")
            enabled: {
                let _ = root.configPollVersion
                return !AppState.isGroupAllEnabled(root.currentGroup)
            }
            onClicked: root.selectAllRequested()
        }
        Button {
            text: T.tr("deselectAll")
            enabled: {
                let _ = root.configPollVersion
                return AppState.isGroupAnyEnabled(root.currentGroup)
            }
            onClicked: root.deselectAllRequested()
        }
    }
}
