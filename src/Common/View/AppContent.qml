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
        // Close share dialog
        if (typeof item.shareStage !== 'undefined' && item.shareStage !== 0) item.shareStage = 0
        // 5WHY: Dismissing the cellular warning via nav tap left the run
        // paused at the G2->G3 boundary with no way to resume.  The dialog's
        // own Cancel button calls appState.cancel(); match that behaviour so
        // the run doesn't hang in Running state after navigation.
        if (appState.cellularWarnVisible) { appState.cellularWarnVisible = false; appState.cancel() }
    }

    // ── Single source of truth for tab definitions ───────────────────
    readonly property var tabScreens: ["dashboard","diagnostic","config","settings"]
    readonly property var tabComponents: [dashboardComp, diagnosticComp, configComp, settingsComp]
    readonly property var tabLabels: [Tr.dashboard, Tr.diagnostics, Tr.config, Tr.settings]

    function switchToTab(idx) {
        if (idx < 0 || idx >= tabScreens.length) return
        for (var i = 0; i < stackView.depth; i++) {
            var item = stackView.get(i)
            if (item && item.objectName === tabScreens[idx]) {
                stackView.pop(item)
                return
            }
        }
        stackView.push(tabComponents[idx].createObject(stackView))
    }

    Component { id: diagnosticComp; DiagnosticScreen { objectName: "diagnostic" } }
    Component { id: dashboardComp;  DashboardScreen  { objectName: "dashboard"  } }
    Component { id: configComp;     ConfigScreen     { objectName: "config"     } }
    Component { id: settingsComp;   SettingsScreen   { objectName: "settings"   } }

    // ── Capture overlay loader — single mount point for capture UI stack ──
    // 5WHY: CaptureModePanel/CaptureOrchestrator were unreachable dead code.
    // This Loader is driven by captureOrchestrator signals: modeSelectionRequested
    // → load CaptureModePanel; stateChanged → load running/summary overlays.
    Loader {
        id: captureOverlay
        anchors.fill: parent
        z: 2000
        active: false

        // CaptureState enum values — MUST stay in sync with CaptureStateMachine.h enum order.
        // 5WHY: Hardcoded magic numbers (s === 2, 5, 8, ...) silently break overlay
        // display when the C++ enum is reordered.  Named constants make the mapping
        // visible and grep-able so a reorder is not missed.
        readonly property int kCaptureIdle: 0
        readonly property int kCapturePreflight: 1
        readonly property int kCaptureCountdown: 2
        readonly property int kCaptureCreatingSession: 3
        readonly property int kCaptureStartingRecording: 4
        readonly property int kCaptureExecuting: 5
        readonly property int kCaptureStoppingRecording: 6
        readonly property int kCaptureFinalizing: 7
        readonly property int kCaptureCompleted: 8
        readonly property int kCaptureCancelled: 9
        readonly property int kCaptureFailed: 10

        // ── Listen for orchestrator signals ──────────────────────────────
        Connections {
            target: captureOrchestrator
            function onModeSelectionRequested() {
                captureOverlay.active = true
                captureOverlay.source = "qrc:/qml/capture/CaptureModePanel.qml"
            }
            function onStateChanged() {
                var s = captureOrchestrator.state
                if (s === captureOverlay.kCaptureCountdown) {
                    captureOverlay.active = true
                    captureOverlay.source = "qrc:/qml/capture/CapturePreflightOverlay.qml"
                }
                else if (s === captureOverlay.kCaptureExecuting) {
                    captureOverlay.active = true
                    captureOverlay.source = "qrc:/qml/capture/CaptureRunningOverlay.qml"
                }
                else if (s === captureOverlay.kCaptureCompleted) {
                    captureOverlay.active = true
                    captureOverlay.source = "qrc:/qml/capture/CaptureResultSummary.qml"
                }
                else if (s === captureOverlay.kCaptureIdle || s === captureOverlay.kCaptureCancelled || s === captureOverlay.kCaptureFailed) {
                    captureOverlay.active = false
                    captureOverlay.source = ""
                }
            }
            function onStepChanged(current, total) {
                if (captureOverlay.item && typeof captureOverlay.item.stepProgress !== "undefined") {
                    captureOverlay.item.stepProgress = current
                    captureOverlay.item.stepTotal = total
                }
                // 5WHY: Flickable was wired once on overlay load, but Navigate
                // steps change tabs — the old Flickable is destroyed/replaced.
                // Re-wire on every step so ScrollController always has the
                // current page's Flickable.
                if (captureOverlay.item && typeof captureOverlay.item.wireFlickable === "function") {
                    var cur = stackView.currentItem
                    if (cur) {
                        var f = captureOverlay.findFlickable(cur)
                        if (f) captureOverlay.item.wireFlickable(f)
                    }
                }
            }
            function onActionChanged(action) {
                if (captureOverlay.item && typeof captureOverlay.item.currentStep !== "undefined") {
                    captureOverlay.item.currentStep = action
                }
            }
            function onCaptureCountChanged(count) {
                if (captureOverlay.item && typeof captureOverlay.item.captureCount !== "undefined") {
                    captureOverlay.item.captureCount = count
                }
            }
            function onCaptureCompleted(sessionPath) {
                if (captureOverlay.item && typeof captureOverlay.item.sessionPath !== "undefined") {
                    captureOverlay.item.sessionPath = sessionPath
                    // 5WHY: ResultSummary properties were never populated,
                    // showing 0 screenshots and empty recording/duration.
                    captureOverlay.item.totalScreenshots = captureOrchestrator.captureCount
                    captureOverlay.item.recordingFile = captureOrchestrator.isRecordingCapture() ? "recording.mp4" : ""
                    captureOverlay.item.elapsedTime = captureOrchestrator.elapsedSeconds + "s"
                }
            }
        }

        // ── Wire loaded panel signals to orchestrator ────────────────────
        onLoaded: {
            if (!item) return
            // CaptureModePanel → start capture
            if (typeof item.startRequested !== "undefined") {
                item.startRequested.connect(function(mode, url) {
                    captureOrchestrator.startCapture(mode, url)
                })
            }
            // Any panel → cancel
            if (typeof item.cancelled !== "undefined") {
                item.cancelled.connect(function() {
                    captureOrchestrator.cancel()
                    captureOverlay.active = false
                    captureOverlay.source = ""
                })
            }
            // Preflight countdown finished
            if (typeof item.countdownFinished !== "undefined") {
                // 5WHY: start() was never called after loading the preflight
                // overlay, so the countdown never animated — it stayed at "3".
                // Kick off the countdown as soon as the overlay is ready.
                if (typeof item.start === "function") item.start()
                item.countdownFinished.connect(function() {
                    // 5WHY: The C++ side has an independent 3s QTimer that
                    // auto-advances CountdownToStart→CreatingSession. The QML
                    // countdown is a visual animation only — when it finishes,
                    // the C++ timer has already fired or will fire within ~50ms.
                    // No action needed here; onStateChanged handles the overlay.
                })
            }
            // Result summary dismissed
            if (typeof item.dismissed !== "undefined") {
                item.dismissed.connect(function() {
                    captureOverlay.active = false
                    captureOverlay.source = ""
                })
            }
            // Wire ScrollController when running overlay is on diagnostic page
            if (typeof item.wireFlickable === "function") {
                var currentItem = stackView.currentItem
                if (currentItem) {
                    var flick = findFlickable(currentItem)
                    if (flick) item.wireFlickable(flick)
                }
            }
        }

        function findFlickable(obj) {
            if (!obj) return null
            if (typeof obj.contentHeight !== "undefined" && typeof obj.contentY !== "undefined")
                return obj
            if (obj.children) {
                for (var i = 0; i < obj.children.length; i++) {
                    var r = findFlickable(obj.children[i])
                    if (r) return r
                }
            }
            return null
        }
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // ── Screen stack (fills remaining space above the dock) ──────
        StackView {
            id: stackView
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            initialItem: diagnosticComp
        }

        // ── Bottom dock navigation bar (Material Design 3 compliant) ──
        Rectangle {
            Layout.fillWidth: true
            // M3: 80dp full, 56dp compact desktop.  Apple HIG: 44-48pt mobile.
            implicitHeight: compact ? 48 : 56
            color: ThemeEngine.colors.navBar
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
                Row { spacing: compact ? 4 : 4
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
                                Tr.lang // force re-evaluation on language change
                                return content.tabLabels[index] || modelData.screen
                            }
                            // M3: icon 24dp + gap 8dp + text + padding 12dp each side
                            implicitWidth: compact ? 48
                                : Math.max(80, labelMetrics.width + 24 + 8 + 24)
                            // M3 touch target: 48dp minimum
                            implicitHeight: compact ? 48 : 44
                            TextMetrics {
                                id: labelMetrics
                                font.family: ThemeEngine.monoFont; font.pixelSize: 12
                                text: navBtn.labelText
                            }
                            background: Rectangle {
                                color: navBtn.active ? Qt.alpha(ThemeEngine.colors.primary, 0.12) : "transparent"
                                radius: ThemeEngine.radius.md
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
                                        font.family: ThemeEngine.monoFont; font.pixelSize: 12
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
