// =============================================================================
// PremiumCard.qml — Modern Premium (IAP) entry card for SettingsScreen
//
// Modern card design: brand gradient hero band + one-time/lifetime messaging +
// feature chips (PDF / HTML / lifetime) + primary purchase CTA + restore.
//
// Emits purchaseRequested() so the page can open the full PremiumDialog, and
// restoreRequested() for a direct restore tap.  When premium is already
// unlocked the card switches to an "owned" state (green banner + restore).
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
    implicitHeight: cardBody.implicitHeight + hero.height
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

    // ── Hero band — brand gradient (indigo → sky) ────────────────────
    Rectangle {
        id: hero
        anchors { left: parent.left; right: parent.right; top: parent.top }
        // 5WHY: card.isMobile was referenced but never declared — qmllint
        // flagged it (missing-property).  Use the ThemeEngine singleton, the
        // canonical platform layout flag, instead of an undeclared property.
        implicitHeight: heroCol.implicitHeight + (ThemeEngine.isMobile ? 22 : 26)
        gradient: Gradient {
            GradientStop { position: 0.0; color: Th.ThemeEngine.colors.secondary }
            GradientStop { position: 1.0; color: Th.ThemeEngine.colors.primary }
        }
        ColumnLayout {
            id: heroCol
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: Th.ThemeEngine.spacing.lg }
            spacing: 6
            RowLayout {
                Layout.fillWidth: true; spacing: 12
                // Icon tile — rounded square (44pt, modern), not a circle
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
                        // 5WHY (font-metric audit, scripts/text_metrics.cpp):
                        // "NetDiagnostics PRO" measures 160px @17px but the hero
                        // text column is only 146px on iPhone SE (320pt) — it
                        // elided there.  Shrink responsively to 15px (≈141px,
                        // +5px slack) when the column is narrow; elide remains
                        // as a belt-and-braces fallback.
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
                // PRO pill (top-aligned, never stretches)
                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: proPill.implicitWidth + 16; implicitHeight: 24; radius: 12
                    color: appState.isPremium ? Qt.alpha("#FFFFFF", 0.24) : Qt.alpha("#FFFFFF", 0.14)
                    RowLayout {
                        id: proPill
                        anchors.centerIn: parent; spacing: 4
                        AppIcon { name: "badge-check"; size: 11; color: "#FFFFFF" }
                        Label {
                            text: T.tr("premiumBadge")
                            font.family: Th.ThemeEngine.fontUi; font.pixelSize: 10; font.weight: Font.DemiBold; color: "#FFFFFF"
                        }
                    }
                }
            }
        }
    }

    // ── Body ─────────────────────────────────────────────────────────
    ColumnLayout {
        id: cardBody
        anchors { left: parent.left; right: parent.right; top: hero.bottom; margins: Th.ThemeEngine.spacing.lg }
        spacing: Th.ThemeEngine.spacing.md

        // Feature list — VERTICAL rows shown in BOTH states: locked (blue
        // icons = what you get) and unlocked (green checks = owned benefits).
        // 5WHY (iOS screenshot): the old unlocked state showed a separate
        // "Premium Unlocked · One-time…" banner that duplicated the hero
        // subtitle and clipped on narrow screens.  Removed — the hero
        // subtitle already states the state; the green rows confirm the
        // owned benefits (App Store subscription-management pattern).
        Repeater {
            model: [
                { icon: "file-pdf",    key: "premiumFeaturePdf" },
                { icon: "file-html",   key: "premiumFeatureHtml" },
                { icon: "badge-check", key: "premiumFeatureLifetime" }
            ]
            delegate: Rectangle {
                Layout.fillWidth: true
                implicitHeight: featRow.implicitHeight + 16; radius: 10
                color: appState.isPremium ? Qt.alpha(Th.ThemeEngine.colors.passGreen, 0.08)
                                          : Qt.alpha(Th.ThemeEngine.colors.primary, 0.06)
                border { width: 1; color: appState.isPremium ? Qt.alpha(Th.ThemeEngine.colors.passGreen, 0.3)
                                                             : Qt.alpha(Th.ThemeEngine.colors.primary, 0.16) }
                RowLayout {
                    id: featRow
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 12 }
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
                        color: appState.isPremium ? Th.ThemeEngine.colors.textSecondary : Th.ThemeEngine.colors.textPrimary
                        wrapMode: Text.WordWrap; lineHeight: 1.35
                    }
                }
            }
        }

        // Unavailable notice — premium platform without a store backend
        // (Android/macOS today): honest copy instead of a dead-end CTA.
        // 5WHY: supportsIap() is iOS-only; Android/macOS are premium platforms
        // but have no purchase backend yet, so a Buy button would dead-end
        // (Apple/Google review policy forbids that).
        Rectangle {
            visible: !appState.isPremium && !appState.platformSupportsIap
            Layout.fillWidth: true; implicitHeight: 44; radius: 10
            color: Qt.alpha(Th.ThemeEngine.colors.warnYellow, 0.08)
            border { width: 1; color: Qt.alpha(Th.ThemeEngine.colors.warnYellow, 0.3) }
            Label {
                anchors { fill: parent; margins: 8 }
                text: T.tr("iapNotAvailable")
                font.family: Th.ThemeEngine.fontUi; font.pixelSize: 12
                color: Th.ThemeEngine.colors.warnYellow; wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
        }

        // ── Actions ──────────────────────────────────────────────────
        // Primary CTA — solid primary + hover (the hero carries the gradient;
        // a second gradient would compete for visual focus).
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

        // Restore — secondary outline
        Rectangle {
            visible: !appState.purchaseInProgress && appState.platformSupportsIap
            Layout.fillWidth: true; implicitHeight: 42; radius: 10
            color: restoreArea.containsMouse ? Qt.alpha(Th.ThemeEngine.colors.textSecondary, 0.08) : "transparent"
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
