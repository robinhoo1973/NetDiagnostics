// =============================================================================
// PremiumCard.qml — Premium (IAP) entry card for SettingsScreen
//
// Minimal layout per product spec (text mockup → real UI):
//
//   Premium Subscription                     ← title (premiumCardTitle)
//   (i) Share Unlimited PDF / HTML Reports    ← feature row (info/check icon)
//   (i) One-Time Purchase, Valid Forever      ← feature row (info/check icon)
//   ─────────────────────────────────────     ← divider
//   [ Unlock Premium ]                        ← locked: primary CTA
//   [ Premium Pro Unlocked ]                  ← unlocked: green status
//
// Locked state shows ONE action button "Unlock Premium" that opens the full
// PremiumDialog (which auto-probes for a previous subscription and restores
// it with a confirmation, else presents the buy flow).  Unlocked state shows
// a green "Premium Pro Unlocked" status button (non-interactive) — so a tap
// can never silently no-op on an already-unlocked device.
//
// Same layout in dark & light — only the palette colors (ThemeEngine.colors)
// differ, per spec.
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as Th
// 5WHY (review 2026-08-17): 文件裸用 T.* 但此前只有限定导入（as Th）——
// ReferenceError 使卡片全部文本为空（仅 premium 平台可见；RTL 镜像与
// Accessible.name 同样静默失效）。非限定导入让 qmldir 单例 T 进入作用域。
import "../theme"
import "../widgets"

Rectangle {
    id: card
    Layout.fillWidth: true
    // Premium 状态经 PremiumStore（恢复的后端）；页级可见性仍由 AppState 门控
    readonly property bool _prem: PremiumStore.isPremium
    // 5WHY (iOS b21294): height must be driven by an internal ColumnLayout
    // whose implicitHeight propagates reliably on iOS static QML.  No
    // anchors-positioned RowLayout/hero chain — that pattern collapses on
    // static builds.  cardCol owns every child's vertical layout.
    implicitHeight: cardCol.implicitHeight
    radius: Th.ThemeEngine.radius.lg
    color: Th.ThemeEngine.colors.surfaceContainerLow
    border { width: 1; color: Th.ThemeEngine.colors.outlineVariant }
    clip: true

    // Emitted by the locked "Unlock Premium" button; the page opens the full
    // PremiumDialog (which auto-probes restore + offers the buy flow).
    signal purchaseRequested()

    // 5WHY: RTL (Arabic) — RowLayouts must mirror so icons hug the start edge
    // of the reading direction.
    LayoutMirroring.enabled: T.isRtl
    LayoutMirroring.childrenInherit: true

    ColumnLayout {
        id: cardCol
        anchors.fill: parent
        spacing: 0

        // ── Title ────────────────────────────────────────────────────
        Label {
            Layout.fillWidth: true
            Layout.topMargin: Th.ThemeEngine.spacing.lg
            Layout.leftMargin: Th.ThemeEngine.spacing.lg
            Layout.rightMargin: Th.ThemeEngine.spacing.lg
            text: T.tr("premiumCardTitle")
            font.family: Th.ThemeEngine.fontUi; font.pixelSize: 16; font.weight: Font.Bold
            color: Th.ThemeEngine.colors.onSurface; wrapMode: Text.WordWrap
        }

        // ── Feature rows — flat icon+text, identical in both themes ────
        // Locked: info (i) icon in onSurfaceVariant.  Unlocked: check (✓) in
        // success.  Text always onSurface.
        // 5WHY (simplify 2026-08-17): 两条特性行仅 tr key 与间距不同——
        // Repeater 由两行模型驱动，新增行只加模型条目。
        Repeater {
            model: [
                { key: "premiumFeaturePdf",      first: true },
                { key: "premiumFeatureLifetime", first: false }
            ]
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: modelData.first ? Th.ThemeEngine.spacing.md : Th.ThemeEngine.spacing.sm
                Layout.leftMargin: Th.ThemeEngine.spacing.lg
                Layout.rightMargin: Th.ThemeEngine.spacing.lg
                spacing: 10
                AppIcon {
                    name: card._prem ? "check" : "info"
                    size: 18
                    color: card._prem ? Th.ThemeEngine.colors.success : Th.ThemeEngine.colors.onSurfaceVariant
                }
                Label {
                    Layout.fillWidth: true
                    text: T.tr(modelData.key)
                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: Th.ThemeEngine.fontSize.body
                    color: Th.ThemeEngine.colors.onSurface; wrapMode: Text.WordWrap
                }
            }
        }

        // ── Divider ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Th.ThemeEngine.spacing.md
            Layout.bottomMargin: Th.ThemeEngine.spacing.md
            implicitHeight: 1
            // 5WHY (dark-mode audit): Qt.alpha(outlineVariant, 0.6) on dark card
            // (#334155·0.6≈#232A37 vs #1E293B → 1.07:1) was invisible.  Full
            // outlineVariant (#334155) yields 1.41:1 — low but acceptable for a
            // decorative divider (M3 outlineVariant range is 1.2–1.5:1).
            color: Th.ThemeEngine.colors.outlineVariant
        }

        // ── Single action button ─────────────────────────────────────
        // Locked: solid primary "Unlock Premium" → opens PremiumDialog
        // (auto probe-restore + buy flow).  Unlocked: green "Premium Pro
        // Unlocked" status — non-interactive so a tap can never silently
        // no-op (PremiumStore::restorePurchases() returns early when premium
        // is already owned).
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Th.ThemeEngine.spacing.lg
            Layout.rightMargin: Th.ThemeEngine.spacing.lg
            Layout.bottomMargin: Th.ThemeEngine.spacing.lg
            implicitHeight: 46; radius: 12
            color: card._prem ? Qt.alpha(Th.ThemeEngine.colors.success, 0.16)
                                      : Th.ThemeEngine.colors.primary
            border { width: card._prem ? 1 : 0
                     color: card._prem ? Qt.alpha(Th.ThemeEngine.colors.success, 0.5) : "transparent" }
            Rectangle { // hover overlay (locked only — no Qt.lighter, static-safe)
                anchors.fill: parent; radius: 12
                visible: !card._prem && btnHover.containsMouse
                // 5WHY (review round 3): 裸 #FFFFFF 违反 M3 单一事实源——onSurface
                // 状态层惯例（暗=白纱保持现状；亮=深色纱，此前白纱在亮主色上不可见）
                color: Qt.alpha(Th.ThemeEngine.colors.onSurface, 0.12)
            }
            RowLayout {
                anchors.centerIn: parent; spacing: 8
                AppIcon {
                    name: card._prem ? "check" : "zap"
                    size: 16
                    // 5WHY (review 2026-08-17): 解锁态 success 绿在 success@0.16
                    // 浅底上仅 ~2.2:1（AA 失败）——onSuccessContainer 是 success 系绿
                    // 在 success 淡底上的专用 AA 角色（暗=#4ADE80；亮=#047857 ≈4.7:1；
                    // 5WHY review round 3: 原借用 terminalText 语义错位，终端样式
                    // 重调会连带改动购买按钮）。
                    color: card._prem ? Th.ThemeEngine.colors.onSuccessContainer : Th.ThemeEngine.colors.onPrimary
                }
                Label {
                    text: card._prem ? T.tr("premiumUnlockedBtn") : T.tr("subscribeBtn")
                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: 14; font.weight: Font.Bold
                    // 5WHY: primary fill + white failed WCAG (1.89:1 dark).
                    color: card._prem ? Th.ThemeEngine.colors.onSuccessContainer : Th.ThemeEngine.colors.onPrimary
                    elide: T.textElideStart
                }
            }
            MouseArea {
                id: btnHover
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                enabled: !card._prem
                onClicked: card.purchaseRequested()
            }
            Accessible.name: card._prem ? T.tr("premiumUnlockedBtn") : T.tr("subscribeBtn")
            Accessible.role: Accessible.Button
        }
    }
}
