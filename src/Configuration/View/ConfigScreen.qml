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

    // ── 行委托（UI 评审重设计）：状态图标 + 标题/副标题 + 紧凑开关 + 分隔线 ──
    //   对齐：行高 60，左右元素 Layout.alignment 垂直居中；整行可点击切换。
    Component {
        id: rowDelegate
        Rectangle {
            id: row
            width: ListView.view.width
            height: 60
            color: rowMouse.containsMouse ? Qt.alpha(ThemeEngine.colors.primary, 0.04) : "transparent"

            property int _pv: page.configPollVersion
            readonly property bool enabled: {
                let _ = row._pv
                return AppState.isDiagEnabled(modelData)
            }

            // 整行点击切换（Switch 在其上层，不冲突）
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: AppState.setDiagEnabled(modelData, !row.enabled)
            }

            // 锚点定位（不经布局引擎）：图标左对齐、开关右对齐，垂直中线一致
            AppIcon {
                id: rowIcon
                anchors { left: parent.left; leftMargin: ThemeEngine.spacing.lg; verticalCenter: parent.verticalCenter }
                width: 20
                height: 20
                name: row.enabled ? "badge-check" : "circle"
                size: 18
                color: row.enabled ? ThemeEngine.colors.primary
                                   : ThemeEngine.colors.textMuted
            }
            ColumnLayout {
                anchors {
                    left: rowIcon.right; leftMargin: ThemeEngine.spacing.md
                    right: rowSwitch.left; rightMargin: ThemeEngine.spacing.md
                    verticalCenter: parent.verticalCenter
                }
                spacing: 2
                Label {
                    Layout.fillWidth: true   // 5WHY (2026-08-22): ColumnLayout 子项不 fillWidth 时按隐式宽度排布——长文本溢出布局框压到开关上；fillWidth 后受锚定宽度约束，elide 生效
                    text: {
                        let _ = row._pv
                        return T.diagName(modelData) || (T.tr("testIdPrefix") + " " + modelData)
                    }
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.body
                    color: row.enabled ? ThemeEngine.colors.onSurface
                                       : ThemeEngine.colors.onSurfaceVariant
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: {
                        let _ = row._pv
                        return T.diagDesc(modelData) || ""
                    }
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.caption
                    color: ThemeEngine.colors.onSurfaceVariant
                    // 5WHY (2026-08-22 竖屏重叠): 描述文本曾无宽度约束、
                    // 无换行——implicitWidth 超出布局框后与开关控件重叠。
                    // fillWidth 限定宽度 + WordWrap 换行 + 两行上限。
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }
            ToggleSwitch {
                id: rowSwitch
                anchors { right: parent.right; rightMargin: ThemeEngine.spacing.lg; verticalCenter: parent.verticalCenter }
                width: 36
                height: 20
                checked: row.enabled
                onToggled: AppState.setDiagEnabled(modelData, checked)
                // NEW-22：开关读屏名 = 检测名 + 后缀
                Accessible.name: (T.diagName(modelData) || ("#" + modelData)) + " " + T.tr("accDiagnosticSuffix")
            }

            Rectangle {   // 分隔线（左右各 16px 缩进，与行边距一致）
                anchors {
                    left: parent.left; right: parent.right; bottom: parent.bottom
                    leftMargin: ThemeEngine.spacing.lg
                    rightMargin: ThemeEngine.spacing.lg
                }
                height: 1
                color: Qt.alpha(ThemeEngine.colors.outlineVariant, 0.7)
            }
        }
    }
}
