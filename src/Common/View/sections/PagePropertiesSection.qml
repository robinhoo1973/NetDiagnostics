// PagePropertiesSection.qml — DetailPage 属性列表（page-detail.md §2.5）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    bottomMargin: ThemeEngine.spacing.lg
    contentSpacing: 6

    property var detailData: ({})
    readonly property bool _hasProperties: (detailData.properties || []).length > 0
    active: _hasProperties && detailData.showProperties !== false
    property bool _expanded: true

    ColumnLayout {
        spacing: 0
        CollapsibleSectionHeader {
            Layout.fillWidth: true
            title: T.tr("detailProperties")
            expanded: root._expanded
            onToggleRequested: root._expanded = !root._expanded
        }
        Repeater {
            model: root._expanded ? (detailData.properties || []) : []
            delegate: ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                RowLayout {
                    Layout.fillWidth: true
                    spacing: ThemeEngine.spacing.sm
                    Label {
                        text: modelData.label || ""
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.body
                        color: ThemeEngine.colors.textSecondary
                        Layout.preferredWidth: Math.min(180, implicitWidth)
                        elide: Text.ElideRight
                    }
                    Label {
                        text: modelData.value || ""
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: ThemeEngine.fontSize.body
                        color: ThemeEngine.colors.textPrimary
                        Layout.fillWidth: true
                        wrapMode: Text.WrapAnywhere
                    }
                }
                Repeater {
                    model: modelData.children || []
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: ThemeEngine.spacing.lg
                        spacing: ThemeEngine.spacing.sm
                        Label {
                            text: "· " + (modelData.label || "")
                            font.family: ThemeEngine.fontUi
                            font.pixelSize: ThemeEngine.fontSize.caption
                            color: ThemeEngine.colors.textMuted
                        }
                        Label {
                            text: modelData.value || ""
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: ThemeEngine.fontSize.caption
                            color: ThemeEngine.colors.textSecondary
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }
        }
    }
}
