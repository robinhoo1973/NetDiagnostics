import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "screens"
import "widgets"
import "theme"

// ── Shared production GUI: nav bar + screen stack ─────────────────────
// Used by main.qml (production)

Item {
    id: content
    objectName: "appContent"
    readonly property alias stackView: stackView
    property bool compact: false // mobile: icons only, right-aligned, no close
    // 5WHY (nav animation direction): StackView's default push/pop transitions
    // slide in from fixed sides (pushEnter: right, popEnter: left), so a tab
    // reached via pop() (already in the stack, e.g. dashboard) slid in from the
    // LEFT even when the user swiped LEFT to reach it — the dashboard↔diag pair
    // appeared to animate backwards vs every other pair.  Direction is now
    // driven by _navDir (+1 = next tab → enter from right, -1 = prev tab →
    // enter from left) set from the target index vs current index, so push and
    // pop both slide the same way regardless of stack state.
    property int _navDir: 1
    // 5WHY: navBlocked only checked item-local overlayVisible (detailOverlay /
    // previewOverlay), not the cross-cutting cellular-warning dialog.  If the
    // cellular warning was showing without a detail overlay, navBlocked was
    // false, closeCurrentOverlay() was never called, and the nav tap bypassed
    // the dismiss+cancel logic — leaving the run paused at G2->G3 forever.
    // Include cellularWarnVisible so ANY navigation tap while the warning is
    // showing triggers the dismiss+cancel path in closeCurrentOverlay().
    property bool navBlocked: (stackView.currentItem && stackView.currentItem.overlayVisible === true)
                               || appState.cellularWarnVisible
    signal closeRequested()

    // 5WHY: Nav buttons were disabled when overlays were open, preventing
    // navigation.  Users expect nav taps to dismiss overlays like tapping
    // the backdrop does.  Close any open overlay on the active screen.
    function closeCurrentOverlay() {
        var item = stackView.currentItem
        if (!item) return
        // Close detail overlay (DiagnosticScreen)
        if (item.detailOverlay && item.detailOverlay.visible) item.detailOverlay.visible = false
        // Close preview overlay (ReportScreen / DashboardScreen)
        if (typeof item.previewVisible !== 'undefined' && item.previewVisible) item.previewVisible = false
        // 5WHY: Dismissing the cellular warning via nav tap left the run
        // paused at the G2->G3 boundary with no way to resume.  The dialog's
        // own Cancel button calls appState.cancel(); match that behaviour so
        // the run doesn't hang in Running state after navigation.
        if (appState.cellularWarnVisible) { appState.cellularWarnVisible = false; appState.cancel() }
    }

    // ── Single source of truth for tab definitions ───────────────────
    readonly property var tabScreens: ["dashboard","diagnostic","config","settings"]
    readonly property var tabComponents: [dashboardComp, diagnosticComp, configComp, settingsComp]
    readonly property var tabLabels: [T.tr("dashboard"), T.tr("diagnostics"), T.tr("config"), T.tr("settings")]

    function switchToTab(idx) {
        if (idx < 0 || idx >= tabScreens.length) return
        // Set slide direction from target vs current index so BOTH push and pop
        // enter from the same side (ViewPager convention: next→right, prev→left).
        var curIdx = currentTabIndex()
        if (idx > curIdx) _navDir = 1
        else if (idx < curIdx) _navDir = -1
        for (var i = 0; i < stackView.depth; i++) {
            var item = stackView.get(i)
            if (item && item.objectName === tabScreens[idx]) {
                stackView.pop(item)
                return
            }
        }
        stackView.push(tabComponents[idx].createObject(stackView))
    }

    // Index of the currently active tab (0..tabScreens.length-1), or -1 when
    // the top screen is not a tab (e.g. the ReportScreen overlay).  Used by
    // the touch swipe handler to resolve "left/right adjacent" pages.
    function currentTabIndex() {
        var cur = stackView.currentItem
        if (!cur) return -1
        for (var i = 0; i < tabScreens.length; i++) {
            if (tabScreens[i] === cur.objectName) return i
        }
        return -1
    }

    Component { id: diagnosticComp; DiagnosticScreen { objectName: "diagnostic" } }
    Component { id: dashboardComp;  DashboardScreen  { objectName: "dashboard"  } }
    Component { id: configComp;     ConfigScreen     { objectName: "config"     } }
    Component { id: settingsComp;   SettingsScreen   { objectName: "settings"   } }

    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // ── Screen stack (fills remaining space above the dock) ──────
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true

            StackView {
                id: stackView
                anchors.fill: parent
                clip: true
                initialItem: diagnosticComp
                // 5WHY (nav animation direction): explicit transitions so the
                // slide side follows _navDir — the ENTERING page always comes
                // from the swipe/tap direction (+1 → from right, -1 → from
                // left) and the LEAVING page exits the opposite way, for BOTH
                // push and pop.  Without these, push() always entered from the
                // right and pop() always from the left, breaking dashboard↔diag.
                pushEnter: Transition {
                    XAnimator { from: stackView.width * content._navDir; to: 0; duration: 220; easing.type: Easing.OutCubic }
                }
                pushExit: Transition {
                    XAnimator { from: 0; to: -stackView.width * content._navDir; duration: 220; easing.type: Easing.InCubic }
                }
                popEnter: Transition {
                    XAnimator { from: stackView.width * content._navDir; to: 0; duration: 220; easing.type: Easing.OutCubic }
                }
                popExit: Transition {
                    XAnimator { from: 0; to: -stackView.width * content._navDir; duration: 220; easing.type: Easing.InCubic }
                }
            }

            // ── Touch-swipe page navigation (adjacent tabs) ──────────
            // 5WHY (Android touch UX): pages were only reachable through
            // bottom-dock taps.  On a touch screen the natural gesture for
            // "next/previous tab" is a horizontal swipe (ViewPager pattern).
            //
            // Design decisions:
            // - Touch only (PointerDevice.TouchScreen): a desktop mouse drag
            //   must keep selecting text / dragging scrollbars, so it never
            //   triggers page switches.
            // - Horizontal only (yAxis.enabled: false): every screen's root
            //   is a vertical Flickable/ListView; disabling the Y axis means
            //   this handler never activates on vertical drags, so scrolling
            //   is untouched.
            // - Transparent overlay Item: the Item itself accepts no input,
            //   so taps/clicks pass through to the page below; the DragHandler
            //   only engages once a real horizontal drag exceeds the threshold.
            // - Direction = ViewPager convention: swipe left → next tab
            //   (right neighbour), swipe right → previous tab (left neighbour).
            // - The dock tab order is intentionally NOT mirrored in RTL (see
            //   dock 5WHY), so swipe direction maps to the fixed tab order
            //   regardless of language.
            // - currentTabIndex() returns -1 on the ReportScreen overlay →
            //   swipe is a no-op there (it is not a tab).
            // - Mirrors dock tap behaviour: when an overlay / cellular-warning
            //   blocks navigation, a swipe dismisses it instead of switching.
            Item {
                anchors.fill: parent
                DragHandler {
                    id: pageSwipe
                    acceptedDevices: PointerDevice.TouchScreen
                    target: null
                    dragThreshold: 24
                    // Horizontal-only: never steal vertical Flickable scrolls.
                    xAxis.enabled: true
                    yAxis.enabled: false
                    // May take over from plain items and other handlers
                    // (Flickable's internal one) once a horizontal drag is
                    // unambiguous; vertical drags never activate this handler.
                    grabPermissions: PointerHandler.CanTakeOverFromItems
                                   | PointerHandler.CanTakeOverFromHandlersOfDifferentType
                    property int swipeStartIndex: -1
                    property real swipeStartX: 0
                    property real swipeStartY: 0
                    property real swipeLastX: 0
                    property real swipeLastY: 0
                    onActiveChanged: {
                        if (active) {
                            swipeStartIndex = content.currentTabIndex()
                            swipeStartX = centroid.position.x
                            swipeStartY = centroid.position.y
                            swipeLastX = swipeStartX
                            swipeLastY = swipeStartY
                        } else if (swipeStartIndex >= 0) {
                            var dx = swipeLastX - swipeStartX
                            var dy = swipeLastY - swipeStartY
                            // Require a decisive horizontal swipe (min travel
                            // + clearly horizontal) to avoid accidental ones.
                            if (Math.abs(dx) >= 60 && Math.abs(dx) >= 1.5 * Math.abs(dy)) {
                                if (content.navBlocked) {
                                    content.closeCurrentOverlay()
                                } else if (dx < 0) {
                                    content.switchToTab(swipeStartIndex + 1) // left → next
                                } else {
                                    content.switchToTab(swipeStartIndex - 1) // right → prev
                                }
                            }
                            swipeStartIndex = -1
                        }
                    }
                    onCentroidChanged: {
                        if (active) {
                            swipeLastX = centroid.position.x
                            swipeLastY = centroid.position.y
                        }
                    }
                }
            }
        }

        // ── Bottom dock navigation bar (Material Design 3 compliant) ──
        Rectangle {
            Layout.fillWidth: true
            // M3: 80dp full, 56dp compact desktop.  Apple HIG: 44-48pt mobile.
            implicitHeight: compact ? 48 : 56
            color: ThemeEngine.colors.navBar
            // 5WHY: LayoutMirroring from main.qml mirrored the whole dock in RTL
            // (Arabic), reversing nav-button order to Settings…Dashboard.  Bottom
            // navigation is a fixed-position chrome element: Apple HIG keeps tab
            // order constant across localizations, and reversing it breaks the
            // user's spatial muscle memory on every language switch.  Keep
            // Dashboard→Diagnostics→Config→Settings left-to-right in ALL
            // languages; only the dock's content (labels) stays LTR-styled.
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: false
            // Drag handle for frameless window (Qt.FramelessWindowHint)
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPositionChanged: function(mouse) {
                    if (mouse.buttons & Qt.LeftButton) {
                        var win = content.Window.window
                        if (win && typeof win.startSystemMove === "function")
                            win.startSystemMove()
                    }
                }
            }
            RowLayout {
                anchors { fill: parent; leftMargin: compact ? 0 : 16; rightMargin: compact ? 4 : 16 }
                // Nav items centered via balanced left+right Layout.fillWidth spacers
                Item { Layout.fillWidth: true }
                // M3 spec: 8dp minimum gap between touch targets. 4dp for same-group icons.
                // 5WHY: compact spacing was 0 — adjacent 48dp touch targets with zero
                // separation cause mis-taps on narrow mobile screens.
                Row { spacing: 4
                    Repeater {
                        model: [
                            { screen: "dashboard",  icon: "dashboard" },
                            { screen: "diagnostic", icon: "diagnostics" },
                            { screen: "config",     icon: "config" },
                            { screen: "settings",   icon: "gear" }
                        ]
                        delegate: ItemDelegate {
                            id: navBtn
                            property bool active: stackView.currentItem && stackView.currentItem.objectName === modelData.screen
                            property string labelText: {
                                T.lang // force re-evaluation on language change
                                return content.tabLabels[index] || modelData.screen
                            }
                            // M3: icon 24dp + gap 8dp + text + padding 12dp each side
                            implicitWidth: compact ? 48
                                : Math.max(80, labelMetrics.width + 24 + 8 + 24)
                            // M3 touch target: 48dp minimum
                            implicitHeight: compact ? 48 : 44
                            TextMetrics {
                                id: labelMetrics
                                font.family: ThemeEngine.fontUi; font.pixelSize: 12
                                text: navBtn.labelText
                            }
                            background: Rectangle {
                                // 5WHY: no hover feedback on nav items — desktop
                                // users got zero affordance that tabs are clickable.
                                // Subtle primary tint on hover (M3 state-layer 6%),
                                // stronger for active. Animated for polish.
                                color: navBtn.active ? Qt.alpha(ThemeEngine.colors.primary, 0.12)
                                     : navBtn.hovered ? Qt.alpha(ThemeEngine.colors.primary, 0.07)
                                     : "transparent"
                                radius: ThemeEngine.radius.md
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }
                            contentItem: Item {
                                // Compact (mobile): M3 24dp icon, 48dp touch target
                                AppIcon {
                                    visible: content.compact
                                    anchors.centerIn: parent
                                    name: modelData.icon; size: 24
                                    color: navBtn.active ? ThemeEngine.colors.primary
                                                          : ThemeEngine.colors.textSecondary
                                }
                                // Desktop: M3 icon 24dp + label 12sp, 8dp gap
                                RowLayout {
                                    visible: !content.compact
                                    anchors.centerIn: parent; spacing: 8
                                    AppIcon {
                                        name: modelData.icon; size: 24
                                        color: navBtn.active ? ThemeEngine.colors.primary
                                                              : ThemeEngine.colors.textSecondary
                                    }
                                    Label {
                                        text: navBtn.labelText
                                        // 5WHY: nav labels are UI chrome — proportional fontUi.
                                        font.family: ThemeEngine.fontUi; font.pixelSize: 12
                                        font.weight: navBtn.active ? Font.DemiBold : Font.Normal
                                        color: navBtn.active ? ThemeEngine.colors.primary
                                                              : ThemeEngine.colors.textSecondary
                                    }
                                }
                            }
                            onClicked: {
                                if (navBlocked) { closeCurrentOverlay(); return }
                                switchToTab(index)
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Item { width: compact ? 0 : 4; visible: !compact }
            }
        }
    }
}
