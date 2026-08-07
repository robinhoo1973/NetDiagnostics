// =============================================================================
// PremiumCard.qml — Premium (IAP) entry card for SettingsScreen
//
// Compact card design: indigo hero band + icon + title/subtitle +
// two flat feature rows (no background chips) + divider + action buttons.
//
// Emits purchaseRequested() so the page can open the full PremiumDialog, and
// restoreRequested() for a direct restore tap.  When premium is already
// unlocked the card switches to an "owned" state (green checkmarks, no CTA).
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as Th
import "../widgets"

Rectangle {
    id: card
    Layout.fillWidth: true
    // 5WHY: implicitHeight must be explicit — the body uses anchors (not a
    // Layout), so its content does NOT contribute to the root implicit height.
    // Without this the card collapses to height 0 inside SettingsScreen's
    // ColumnLayout and never renders.
    implicitHeight: hero.height + cardBody.implicitHeight
    radius: Th.ThemeEngine.radius.lg
    color: Th.ThemeEngine.colors.card
    border { width: 1; color: Th.ThemeEngine.colors.borderCard }
    clip: true

    signal purchaseRequested()
    signal restoreRequested()

    // 5WHY: RTL (Arabic) — RowLayouts must mirror so icons hug the start edge
    // of the reading direction.
    LayoutMirroring.enabled: T.isRtl
    LayoutMirroring.childrenInherit: true

    // ── Hero band — solid indigo + icon + title + subtitle ─────────────
    Rectangle {
        id: hero
        anchors { left: parent.left; right: parent.right; top: parent.top }
        implicitHeight: heroCol.implicitHeight + (ThemeEngine.isMobile ? 22 : 26)
        // 5WHY (iOS screenshots 136/137): the Gradient hero band did NOT render
        // in Light theme on iOS static QML.  Solid secondary (indigo) replaces
        // the gradient; decoration adds depth without gradient dependency.
        color: Th.ThemeEngine.colors.secondary
        // Decorative corner circle (modern depth)
        Rectangle {
            anchors { right: parent.right; bottom: parent.bottom; rightMargin: -34; bottomMargin: -34 }
            width: 132; height: 132; radius: 66
            color: Qt.alpha("#FFFFFF", 0.07)
        }
        // Subtle primary accent line at the hero base
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 2
            color: Qt.alpha(Th.ThemeEngine.colors.primary, 0.45)
        }

        RowLayout {
            id: heroCol
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: Th.ThemeEngine.spacing.lg }
            spacing: 12
            // Icon tile — rounded square
            Rectangle {
                implicitWidth: 44; implicitHeight: 44; radius: 14
                color: Qt.alpha("#FFFFFF", 0.16)
                border { width: 1; color: Qt.alpha("#FFFFFF", 0.35) }
                AppIcon {
                    anchors.centerIn: parent
                    name: appState.isPremium ? "badge-check" : "zap"
                    size: 22; color: "#FFFFFF"
                }
            }
            // Title + subtitle — subtitle WRAPS (never elides) so long
            // translations (German/Russian/Arabic) never lose the story.
            ColumnLayout {
                Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter; spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: T.tr("premiumHero")
                    font.family: Th.ThemeEngine.fontUi
                    font.pixelSize: (card.width - 148) < 162 ? 15 : 17
                    font.weight: Font.Bold
                    color: "#FFFFFF"; elide: T.textElideStart; maximumLineCount: 1
                }
                Label {
                    Layout.fillWidth: true
                    text: appState.isPremium ? T.tr("premiumUnlocked") : T.tr("premiumOneTime")
                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: 12
                    color: Qt.alpha("#FFFFFF", 0.92); wrapMode: Text.WordWrap; lineHeight: 1.3
                }
            }
        }
    }

    // ── Body ─────────────────────────────────────────────────────────
    ColumnLayout {
        id: cardBody
        anchors { left: parent.left; right: parent.right; top: hero.bottom; margins: Th.ThemeEngine.spacing.lg }
        spacing: Th.ThemeEngine.spacing.sm

        // Feature rows — flat icon+text rows (no background chips).
        // Locked: layers + badge-check with primary icons.
        // Unlocked: both become green checkmarks.
        Repeater {
            model: [
                { icon: "layers",          key: "premiumFeaturePdf" },
                { icon: "badge-check",     key: "premiumFeatureLifetime" }
            ]
            delegate: RowLayout {
                Layout.fillWidth: true
                spacing: 10
                AppIcon {
                    name: appState.isPremium ? "badge-check" : modelData.icon
                    size: 18
                    color: appState.isPremium ? Th.ThemeEngine.colors.passGreen : Th.ThemeEngine.colors.primary
                }
                Label {
                    Layout.fillWidth: true
                    text: T.tr(modelData.key)
                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: 13
                    color: Th.ThemeEngine.colors.textPrimary; wrapMode: Text.WordWrap
                }
            }
        }

        // ── Divider ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            // 5WHY (dark-mode audit): Qt.alpha(borderCard, 0.6) on dark card
            // (#334155·0.6≈#232A37 vs #1E293B → 1.07:1) was invisible.  Full
            // borderCard (#334155) yields 1.41:1 — low but acceptable for a
            // decorative divider (M3 outlineVariant range is 1.2–1.5:1).
            color: Th.ThemeEngine.colors.borderCard
        }

        // ── Actions ──────────────────────────────────────────────────
        // Locked: primary CTA + restore outline
        // Unlocked: restore outline only
        // 5WHY (iOS screenshots 136/137 dark-mode audit): feature rows
        // were previously wrapped in background chips with alpha=0.06
        // primary on card — invisible in dark mode (~1.12:1 contrast).
        // Removed the chip backgrounds entirely; flat rows with visible
        // icons + text are cleaner and never disappear in either theme
        // (Apple App Store / Spotify subscription-card pattern).

        // Primary CTA — solid primary + hover
        Rectangle {
            visible: !appState.isPremium && appState.platformSupportsIap
            Layout.fillWidth: true; implicitHeight: 48; radius: 12
            enabled: !appState.purchaseInProgress
            opacity: enabled ? 1.0 : 0.5
            color: Th.ThemeEngine.colors.primary
            Rectangle { // hover overlay (no Qt.lighter — static-build-safe)
                anchors.fill: parent; radius: 12
                visible: ctaHover.containsMouse
                color: Qt.alpha("#FFFFFF", 0.12)
            }
            RowLayout {
                anchors.centerIn: parent; spacing: 8
                AppIcon {
                    name: appState.purchaseInProgress ? "spinner" : "zap"
                    size: 16; color: "#FFFFFF"
                }
                Label {
                    text: appState.purchaseInProgress ? T.tr("purchaseInProgress") : T.tr("subscribeBtn")
                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: 14; font.weight: Font.Bold
                    color: "#FFFFFF"; elide: T.textElideStart
                }
            }
            MouseArea {
                id: ctaHover
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                enabled: !appState.purchaseInProgress
                onClicked: card.purchaseRequested()
            }
            Accessible.name: T.tr("subscribeBtn")
            Accessible.role: Accessible.Button
        }

        // Restore — secondary outline (always visible, both states)
        Rectangle {
            visible: !appState.purchaseInProgress && appState.platformSupportsIap
            Layout.fillWidth: true; implicitHeight: 42; radius: 10
            // 5WHY: hover feedback alpha=0.08 was invisible in dark (~1.06:1).
            color: restoreArea.containsMouse ? Qt.alpha(Th.ThemeEngine.colors.textSecondary, 0.12) : "transparent"
            border { width: 1; color: Qt.alpha(Th.ThemeEngine.colors.textSecondary, 0.4) }
            Behavior on color { ColorAnimation { duration: 150 } }
            RowLayout {
                anchors.centerIn: parent; spacing: 8
                AppIcon { name: "badge-circle"; size: 15; color: Th.ThemeEngine.colors.textSecondary }
                Label {
                    text: T.tr("restoreBtn")
                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: 13; font.weight: Font.DemiBold
                    color: Th.ThemeEngine.colors.textSecondary; elide: T.textElideStart
                }
            }
            MouseArea {
                id: restoreArea
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                onClicked: card.restoreRequested()
            }
            Accessible.name: T.tr("restoreBtn")
            Accessible.role: Accessible.Button
        }
    }
}
