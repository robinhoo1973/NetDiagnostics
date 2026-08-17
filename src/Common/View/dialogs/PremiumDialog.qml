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

    // 5WHY (review 2026-08-17): 弹窗缺 LayoutMirroring——阿拉伯语下关闭按钮
    // 停留在错误视觉边；PremiumCard 已有镜像，弹窗补齐。
    LayoutMirroring.enabled: T.isRtl
    LayoutMirroring.childrenInherit: true

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
        color: ThemeEngine.colors.surfaceContainerLow
        border { width: 1; color: ThemeEngine.colors.outlineVariant }

        ColumnLayout {
            id: bodyCol
            anchors.fill: parent
            anchors.margins: ThemeEngine.spacing.xl
            spacing: ThemeEngine.spacing.md

            // 关闭：共享 IconActionButton（5WHY simplify 2026-08-17：本处是
            // 该脚手架的第三份复制，且曾漏掉键盘支持——现已统一）
            IconActionButton {
                Layout.alignment: Qt.AlignRight
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                iconName: "close"
                iconSize: 18
                iconColor: ThemeEngine.colors.textMuted
                Accessible.name: T.tr("dialogCancel")
                onActivated: root.closeDialog()
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
                color: ThemeEngine.colors.onSurface
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: T.tr("premiumOneTime")
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.body
                color: ThemeEngine.colors.onSurfaceVariant
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
                        color: ThemeEngine.colors.success
                    }
                    Label {
                        Layout.fillWidth: true
                        text: T.tr(modelData)
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: ThemeEngine.fontSize.body
                        color: ThemeEngine.colors.onSurface
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
                color: ThemeEngine.colors.warning
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
