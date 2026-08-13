// =============================================================================
// PremiumDialog.qml — 一次性买断 Premium 弹窗（PremiumStore 后端恢复）
//
// 购买/恢复经 PremiumStore（StoreKit on iOS/macOS；其它平台诚实降级）。
// 仅 Premium 平台（移动/Apple）在 SettingsScreen 通过 Loader 实例化。
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(ThemeEngine.colors.surface, 0.85)
    visible: root.open
    z: 1200

    property bool open: false
    property bool isMobile: false
    property string statusText: ""      // 瞬时提示（恢复/不可用）
    signal dismissed()

    function openDialog() {
        root.open = true
        root.statusText = ""
        // 5WHY（iOS b21294）：非阻塞探针恢复——不置 purchaseInProgress，
        // Buy/Restore 按钮在探测期间保持可点。
        if (!PremiumStore.isPremium) PremiumStore.probeRestore()
    }
    function closeDialog() {
        root.open = false
        root.dismissed()
    }

    // ── PremiumStore 信号（购买/恢复反馈闭环）──
    Connections {
        target: PremiumStore
        function onPremiumChanged() {
            if (PremiumStore.isPremium) { root.statusText = ""; root.closeDialog() }
        }
        function onPurchaseFailed() { root.statusText = T.tr("purchaseFailed") }
        function onPurchaseDeferred() { root.statusText = T.tr("purchaseDeferredMsg") }
        function onRestoreCompleted(restoredAny, isError) {
            if (restoredAny) root.statusText = T.tr("purchasesRestored")
            else if (isError) root.statusText = T.tr("purchaseFailed")
        }
    }

    // 背景点击关闭
    MouseArea {
        anchors.fill: parent
        onClicked: root.closeDialog()
    }

    // ── 卡片 ──
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        height: Math.min(parent.height - 48, bodyCol.implicitHeight + 2 * ThemeEngine.spacing.xl)
        radius: ThemeEngine.radius.xl
        color: ThemeEngine.colors.card
        border { width: 1; color: ThemeEngine.colors.borderCard }

        ColumnLayout {
            id: bodyCol
            anchors.fill: parent
            anchors.margins: ThemeEngine.spacing.xl
            spacing: ThemeEngine.spacing.md

            // 关闭
            AppIcon {
                Layout.alignment: Qt.AlignRight
                name: "chevron-right"; size: 18
                rotation: 90
                color: ThemeEngine.colors.textMuted
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeDialog()
                }
                Accessible.role: Accessible.Button
                Accessible.name: T.tr("dialogCancel")
            }

            AppIcon {
                Layout.alignment: Qt.AlignHCenter
                name: "zap"; size: 40
                color: ThemeEngine.colors.primary
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: T.tr("premiumHero")
                font.family: ThemeEngine.fontUi
                font.pixelSize: 19
                font.weight: Font.Bold
                color: ThemeEngine.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: T.tr("premiumOneTime")
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.body
                color: ThemeEngine.colors.textSecondary
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: ["premiumFeatureLifetime", "premiumFeaturePdf", "premiumFeatureHtml"]
                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: ThemeEngine.spacing.sm
                    AppIcon {
                        name: "badge-check"; size: 18
                        color: ThemeEngine.colors.passGreen
                    }
                    Label {
                        Layout.fillWidth: true
                        text: T.tr(modelData)
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.body
                        color: ThemeEngine.colors.textPrimary
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.statusText !== ""
                text: root.statusText
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.caption
                color: ThemeEngine.colors.warnYellow
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: T.tr("subscribeBtn")
                onClicked: PremiumStore.requestSubscription()
            }
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: T.tr("restoreBtn")
                flat: true
                onClicked: PremiumStore.restorePurchases()
            }
        }
    }
}
