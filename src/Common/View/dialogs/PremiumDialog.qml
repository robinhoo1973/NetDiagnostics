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

    // 5WHY (复核 2026-08-19 壳单一来源): 根级 LayoutMirroring 曾是本弹窗
    // 的镜像承载（review 2026-08-17 补齐）——DialogCard 壳现已内置镜像并
    // 向下继承，根级声明无剩余职责，删除避免双规格互相覆盖。

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
        // 5WHY (2026-08-18): Android release 端 "Buy" 曾是无处理的静默死胡同——
        // PremiumStore.requestSubscription() 在发布构建 emit premiumRequired()
        // 但全仓无任何 onPremiumRequired 处理器，弹窗停留、无 toast、无文案。
        // 补上处理器：状态文案提示"Premium 订阅即将开放"，Buy 按钮同时被
        // supportsIap 门控隐藏，双保险杜绝死胡同。
        function onPremiumRequired() {
            root.statusText = T.tr("premiumRequiredMsg")
        }
    }

    // 背景点击关闭
    MouseArea {
        anchors.fill: parent
        onClicked: root.closeDialog()
    }

    // ── 卡片（5WHY 复核 2026-08-19 壳抽取）──
    // 卡片镀铬（双向钳制/radius/底色/边框/边距/RTL 镜像）与蜂窝弹窗同源
    // 收敛进 DialogCard；本处只余 Premium 专属内容。
    DialogCard {
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

        // 5WHY (2026-08-18): Buy/Restore 此前无条件显示——Android 发布端
        // supportsIap()=false 时 Buy 点击是死胡同（premiumRequired 无处理器），
        // Restore 直接 no-op。PremiumStore.h 契约声明"UI 按 supportsIap
        // 门控购买按钮"，实现却未遵守。现在补齐门控：不支持的平台隐藏购买
        // 按钮，仅显示功能列表——App Store 审核姿态也更正确。
        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            text: T.tr("subscribeBtn")
            visible: PremiumStore.supportsIap
            onClicked: PremiumStore.requestSubscription()
        }
        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            text: T.tr("restoreBtn")
            flat: true
            visible: PremiumStore.supportsIap
            onClicked: PremiumStore.restorePurchases()
        }
    }
}
