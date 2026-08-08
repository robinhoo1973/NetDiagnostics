// ShareSubscriptionDialog.qml — Shared share/subscription dialog (M3)
// 5WHY: The share/subscription dialog was duplicated identically in
// DiagnosticScreen, DashboardScreen, and ReportScreen (~38 lines each).
// Extract once so adding a new button, changing the layout, or updating
// the subscription flow affects all screens uniformly.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as Th
import "../widgets"

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(Th.ThemeEngine.colors.surface, 0.85)
    visible: shareStage !== 0; z: 1100

    // 5WHY: RTL (Arabic) — feature RowLayouts must mirror so icons hug the
    // start edge of the reading direction.
    LayoutMirroring.enabled: T.isRtl
    LayoutMirroring.childrenInherit: true

    required property int shareStage       // 0=none, 1=subscribe, 2=confirm
    required property bool isMobile
    property bool showProBadge: false      // ReportScreen shows a PRO badge
    property bool showIconBorder: false    // ReportScreen shows border on icon
    signal dismissed()

    MouseArea { anchors.fill: parent; onClicked: root.dismissed() }

    Rectangle {
        id: shareCard
        anchors.centerIn: parent
        width: Math.min(420, parent.width * 0.92)
        // 5WHY: The subscribe stage grew (badge + 3 feature rows + restore) so
        // the dialog can exceed small phone screens.  Cap the height and scroll
        // the body so the action buttons are never clipped off-screen.
        height: Math.min(contentCol.implicitHeight, Math.max(360, parent.height * 0.88))
        radius: 14
        color: Th.ThemeEngine.colors.card
        clip: true
        // 5WHY: Without this MouseArea, clicks on empty card space (between
        // title text and buttons, or near-misses) propagate to the backdrop
        // MouseArea and dismiss the dialog.  Absorb clicks so only explicit
        // button presses or backdrop taps trigger actions.
        // 5WHY: QML MouseArea only consumes mouse/touch events when it has
        // at least one signal handler connected.  Without a handler, it is
        // click-transparent — events propagate to the backdrop MouseArea
        // and dismiss the dialog on accidental card taps.
        MouseArea { anchors.fill: parent; onClicked: {} }
        ColumnLayout {
            id: contentCol
            anchors { fill: parent; margins: Th.ThemeEngine.spacing.xl }
            spacing: 14

            // Scrollable body (only scrolls when content exceeds the cap)
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                // 5WHY: without preferredHeight the Flickable reports an
                // implicitHeight of 0, so contentCol.implicitHeight collapsed
                // to ~button height and the whole dialog shrank to a thin bar
                // (icon/title/features invisible).  preferredHeight must equal
                // the content height so the cap formula sees the real height.
                Layout.preferredHeight: dlgCol.implicitHeight
                clip: true
                contentHeight: dlgCol.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                ColumnLayout {
                    id: dlgCol
                    width: parent.width
                    spacing: 14

            // Icon badge
            Rectangle {
                Layout.alignment: Qt.AlignHCenter; width: 60; height: 60; radius: 30
                color: Qt.alpha(root.shareStage === 1 ? Th.ThemeEngine.colors.warnYellow : Th.ThemeEngine.colors.cyan, 0.12)
                border {
                    // 5WHY: Qt Quick Rectangle borders expose only width/color —
                    // there is no visible flag on the border.  Setting an
                    // undeclared property causes "Cannot assign to non-existent
                    // property" fatal error at QML load time.  Use width: 0 to
                    // hide the border instead.
                    width: root.showIconBorder ? 1.5 : 0
                    color: Qt.alpha(root.shareStage === 1 ? Th.ThemeEngine.colors.warnYellow : Th.ThemeEngine.colors.cyan, 0.35)
                }
                AppIcon {
                    anchors.centerIn: parent
                    name: root.shareStage === 1 ? "zap" : "report"; size: 28
                    color: root.shareStage === 1 ? Th.ThemeEngine.colors.warnYellow : Th.ThemeEngine.colors.cyan
                }
            }

            // Title
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: root.shareStage === 1 ? T.tr("premiumHero") : T.tr("confirmShareTitle")
                font.family: Th.ThemeEngine.fontUi; font.pixelSize: 17
                font.weight: Font.Bold; color: Th.ThemeEngine.colors.textPrimary; wrapMode: Text.WordWrap
            }

            // Body
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                // 5WHY: On Android/desktop there is no store backend, so a
                // "Subscribe" CTA would dead-end.  Show an honest notice and
                // hide the Subscribe button (below) on those platforms.
                text: root.shareStage === 1
                      ? (appState.platformSupportsIap ? T.tr("subscribeBody") : T.tr("iapNotAvailable"))
                      : T.tr("confirmShareBody")
                font.family: Th.ThemeEngine.fontUi; font.pixelSize: 13
                color: Th.ThemeEngine.colors.textSecondary; wrapMode: Text.WordWrap
            }

            // PRO badge (subscribe stage) — one-time + lifetime story
            Rectangle {
                visible: root.shareStage === 1
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: proRow.implicitWidth + 20; implicitHeight: 26; radius: 13
                color: Qt.alpha(Th.ThemeEngine.colors.warnYellow, 0.15)
                RowLayout {
                    id: proRow
                    anchors.centerIn: parent; spacing: 5
                    AppIcon { name: "badge-check"; size: 12; color: Th.ThemeEngine.colors.warnYellow }
                    Label { text: T.tr("premiumOneTime"); color: Th.ThemeEngine.colors.warnYellow
                        font.family: Th.ThemeEngine.fontUi; font.pixelSize: 11; font.weight: Font.DemiBold }
                }
            }

            // Feature rows (subscribe stage) — PDF / HTML / lifetime
            Repeater {
                visible: root.shareStage === 1
                model: [
                    { icon: "layers",          key: "premiumFeaturePdf" },
                    { icon: "badge-check",     key: "premiumFeatureLifetime" }
                ]
                delegate: Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: featureRow.implicitHeight + 12; radius: 8
                    // 5WHY (dark-mode audit same as PremiumCard.qml): alpha=0.06
                    // warnYellow on card is invisible in dark (~1.13:1 contrast).
                    color: Qt.alpha(Th.ThemeEngine.colors.warnYellow, 0.12)
                    border { width: 1; color: Qt.alpha(Th.ThemeEngine.colors.warnYellow, 0.35) }
                    RowLayout {
                        id: featureRow
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 10 }
                        spacing: 8
                        AppIcon { name: modelData.icon; size: 15; color: Th.ThemeEngine.colors.warnYellow }
                        Label {
                            Layout.fillWidth: true
                            text: T.tr(modelData.key)
                            font.family: Th.ThemeEngine.fontUi; font.pixelSize: 12
                            color: Th.ThemeEngine.colors.textPrimary; wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            // Restore (subscribe stage only, not in-flight)
            // 5WHY (UX flow): the standalone "Restore or Purchase" button was
            // removed everywhere — PremiumDialog auto-probes for a previous
            // subscription on open (probeRestore) and restores it with a
            // "Purchase Restored" confirmation, else shows the buy flow.  A
            // separate restore action would only duplicate the automatic check.
            // Note: shareStage=1 (subscribe) is currently unreachable from the
            // pages (doShare() routes premium platforms through PremiumDialog),
            // so this block is defensive dead-code cleanup for consistency.
            Item { Layout.fillWidth: true; implicitHeight: 0 }
                }
            }

            // Fixed action row (never clipped)
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 4; spacing: 10
                DialogBtn {
                    visible: !appState.purchaseInProgress
                    label: root.shareStage === 1 ? T.tr("subscribeNotNow") : T.tr("dialogCancel")
                    accent: Th.ThemeEngine.colors.textSecondary; filled: false
                    onClicked: root.dismissed()
                }
                DialogBtn {
                    visible: root.shareStage !== 1 || appState.platformSupportsIap
                    label: root.shareStage === 1
                           ? (appState.purchaseInProgress ? T.tr("purchaseInProgress") : T.tr("subscribeBtn"))
                           : (root.isMobile ? T.tr("shareBtn") : T.tr("emailBtn"))
                    accent: root.shareStage === 1 ? Th.ThemeEngine.colors.warnYellow : Th.ThemeEngine.colors.cyan
                    filled: true
                    // 5WHY: Prevent duplicate StoreKit dialogs while a purchase
                    // is already in flight.
                    enabled: !appState.purchaseInProgress
                    onClicked: root.actionRequested()
                }
            }
        }
    }

    signal actionRequested()
}
