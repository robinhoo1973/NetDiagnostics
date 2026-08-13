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

    RowLayout {
        Layout.fillWidth: true
        spacing: ThemeEngine.spacing.md

        // 状态圆盘
        Rectangle {
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            radius: 22
            color: Qt.alpha(ThemeEngine.statusColors[(detailData.status !== undefined ? detailData.status : 5)] || ThemeEngine.colors.skipGray, 0.14)
            AppIcon {
                anchors.centerIn: parent
                name: detailData.statusIcon || ThemeEngine.statusIconNames[(detailData.status !== undefined ? detailData.status : 5)] || "badge-info"
                size: 24
                color: ThemeEngine.statusColors[(detailData.status !== undefined ? detailData.status : 5)] || ThemeEngine.colors.skipGray
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
                color: ThemeEngine.colors.textPrimary
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Label {
                visible: text !== ""
                text: detailData.summary || ""
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.body
                color: ThemeEngine.colors.textSecondary
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
