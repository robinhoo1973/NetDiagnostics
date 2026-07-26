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
        // 5WHY: onCaptureFailed sets this synchronously. onStateChanged
        // checks this (not item.isError which is async via Qt.callLater)
        // to prevent the error overlay from being hidden mid-incubation.
        property bool pendingError: false
        // 5WHY: Qt.callLater runs BEFORE Loader incubation, so item is null
        // when the callback fires.  Store error params on the Loader itself
        // (not the item) so onLoaded can apply them after incubation completes.
        property string pendingErrorCode: ""
        property string pendingErrorMessage: ""

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

        // 5WHY: If a capture QML source fails to load (import resolution failure,
        // QRC corruption, platform-specific incompatibility), the Loader silently
        // enters status=Error and item remains null.  Without this handler, the
        // user sees nothing — no panel, no error, no indication anything went wrong.
        // Log the error so a developer investigating a blank overlay can diagnose
        // the failure from console output on iOS/Android.
        onStatusChanged: {
            if (captureOverlay.status === Loader.Error) {
                console.warn("CaptureOrchestrator: Loader failed to load",
                              captureOverlay.source,
                              "- check QML imports and QRC paths")
            }
        }

        // 5WHY: Indirect binding (property var → captureOrchestrator → Connections.target)
        // was fragile across Qt 6.x versions — some QML engines lost the binding at
        // QML load time, leaving target=null and silently disabling all signal handlers.
        // Use captureOrchestrator directly to eliminate the indirection layer.
        // ── Listen for orchestrator signals ──────────────────────────────
        Connections {
            target: captureOrchestrator
            function onModeSelectionRequested() {
                // 5WHY: pendingError must be false for all non-error transitions
                // so onStateChanged(Failed) can distinguish error vs normal flow.
                captureOverlay.pendingError = false; captureOverlay.active = true
                captureOverlay.source = "qrc:/qml/capture/CaptureModePanel.qml"
            }
            function onStateChanged() {
                var s = captureOrchestrator.state
                if (s === captureOverlay.kCaptureCountdown) {
                    captureOverlay.pendingError = false; captureOverlay.active = true
                    // 5WHY: Separate DND guide from countdown.  On platforms
                    // that need manual Focus setup (iOS), show the DND guide
                    // first.  On platforms with programmatic DND (Android),
                    // go directly to the countdown overlay.
                    // needsFocusModeSetup: iOS always true, Android conditional.
                    if (captureOrchestrator.needsFocusModeSetup) {
                        captureOverlay.source = "qrc:/qml/capture/CapturePreflightOverlay.qml"
                    } else {
                        captureOverlay.source = "qrc:/qml/capture/CaptureCountdownOverlay.qml"
                    }
                }
                else if (s === captureOverlay.kCaptureExecuting) {
                    captureOverlay.pendingError = false; captureOverlay.active = true
                    captureOverlay.source = "qrc:/qml/capture/CaptureRunningOverlay.qml"
                }
                else if (s === captureOverlay.kCaptureCompleted) {
                    captureOverlay.pendingError = false; captureOverlay.active = true
                    captureOverlay.source = "qrc:/qml/capture/CaptureResultSummary.qml"
                }
                // 5WHY: intermediate states (CreatingSession→StartingRecording and
                // StoppingRecording→Finalizing) had no overlay handler.  The
                // Countdown overlay stayed visible during these transitions,
                // making it appear frozen.  Clear the overlay source but keep
                // active=true so the next state (ExecutingSteps/Completed) can
                // load its overlay without re-incubating the Loader.
                else if (s === captureOverlay.kCaptureCreatingSession
                      || s === captureOverlay.kCaptureStartingRecording
                      || s === captureOverlay.kCaptureStoppingRecording
                      || s === captureOverlay.kCaptureFinalizing) {
                    captureOverlay.pendingError = false
                    captureOverlay.source = ""
                }
                else if (s === captureOverlay.kCaptureIdle || s === captureOverlay.kCaptureCancelled) {
                    captureOverlay.active = false
                    captureOverlay.pendingError = false
                    captureOverlay.source = ""
                }
                else if (s === captureOverlay.kCaptureFailed) {
                    // 5WHY: onCaptureFailed() sets pendingError=true synchronously
                    // BEFORE the state machine transitions to Failed.  Use the
                    // synchronous pendingError flag (not item.isError which is set
                    // asynchronously via Qt.callLater) so the guard works reliably
                    // even when the Loader hasn't finished incubating.
                    //
                    // If pendingError is true → error overlay was requested by
                    // onCaptureFailed → keep the overlay visible.
                    // If pendingError is false → failure without captureFailed
                    // signal (e.g. corrupt manifest) → no overlay loaded → hide.
                    if (!captureOverlay.pendingError) {
                        captureOverlay.active = false
                        captureOverlay.source = ""
                    }
                    // 5WHY: Don't reset pendingError here. onLoaded reads it
                    // to decide whether to apply errorCode/errorMessage to the
                    // incubated item.  If we zero it here, onLoaded sees false
                    // and skips the error property injection.  Reset happens in
                    // onLoaded after the properties are applied (see below).
                }
            }
            function onStepChanged(current, total) {
                // 5WHY: The new CaptureRunningOverlay handles step-label
                // updates via its own Connections block (stepProgress/
                // stepTotal aliases were removed).  Only the Flickable
                // re-wire is needed here — ScrollController must track
                // the current page's Flickable as Navigate steps switch
                // tabs and the old Flickable is destroyed/replaced.
                if (captureOverlay.item && typeof captureOverlay.item.wireFlickable === "function") {
                    var cur = stackView.currentItem
                    if (cur) {
                        var f = captureOverlay.findFlickable(cur)
                        if (f) captureOverlay.item.wireFlickable(f)
                    }
                }
            }
            // 5WHY: onActionChanged + onCaptureCountChanged handlers were
            // removed.  The new compact status bar displays step numbers
            // and screenshot count via its own Connections block, so the
            // aliases (currentStep, captureCount) no longer exist.
            // Action text (e.g. "Navigating to Diagnostics...") is
            // deliberately omitted from the compact bar — only numeric
            // progress indicators fit in the 36px floating pill.  The
            // C++ emission of actionChanged() is preserved for potential
            // future use (e.g. a detail-expansion panel).
            // Setting the removed aliases via typeof-guarded if-blocks
            // was dead code that silently skipped every invocation.
            function onCaptureCompleted(sessionPath) {
                if (captureOverlay.item && typeof captureOverlay.item.sessionPath !== "undefined") {
                    captureOverlay.item.sessionPath = sessionPath
                    // 5WHY: ResultSummary properties were never populated,
                    // showing 0 screenshots and empty recording/duration.
                    captureOverlay.item.totalScreenshots = captureOrchestrator.captureCount
                    // 5WHY: isRecordingCapture() reads m_recording which
                    // restoreSystemState() clears BEFORE the deferred
                    // captureCompleted signal fires.  wasRecordingSession()
                    // reads m_captureMode instead, which survives
                    // restoreSystemState() — see CaptureOrchestrator.h:57-63.
                    captureOverlay.item.recordingFile = captureOrchestrator.wasRecordingSession() ? "recording.mp4" : ""
                    captureOverlay.item.elapsedTime = captureOrchestrator.elapsedSeconds + "s"
                }
            }
            function onCaptureFailed(errorCode, userMessage) {
                // 5WHY: set pendingError=true synchronously so onStateChanged(Failed)
                // can distinguish this path from a 'silent' Failed transition
                // (e.g. corrupt manifest) and keep the error overlay visible.
                //
                // Store error params on the Loader (not via Qt.callLater which
                // runs before incubation — item would be null).  onLoaded reads
                // pendingErrorCode/pendingErrorMessage and applies them to the
                // fully-incubated item, avoiding the timing race entirely.
                captureOverlay.pendingError = true
                captureOverlay.pendingErrorCode = errorCode
                captureOverlay.pendingErrorMessage = userMessage
                captureOverlay.active = true
                captureOverlay.source = "qrc:/qml/capture/CaptureResultSummary.qml"
            }
        }

        // ── Wire loaded panel signals to orchestrator ────────────────────
        onLoaded: {
            if (!item) return
            // 5WHY: Apply error properties stored on the Loader during
            // onCaptureFailed.  At this point the Loader has finished
            // incubation so item is guaranteed to exist.  This replaces
            // the broken Qt.callLater pattern which ran before incubation.
            if (captureOverlay.pendingError && typeof item.isError !== "undefined") {
                item.isError = true
                item.errorCode = captureOverlay.pendingErrorCode
                item.errorMessage = captureOverlay.pendingErrorMessage
            }
            // 5WHY: Clear all pending error state after the Loader has
            // incubated and applied properties.  If we clear before
            // onLoaded fires, the above block skips the error injection.
            // Leaving stale errorCode/errorMessage from a previous
            // capture risks leaking error text into an unrelated panel.
            captureOverlay.pendingError = false
            captureOverlay.pendingErrorCode = ""
            captureOverlay.pendingErrorMessage = ""
            // CaptureModePanel → start capture
            if (typeof item.startRequested !== "undefined") {
                item.startRequested.connect(function(mode, url) {
                    if (captureOrchestrator !== null) {
                        captureOrchestrator.startCapture(mode, url)
                    }
                })
            }
            // Any panel → cancel
            if (typeof item.cancelled !== "undefined") {
                item.cancelled.connect(function() {
                    if (captureOrchestrator !== null) {
                        captureOrchestrator.cancel()
                    }
                    captureOverlay.active = false
                    captureOverlay.pendingError = false
                    captureOverlay.source = ""
                })
            }
            // Preflight overlay → user confirmed DND → switch to countdown
            if (typeof item.requestCountdown !== "undefined") {
                item.requestCountdown.connect(function() {
                    // 5WHY: Clear the DND guide and load the standalone
                    // countdown overlay.  The countdown will auto-start
                    // via its own start() function in the next onLoaded.
                    captureOverlay.source = "qrc:/qml/capture/CaptureCountdownOverlay.qml"
                })
            }
            // Countdown overlay → auto-start + handle finish
            if (typeof item.countdownFinished !== "undefined") {
                // 5WHY: start() was never called after loading the countdown
                // overlay, so the countdown never animated — it stayed at "5".
                // Kick off the countdown as soon as the overlay is ready.
                if (typeof item.start === "function") item.start()
                item.countdownFinished.connect(function() {
                    // 5WHY: The QML countdown is now the sole authority.
                    // When the visual 5-4-3-2-1 reaches 0, call the orchestrator
                    // to transition CountdownToStart → CreatingSession.
                    // Clear the overlay immediately so the countdown UI
                    // doesn't linger during session creation and recording
                    // startup (which may block on system permission dialogs).
                    captureOverlay.pendingError = false
                    captureOverlay.source = ""
                    if (captureOrchestrator !== null) {
                        captureOrchestrator.onCountdownFinished()
                    }
                })
            }
            // Result summary dismissed
            if (typeof item.dismissed !== "undefined") {
                item.dismissed.connect(function() {
                    captureOverlay.active = false
                    captureOverlay.pendingError = false
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
