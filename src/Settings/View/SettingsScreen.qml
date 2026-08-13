// =============================================================================
// SettingsScreen.qml — PageDisplay 子类装配（page-settings.md §3）
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import core
import sections as S
import theme
import widgets
import dialogs

PageDisplay {
    id: page
    objectName: "settings"

    // ── Toast（NEW-7）──
    property string toastText: ""
    function showToast(msg) {
        toastText = msg
        toastTimer.restart()
    }
    Timer {
        id: toastTimer
        interval: ThemeEngine.toastDurationMs
        onTriggered: page.toastText = ""
    }

    // 启动时同步主题（AppState 持久化值 → ThemeEngine）
    Component.onCompleted: ThemeEngine.mode = AppState.themeMode

    headerContent: [
        S.PageHeaderSection { iconName: "gear"; title: T.tr("settings") }
    ]

    bodyContent: [
        // ── Appearance ──
        S.PageSettingsSection {
            iconName: "brightness"
            title: T.tr("appearanceSection")
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                Layout.bottomMargin: 8
                spacing: ThemeEngine.spacing.sm
                ButtonGroup { id: themeGroup }   // 互斥：防止双钮同时 checked
                Button {
                    Layout.fillWidth: true
                    text: T.tr("themeLight")
                    checkable: true
                    checked: AppState.themeMode === 1
                    ButtonGroup.group: themeGroup
                    onClicked: {
                        AppState.setThemeMode(1)
                        ThemeEngine.mode = ThemeEngine.litMode
                    }
                }
                Button {
                    Layout.fillWidth: true
                    text: T.tr("themeDark")
                    checkable: true
                    checked: AppState.themeMode === 2
                    ButtonGroup.group: themeGroup
                    onClicked: {
                        AppState.setThemeMode(2)
                        ThemeEngine.mode = ThemeEngine.drkMode
                    }
                }
            }
        },

        // ── Language ──
        S.PageSettingsSection {
            iconName: "translate"
            title: T.tr("languageSection")
            ComboBox {
                Layout.fillWidth: true
                Layout.margins: 8
                model: AppState.langItems
                currentIndex: AppState.languageIndex
                onActivated: function(i) {
                    AppState.setLanguage(i)
                    page.showToast(T.tr("languageSection"))
                }
            }
        },

        // ── Premium（移动/Apple 平台门控；Windows/Linux 隐藏）──
        S.PageSettingsSection {
            iconName: "zap"
            title: T.tr("premiumSection")
            active: AppState.isPremiumPlatform
            ColumnLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 8
                spacing: ThemeEngine.spacing.sm
                Label {
                    Layout.fillWidth: true
                    text: T.tr("premiumCardTitle")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.body
                    color: ThemeEngine.colors.textSecondary
                    wrapMode: Text.WordWrap
                }
                Button {
                    text: T.tr("premiumUnlockedBtn")
                    onClicked: if (premiumLoader.item) premiumLoader.item.openDialog()
                }
            }
        },

        // ── About ──
        S.PageSettingsSection {
            iconName: "info"
            title: T.tr("aboutSection")
            bottomMargin: 0
            ColumnLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 8
                spacing: ThemeEngine.spacing.sm
                AppIcon {
                    Layout.alignment: Qt.AlignHCenter
                    name: "compass"; size: 48
                    color: ThemeEngine.colors.primary
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: T.tr("appName")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.title
                    font.weight: Font.Bold
                    color: ThemeEngine.colors.textPrimary
                }
                Label {
                    Layout.fillWidth: true
                    text: T.tr("aboutDesc")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.body
                    color: ThemeEngine.colors.textSecondary
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: T.tr("versionLabel") + " " + Qt.application.version
                    font.family: ThemeEngine.monoFont
                    font.pixelSize: ThemeEngine.fontSize.caption
                    color: ThemeEngine.colors.textMuted
                }
            }
        }
    ]

    floatingContent: [
        S.PageToastSection { toastText: page.toastText },
        // Premium 弹窗（仅 Premium 平台实例化；无 Store 后端时按钮诚实降级）
        Loader {
            id: premiumLoader
            anchors.fill: parent
            active: AppState.isPremiumPlatform
            sourceComponent: premiumDialogComp
            z: 1200
        }
    ]

    Component {
        id: premiumDialogComp
        PremiumDialog { isMobile: ThemeEngine.isMobile }
    }
}
