// =============================================================================
// PremiumDialog.qml — Modern "one-time purchase" intro + purchase + restore
//
// Full-featured IAP portal.  Opened from SettingsScreen's PremiumCard entry
// (and reusable anywhere a "learn more / buy / restore" entry exists).
//
// Story told before any purchase: one-time payment → lifetime access →
// unlimited PDF/HTML report sharing.  Same copy keys (premiumHero /
// premiumOneTime / premiumFeaturePdf / premiumFeatureHtml /
// premiumFeatureLifetime) are used by ShareSubscriptionDialog so the
// product story is consistent at every purchase entry point.
//
// States: locked (intro + CTA + restore) | in-progress (spinner) |
//         deferred (Ask-to-Buy) | unlocked (owned, auto-close).
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as Th
import "../widgets"

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(Th.ThemeEngine.colors.surface, 0.85)
    visible: root.open
    z: 1200

    // 5WHY: RTL (Arabic) — the feature RowLayouts must mirror so icons hug
    // the start edge of the reading direction instead of the wrong side.
    LayoutMirroring.enabled: T.isRtl
    LayoutMirroring.childrenInherit: true

    property bool open: false
    property bool isMobile: false
    property string statusText: ""          // transient restore/deferred notices
    signal dismissed()

    function openDialog() {
        root.open = true
        root.statusText = ""
        // 5WHY (UX flow): probe the store for a previous purchase the moment
        // the IAP window opens — if the account already owns Premium, restore
        // it and confirm ("Purchases Restored"); otherwise the guided buy flow
        // stays visible.  Only on platforms with a real store backend.
        // 5WHY (iOS b21294): use probeRestore(), NOT restorePurchases() —
        // the latter sets purchaseInProgress=true, which makes the Buy and
        // Restore buttons below silently dead (`if (m_purchaseInProgress)
        // return;` in PremiumStore) while the probe is in flight.  probeRestore
        // runs the same StoreKit restore WITHOUT blocking the buttons.
        if (!appState.isPremium && appState.platformSupportsIap)
            Qt.callLater(function() { appState.probeRestore() })
    }
    function closeDialog() { root.open = false; root.dismissed() }

    // Backdrop tap dismisses
    MouseArea { anchors.fill: parent; onClicked: root.closeDialog() }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(400, parent.width * 0.92)
        // 5WHY (scripts/text_metrics.cpp vertical audit): on iPhone SE
        // (320x568) the dialog measured ~755px vs ~408px usable — it overflowed
        // the screen and clipped the action buttons.  Cap the height to the
        // viewport; the middle (feature/body) area scrolls while the hero and
        // the action buttons stay fixed.
        // 5WHY (iOS subscribe-success): after a successful purchase the dialog
        // switched to the unlocked state — green banner + a tertiary "Cancel"
        // text link — but bodyCol.implicitHeight shrank (actions column hides)
        // so the card compressed to ~250px and the bottom button could be
        // clipped by clip:true.  Unlocked state now gets an explicit height
        // floor (hero + banner + OK button + margins) so the confirmation
        // button is always fully visible, and the bottom control becomes a
        // clear primary "OK" (premiumOk) instead of a confusing "Cancel".
        height: appState.isPremium
            ? Math.min(hero.height + 44 + 48 + 24 + 24, Math.max(360, parent.height * 0.9))
            : Math.min(bodyCol.implicitHeight + hero.height,
                       Math.max(360, parent.height * 0.9))
        radius: Th.ThemeEngine.radius.xl
        color: Th.ThemeEngine.colors.card
        border { width: 1; color: Th.ThemeEngine.colors.borderCard }
        clip: true
        // 5WHY: Without this MouseArea, near-miss clicks on empty card space
        // propagate to the backdrop MouseArea and dismiss the dialog.
        // QML MouseArea only consumes events when it has a handler attached.
        MouseArea { anchors.fill: parent; onClicked: {} }

        // ── Hero band — solid brand color + subtle decoration ─────────
        Rectangle {
            id: hero
            anchors { left: parent.left; right: parent.right; top: parent.top }
            // 5WHY: a FIXED height clipped long translations (German/Arabic
            // one-time text) inside clip:true.  Height is now implicit so the
            // hero grows with its content in every language.
            // 5WHY (iOS b21294): same implicitHeight-collapse guard as
            // PremiumCard — on iOS static QML an anchors-positioned Layout's
            // implicitHeight does not propagate reliably.  Math.max floors the
            // hero height so the band (and its text) can never vanish or
            // overlap the body.
            implicitHeight: Math.max(150, heroCol.implicitHeight + (root.isMobile ? 30 : 34))
            // 5WHY (iOS screenshots 136/137): the Gradient hero band did NOT
            // render in Light theme (JS-object-bound GradientStop not re-evaluated
            // on iOS static QML) leaving white-on-white text invisible.  Solid
            // color + decoration replaces the gradient — same fix as the card.
            color: Th.ThemeEngine.colors.secondary
            // Decorative corner circle (modern depth, no gradient)
            Rectangle {
                anchors { right: parent.right; bottom: parent.bottom; rightMargin: -40; bottomMargin: -40 }
                width: 160; height: 160; radius: 80
                color: Qt.alpha("#FFFFFF", 0.07)
            }
            // Subtle primary accent line at the hero base
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 2
                color: Qt.alpha(Th.ThemeEngine.colors.primary, 0.45)
            }

            // Close (top-right, on the gradient)
            Rectangle {
                id: closeBtn
                anchors { top: parent.top; right: parent.right; topMargin: 8; rightMargin: 8 }
                // 5WHY: 44pt mobile touch target (Apple HIG / MD3), 34pt desktop.
                readonly property int btnSize: root.isMobile ? 44 : 34
                implicitWidth: btnSize; implicitHeight: btnSize; radius: btnSize / 2
                color: closeMouse.containsMouse ? Qt.alpha("#FFFFFF", 0.28) : Qt.alpha("#FFFFFF", 0.12)
                AppIcon { anchors.centerIn: parent; name: "close"; size: root.isMobile ? 20 : 15; color: "#FFFFFF" }
                MouseArea {
                    id: closeMouse
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                    onClicked: root.closeDialog()
                }
                Accessible.name: T.tr("dialogCancel")
                Accessible.role: Accessible.Button
            }

            ColumnLayout {
                id: heroCol
                anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: root.isMobile ? 16 : 20 }
                spacing: 8
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 56; implicitHeight: 56; radius: 16
                    color: Qt.alpha("#FFFFFF", 0.16)
                    border { width: 1; color: Qt.alpha("#FFFFFF", 0.35) }
                    AppIcon {
                        anchors.centerIn: parent
                        name: appState.isPremium ? "badge-check" : "zap"
                        size: 28; color: "#FFFFFF"
                    }
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    text: T.tr("premiumHero")
                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: 19; font.weight: Font.Bold
                    color: "#FFFFFF"; elide: T.textElideStart; maximumLineCount: 1
                    horizontalAlignment: Text.AlignHCenter
                }
                // One-time badge — width-capped so long translations wrap
                // instead of overflowing the card.
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: card.width * 0.8
                    implicitWidth: oneTimeRow.implicitWidth + 18; implicitHeight: oneTimeRow.implicitHeight + 10; radius: 13
                    color: Qt.alpha("#FFFFFF", 0.18)
                    RowLayout {
                        id: oneTimeRow
                        anchors.centerIn: parent; spacing: 5
                        AppIcon { name: "badge-check"; size: 12; color: "#FFFFFF" }
                        Label {
                            text: T.tr("premiumOneTime")
                            color: "#FFFFFF"
                            font.family: Th.ThemeEngine.fontUi; font.pixelSize: 11; font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                            Layout.maximumWidth: card.width * 0.68
                        }
                    }
                }
            }
        }

        // ── Body (scrollable content + fixed action row) ──────────────
        ColumnLayout {
            id: bodyCol
            anchors { left: parent.left; right: parent.right; top: hero.bottom; bottom: parent.bottom; margins: Th.ThemeEngine.spacing.xl }
            spacing: 12

            // Scrollable content — only scrolls when the card hits the height
            // cap (small phones).  preferredHeight must equal the content so
            // bodyCol.implicitHeight reflects the real unclipped height.
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: scrollCol.implicitHeight
                clip: true
                contentHeight: scrollCol.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                ColumnLayout {
                    id: scrollCol
                    width: parent.width
                    spacing: 12

                    // Unlocked state banner
                    Rectangle {
                        visible: appState.isPremium
                        Layout.fillWidth: true; implicitHeight: 44; radius: 10
                        // 5WHY (dark-mode audit): alpha=0.12 passGreen on dark card
                        // yields ~1.14:1 contrast — invisible.  Bumped to 0.20.
                        color: Qt.alpha(Th.ThemeEngine.colors.passGreen, 0.20)
                        border { width: 1; color: Qt.alpha(Th.ThemeEngine.colors.passGreen, 0.45) }
                        RowLayout {
                            anchors.centerIn: parent; spacing: 8
                            AppIcon { name: "badge-check"; size: 18; color: Th.ThemeEngine.colors.passGreen }
                            Label {
                                text: T.tr("premiumUnlocked")
                                font.family: Th.ThemeEngine.fontUi; font.pixelSize: 13; font.weight: Font.DemiBold
                                color: Th.ThemeEngine.colors.passGreen
                            }
                        }
                    }

                    // Feature rows (locked only)
                    Repeater {
                        visible: !appState.isPremium
                        model: [
                            { icon: "layers",          key: "premiumFeaturePdf" },
                            { icon: "badge-check",     key: "premiumFeatureLifetime" }
                        ]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: featureRow.implicitHeight + 14; radius: 10
                            // 5WHY (same dark-mode audit as PremiumCard.qml): alpha=0.06
                            // primary on card yields ~1.12:1 contrast — invisible in dark.
                            color: Qt.alpha(Th.ThemeEngine.colors.primary, 0.12)
                            border { width: 1; color: Qt.alpha(Th.ThemeEngine.colors.primary, 0.35) }
                            RowLayout {
                                id: featureRow
                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 12 }
                                spacing: 10
                                AppIcon { name: modelData.icon; size: 18; color: Th.ThemeEngine.colors.primary }
                                Label {
                                    Layout.fillWidth: true
                                    text: T.tr(modelData.key)
                                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: 13
                                    color: Th.ThemeEngine.colors.textPrimary; wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    // Body copy (locked)
                    Label {
                        visible: !appState.isPremium
                        Layout.fillWidth: true
                        text: appState.platformSupportsIap ? T.tr("subscribeBody") : T.tr("iapNotAvailable")
                        font.family: Th.ThemeEngine.fontUi; font.pixelSize: 12
                        color: Th.ThemeEngine.colors.textSecondary; wrapMode: Text.WordWrap; lineHeight: 1.5
                    }

                    // Transient status (deferred / restore result) — UI font, not mono
                    Label {
                        visible: root.statusText !== ""
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                        text: root.statusText
                        font.family: Th.ThemeEngine.fontUi; font.pixelSize: 12
                        color: Th.ThemeEngine.colors.warnYellow; wrapMode: Text.WordWrap
                    }
                }
            }

            // ── Actions (fixed bottom, never clipped) ─────────────────
            ColumnLayout {
                visible: !appState.isPremium
                Layout.fillWidth: true; spacing: 10

                // Primary CTA — solid primary + hover (the hero already carries
                // the gradient; a second gradient competes for focus).
                Rectangle {
                    visible: appState.platformSupportsIap
                    Layout.fillWidth: true; implicitHeight: 48; radius: 12
                    enabled: !appState.purchaseInProgress
                    opacity: enabled ? 1.0 : 0.5
                    color: Th.ThemeEngine.colors.primary
                    Rectangle { // hover overlay (no Qt.lighter — static-safe)
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
                        onClicked: appState.requestSubscription()
                    }
                    Accessible.name: T.tr("subscribeBtn")
                    Accessible.role: Accessible.Button
                }

                // Restore — secondary
                DialogBtn {
                    visible: appState.platformSupportsIap
                    label: T.tr("restoreBtn")
                    accent: Th.ThemeEngine.colors.textSecondary; filled: false
                    onClicked: appState.restorePurchases()
                }

                // Restore hint (locked, not in-flight)
                Label {
                    visible: appState.platformSupportsIap && !appState.purchaseInProgress
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: T.tr("premiumRestoreNote")
                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: 11
                    color: Th.ThemeEngine.colors.textMuted; wrapMode: Text.WordWrap
                }
            }

            // Not now / close — quiet text link (tertiary action, M3 style)
            // Locked: "Not Now".  Unlocked: primary "OK" confirmation — the
            // subscribe-success state must end with an unambiguous affirmative
            // button, not a "Cancel" (5WHY iOS subscribe-success).
            Item {
                visible: !appState.purchaseInProgress
                Layout.fillWidth: true
                implicitHeight: appState.isPremium ? 48 : 40
                // Unlocked → solid primary OK button; locked → text link.
                Rectangle {
                    anchors.fill: parent
                    visible: appState.isPremium
                    radius: 12
                    color: okHover.containsMouse ? Qt.alpha(Th.ThemeEngine.colors.primary, 0.9)
                                                 : Th.ThemeEngine.colors.primary
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                Label {
                    anchors.centerIn: parent
                    text: appState.isPremium ? T.tr("premiumOk") : T.tr("subscribeNotNow")
                    font.family: Th.ThemeEngine.fontUi; font.pixelSize: 13
                    font.weight: appState.isPremium ? Font.Bold : Font.Medium
                    color: appState.isPremium ? "#FFFFFF"
                           : (notNowHover.containsMouse ? Th.ThemeEngine.colors.textPrimary
                                                        : Th.ThemeEngine.colors.textSecondary)
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                MouseArea {
                    id: okHover
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                    visible: appState.isPremium
                    onClicked: root.closeDialog()
                }
                MouseArea {
                    id: notNowHover
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                    visible: !appState.isPremium
                    onClicked: root.closeDialog()
                }
                Accessible.name: appState.isPremium ? T.tr("premiumOk") : T.tr("subscribeNotNow")
                Accessible.role: Accessible.Button
            }
        }
    }

    // ── AppState wiring ────────────────────────────────────────────────
    Connections {
        target: appState
        function onPurchaseDeferred() {
            root.statusText = T.tr("purchaseDeferred")
            root.clearStatusTimer.restart()
        }
        function onPurchaseFailed() {
            // 5WHY: a cancelled/failed StoreKit purchase was silent.  Give
            // explicit feedback instead of the UI appearing to ignore the tap.
            root.statusText = T.tr("purchaseFailed")
            root.clearStatusTimer.restart()
        }
        function onRestoreCompleted(restoredAny, isError) {
            if (isError) root.statusText = T.tr("restoreError")
            else if (restoredAny) root.statusText = T.tr("restoreOk")
            else root.statusText = T.tr("restoreFail")
            root.clearStatusTimer.restart()
        }
        function onPremiumChanged() {
            // Purchase succeeded while the dialog was open → show the unlocked
            // banner briefly, then auto-close so the flow feels seamless.
            if (root.open && appState.isPremium) root.closeAfterUnlock.restart()
        }
    }
    Timer { id: closeAfterUnlock; interval: 1400; onTriggered: { if (root.open) root.closeDialog() } }
    Timer { id: clearStatusTimer; interval: Th.ThemeEngine.toastDurationMs; onTriggered: root.statusText = "" }
}
