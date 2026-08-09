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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as Th
import "../widgets"

Rectangle {
    id: card
    Layout.fillWidth: true
    // 5WHY (iOS b21294): height must be driven by an internal ColumnLayout
    // whose implicitHeight propagates reliably on iOS static QML.  No
    // anchors-positioned RowLayout/hero chain — that pattern collapses on
    // static builds.  cardCol owns every child's vertical layout.
    implicitHeight: cardCol.implicitHeight
    radius: Th.ThemeEngine.radius.lg
    color: Th.ThemeEngine.colors.card
    border { width: 1; color: Th.ThemeEngine.colors.borderCard }
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
            color: Th.ThemeEngine.colors.textPrimary; wrapMode: Text.WordWrap
        }

        // ── Feature rows — flat icon+text, identical in both themes ────
        // Locked: info (i) icon in textSecondary.  Unlocked: check (✓) in
        // passGreen.  Text always textPrimary.
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Th.ThemeEngine.spacing.md
            Layout.leftMargin: Th.ThemeEngine.spacing.lg
            Layout.rightMargin: Th.ThemeEngine.spacing.lg
            spacing: 10
            AppIcon {
                name: appState.isPremium ? "check" : "info"
                size: 18
                color: appState.isPremium ? Th.ThemeEngine.colors.passGreen : Th.ThemeEngine.colors.textSecondary
            }
            Label {
                Layout.fillWidth: true
                text: T.tr("premiumFeaturePdf")
                font.family: Th.ThemeEngine.fontUi; font.pixelSize: 13
                color: Th.ThemeEngine.colors.textPrimary; wrapMode: Text.WordWrap
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Th.ThemeEngine.spacing.sm
            Layout.leftMargin: Th.ThemeEngine.spacing.lg
            Layout.rightMargin: Th.ThemeEngine.spacing.lg
            spacing: 10
            AppIcon {
                name: appState.isPremium ? "check" : "info"
                size: 18
                color: appState.isPremium ? Th.ThemeEngine.colors.passGreen : Th.ThemeEngine.colors.textSecondary
            }
            Label {
                Layout.fillWidth: true
                text: T.tr("premiumFeatureLifetime")
                font.family: Th.ThemeEngine.fontUi; font.pixelSize: 13
                color: Th.ThemeEngine.colors.textPrimary; wrapMode: Text.WordWrap
            }
        }

        // ── Divider ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Th.ThemeEngine.spacing.md
            Layout.bottomMargin: Th.ThemeEngine.spacing.md
            implicitHeight: 1
            // 5WHY (dark-mode audit): Qt.alpha(borderCard, 0.6) on dark card
            // (#334155·0.6≈#232A37 vs #1E293B → 1.07:1) was invisible.  Full
            // borderCard (#334155) yields 1.41:1 — low but acceptable for a
            // decorative divider (M3 outlineVariant range is 1.2–1.5:1).
            color: Th.ThemeEngine.colors.borderCard
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
            color: appState.isPremium ? Qt.alpha(Th.ThemeEngine.colors.passGreen, 0.16)
                                      : Th.ThemeEngine.colors.primary
            border { width: appState.isPremium ? 1 : 0
                     color: appState.isPremium ? Qt.alpha(Th.ThemeEngine.colors.passGreen, 0.5) : "transparent" }
            Rectangle { // hover overlay (locked only — no Qt.lighter, static-safe)
                anchors.fill: parent; radius: 12
                visible: !appState.isPremium && btnHover.containsMouse
                color: Qt.alpha("#FFFFFF", 0.12)
            }
            RowLayout {
                anchors.centerIn: parent; spacing: 8
                AppIcon {
                    name: appState.isPremium ? "check" : "zap"
                    size: 16
                    color: appState.isPremium ? Th.ThemeEngine.colors.passGreen : Th.ThemeEngine.colors.textOnAccent
                }
                Label {
                    text: appState.isPremium ? T.tr("premiumUnlockedBtn") : T.tr("subscribeBtn")
                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: 14; font.weight: Font.Bold
                    // 5WHY: primary fill + white failed WCAG (1.89:1 dark).
                    color: appState.isPremium ? Th.ThemeEngine.colors.passGreen : Th.ThemeEngine.colors.textOnAccent
                    elide: T.textElideStart
                }
            }
            MouseArea {
                id: btnHover
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                enabled: !appState.isPremium
                onClicked: card.purchaseRequested()
            }
            Accessible.name: appState.isPremium ? T.tr("premiumUnlockedBtn") : T.tr("subscribeBtn")
            Accessible.role: Accessible.Button
        }
    }
}
