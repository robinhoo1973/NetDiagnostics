// PageHeroSection.qml — DetailPage 英雄卡（page-detail.md §2.2）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    topMargin: ThemeEngine.spacing.lg
    bottomMargin: ThemeEngine.spacing.sm
    minHeight: 80
    cardRadius: ThemeEngine.radius.lg

    property var detailData: ({})   // NEW-14：data 遮蔽内建属性，统一 detailData

    // 图标名/tint 各求值一次（5WHY review 2026-08-17：原代码在填充与边框
    // 两处各调一次 iconPadTint——每次 detailData 刷新双份查找+转换）
    // 5WHY (review round 2, UX): 垫只承载真实诊断图标——iconName 缺失时
    // 回退状态徽标会与旁边状态圆盘同图形异色冲突（蓝 check 对绿 check）。
    readonly property bool _hasDiagIcon: detailData.iconName !== undefined && detailData.iconName !== ""
    readonly property string _iconName: _hasDiagIcon ? detailData.iconName : "circle"

    RowLayout {
        Layout.fillWidth: true
        spacing: ThemeEngine.spacing.md

        // 45 图标全彩常显：诊断图标光晕垫（共享 IconPad——tint 由组件从
        // iconName 派生，调用方不再维护 _padTint）
        IconPad {
            visible: root._hasDiagIcon
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            iconName: root._iconName
            iconSize: 40
            iconColor: ThemeEngine.colors.primary
        }

        // 状态圆盘
        Rectangle {
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            radius: 22
            color: Qt.alpha(ThemeEngine.statusColors[(detailData.status !== undefined ? detailData.status : 5)] || ThemeEngine.colors.skip, 0.14)
            AppIcon {
                anchors.centerIn: parent
                name: detailData.statusIcon || ThemeEngine.statusIconNames[(detailData.status !== undefined ? detailData.status : 5)] || "badge-info"
                size: 24
                color: ThemeEngine.statusColors[(detailData.status !== undefined ? detailData.status : 5)] || ThemeEngine.colors.skip
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                text: detailData.displayName || ""
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.title
                font.weight: Font.Bold
                color: ThemeEngine.colors.onSurface
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Label {
                visible: text !== ""
                text: detailData.summary || ""
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.body
                color: ThemeEngine.colors.onSurfaceVariant
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
            Label {
                visible: detailData.durationMs !== undefined && detailData.durationMs > 0
                text: ThemeEngine.formatDuration(detailData.durationMs || 0)
                font.family: ThemeEngine.monoFont
                font.pixelSize: ThemeEngine.fontSize.caption
                color: ThemeEngine.colors.textMuted
            }
        }
    }
}
