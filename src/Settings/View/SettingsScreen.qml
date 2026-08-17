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

    // NEW-8/M10：Premium 弹窗打开时阻断 dock 导航/滑动切页
    overlayVisible: premiumLoader.item !== null && premiumLoader.item.visible
    // OverlayHost 契约（5WHY review round 3）：此前缺实现——弹窗打开时 dock
    // 点击/滑动被 navBlocked 吞掉且无法经 AppContent 关闭
    function closeOverlay() {
        if (premiumLoader.item !== null && premiumLoader.item.visible)
            premiumLoader.item.closeDialog()
    }

    // ── PremiumStore 信号 → toast（恢复确认 / 购买失败）──
    Connections {
        target: PremiumStore
        function onRestoreCompleted(restoredAny, isError) {
            if (premiumLoader.item !== null && premiumLoader.item.visible) return  // 弹窗内已提示
            if (restoredAny) page.showToast(T.tr("purchasesRestored"))
            else if (isError) page.showToast(T.tr("purchaseFailed"))
        }
    }

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
                Layout.preferredHeight: 40
                Layout.bottomMargin: 8
                spacing: ThemeEngine.spacing.sm
                Repeater {
                    model: [
                        { label: T.tr("themeLight"), mode: 1, icon: "brightness" },
                        { label: T.tr("themeDark"),  mode: 2, icon: "moon" }
                    ]
                    delegate: Rectangle {
                        // 归档视觉恢复：图标+文字瓷块，激活态 primaryContainer + 主色描边
                        readonly property bool isActive: AppState.themeMode === modelData.mode
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        radius: ThemeEngine.radius.md
                        color: isActive ? ThemeEngine.colors.primaryContainer
                             : themeBtnArea.containsMouse ? Qt.alpha(ThemeEngine.colors.primary, 0.06)
                             : "transparent"
                        Behavior on color { ColorAnimation { duration: 150 } }
                        border {
                            width: 1
                            color: isActive ? ThemeEngine.colors.primary
                                 : themeBtnArea.containsMouse ? Qt.alpha(ThemeEngine.colors.primary, 0.3)
                                 : ThemeEngine.colors.outlineVariant
                        }
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: ThemeEngine.spacing.sm
                            AppIcon {
                                name: modelData.icon; size: 16
                                color: isActive ? ThemeEngine.colors.primary : ThemeEngine.colors.onSurfaceVariant
                            }
                            Label {
                                text: modelData.label
                                font.family: ThemeEngine.fontUi
                                font.pixelSize: ThemeEngine.fontSize.caption
                                font.weight: isActive ? Font.DemiBold : Font.Normal
                                color: isActive ? ThemeEngine.colors.primary : ThemeEngine.colors.onSurfaceVariant
                            }
                        }
                        MouseArea {
                            id: themeBtnArea
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: {
                                AppState.setThemeMode(modelData.mode)
                                ThemeEngine.mode = modelData.mode
                            }
                        }
                        activeFocusOnTab: true
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                                AppState.setThemeMode(modelData.mode)
                                ThemeEngine.mode = modelData.mode
                            }
                        }
                        Accessible.name: modelData.label
                        Accessible.role: Accessible.Button
                        Accessible.description: isActive ? T.tr("accActiveTheme") : T.tr("accSwitchTheme")
                    }
                }
            }
        },

        // ── Language ──
        S.PageSettingsSection {
            iconName: "translate"
            title: T.tr("languageSection")
            ComboBox {
                id: langCombo
                Layout.fillWidth: true
                Layout.margins: 8
                Layout.preferredHeight: 44
                model: AppState.langItems
                currentIndex: AppState.languageIndex
                onActivated: function(i) {
                    AppState.setLanguage(i)
                    page.showToast(AppState.langItems[i] + " ✓")   // M3：显示所选语言名
                }
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.body
                background: Rectangle {
                    radius: 6
                    color: ThemeEngine.colors.surfaceContainerHighest
                    border { width: 1; color: ThemeEngine.colors.outlineVariant }
                }
                contentItem: Label {
                    text: langCombo.displayText
                    font: langCombo.font
                    color: ThemeEngine.colors.onSurface
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: T.isRtl ? 0 : 12
                    rightPadding: T.isRtl ? 12 : 0
                }
                indicator: AppIcon {
                    anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                    name: "chevron-down"; size: 12
                    color: ThemeEngine.colors.onSurfaceVariant
                }
                delegate: ItemDelegate {
                    width: langCombo.width
                    contentItem: Label {
                        text: modelData
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.body
                        color: highlighted ? ThemeEngine.colors.tertiary : ThemeEngine.colors.onSurface
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: T.isRtl ? 0 : 12
                        rightPadding: T.isRtl ? 12 : 0
                    }
                    background: Rectangle {
                        color: highlighted ? Qt.alpha(ThemeEngine.colors.tertiary, 0.1) : "transparent"
                    }
                }
                popup: Popup {
                    y: langCombo.height + 4
                    width: langCombo.width
                    height: Math.min(implicitHeight, 280)
                    padding: 4
                    background: Rectangle {
                        radius: 8
                        color: ThemeEngine.colors.surfaceContainerLow
                        border { width: 1; color: ThemeEngine.colors.outlineVariant }
                    }
                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: langCombo.popup.visible ? langCombo.delegateModel : null
                        currentIndex: langCombo.highlightedIndex
                    }
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
                PremiumCard {
                    onPurchaseRequested: if (premiumLoader.item) premiumLoader.item.openDialog()
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
                    color: ThemeEngine.colors.onSurface
                }
                Label {
                    Layout.fillWidth: true
                    text: T.tr("aboutDesc")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.body
                    color: ThemeEngine.colors.onSurfaceVariant
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
                // M4：版本细节（AppState 编译期信息）
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 2
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: T.tr("versionLabel") + " " + AppState.appVersion()
                              + " · " + AppState.appEdition()
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: ThemeEngine.fontSize.caption
                        color: ThemeEngine.colors.textMuted
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: "build " + AppState.buildNumber() + " · " + AppState.gitHash()
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: ThemeEngine.fontSize.micro
                        color: ThemeEngine.colors.textMuted
                    }
                }
                // AboutRow×4（归档能力矩阵）
                Repeater {
                    model: [
                        { icon: "devices", key: "crossPlat" },
                        { icon: "activity", key: "realtimeDiag" },
                        { icon: "file-html", key: "detailedReport" },
                        { icon: "moon", key: "darkTheme" }
                    ]
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: ThemeEngine.spacing.sm
                        AppIcon { name: modelData.icon; size: 16; color: ThemeEngine.colors.primary }
                        Label {
                            Layout.fillWidth: true
                            text: T.tr(modelData.key)
                            font.family: ThemeEngine.fontUi
                            font.pixelSize: ThemeEngine.fontSize.caption
                            color: ThemeEngine.colors.onSurface
                            wrapMode: Text.WordWrap
                        }
                    }
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
