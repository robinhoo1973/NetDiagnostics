// =============================================================================
// CaptureRunningOverlay.qml — Minimal floating status bar during capture
// =============================================================================
// Design: compact auto-hiding status bar, top-right aligned, transparent
// background with shadow.  Does NOT block underlying UI interaction.
// Hidden during screenshots via suppressOverlay opacity binding.
//
// Layout:  ● 00:01:23 │ 📷  3 │ ✓✓  5/12
//           ↑          ↑  ↑   ↑    ↑ ↑
//           blinking   │  │   │    │ └─ total steps (right-aligned, 2-digit)
//           red dot    │  │   │    └─ separator
//                      │  │   └─ screenshot count (right-aligned, 2-digit)
//                      │  └─ screenshot icon (camera SVG, Lucide)
//                      └─ recording duration (HH:MM:SS)
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../theme" as T
import "../widgets"

Rectangle {
    id: root
    z: 2100

    signal cancelled()

    // ── Derived: true when screenshots are enabled for this session ──
    // 5WHY: RecordingOnly mode takes zero screenshots — a persistent
    // "camera 0" indicator is dead UI.  Bind visibility of the entire
    // camera+count+separator group to this flag so they collapse when
    // screenshots are not being taken.  RowLayout auto-excludes items
    // with visible=false, so no manual layout adjustment is needed.
    // 5WHY: readonly prevents accidental assignment from permanently
    // disconnecting the binding.
    readonly property bool showScreenshotGroup: captureOrchestrator
        && captureOrchestrator.wantsScreenshot

    // 5WHY: The old overlay was a large centered modal card (~400x300px)
    // that blocked the entire app UI behind it — navigation, diagnostics,
    // and scrolling were all occluded.  Replace with a compact floating
    // status bar anchored to the top-right corner with transparent
    // background, so the automated capture steps can execute visibly.
    //
    // 5WHY: During screenshot capture, the orchestrator sets
    // suppressOverlay=true, which drives this opacity binding to 0.
    // This prevents the status bar from appearing in evidence PNGs.

    // ── Layout: top-right aligned, auto-width, fixed height ──────────
    anchors.top: parent.top
    anchors.right: parent.right
    anchors.margins: 12
    width: Math.max(0, statusRow.implicitWidth + leftPadding + rightPadding)
    height: 36
    radius: 10

    // ── Use padding so the RowLayout doesn't need anchors.centerIn ──
    // 5WHY: anchors.centerIn on a RowLayout inside a Rectangle with
    // width bound to the RowLayout's implicitWidth creates a fragile
    // dependency chain.  Use padding on the container instead — the
    // width formula becomes implicitWidth + padding, and the RowLayout
    // fills the padded area naturally via anchors.fill.
    leftPadding: 14
    rightPadding: 14

    // ── Transparent background with shadow ───────────────────────────
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.72)
    // 5WHY: MultiEffect (QtQuick.Effects) replaces DropShadow
    // (Qt5Compat.GraphicalEffects).  The compat module is NOT linked
    // by this project — DropShadow would cause a QML load error on iOS.
    // MultiEffect is already used by AppIcon.qml with the same import.
    //
    // 5WHY: layer.enabled bound to opacity>0 so the GPU offscreen buffer
    // and shadow compositing stop during screenshot suppression instead
    // of rendering an invisible element every frame.
    layer.enabled: opacity > 0
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowBlur: 0.6
        blurMax: 10
        shadowColor: Qt.alpha("#000000", 0.25)
        shadowVerticalOffset: 2
        shadowHorizontalOffset: 1
    }

    // ── Hidden during screenshots ────────────────────────────────────
    opacity: captureOrchestrator && captureOrchestrator.suppressOverlay ? 0 : 1

    // ── Status row: ● 00:00:00 │ camera  3 │ list-checks  5/12 ──
    RowLayout {
        id: statusRow
        anchors.fill: parent
        spacing: 8

        // Blinking red recording dot — visible only when recording
        // 5WHY: In screenshot-only mode there is no recording, so a
        // blinking red "recording" indicator is misleading.  Bind
        // visibility to the orchestrator's isRecordingCapture property.
        Rectangle {
            implicitWidth: 8; implicitHeight: 8; radius: 4
            color: T.ThemeEngine.failRed
            visible: captureOrchestrator && captureOrchestrator.isRecordingCapture
            SequentialAnimation on opacity {
                // 5WHY: Bind running to visible so the animation driver
                // stops ticking when the dot is hidden (screenshot-only
                // mode).  Unconditional running wastes CPU on every frame.
                running: visible; loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.2; duration: 600 }
                NumberAnimation { from: 0.2; to: 1.0; duration: 600 }
            }
        }

        // Elapsed time: HH:MM:SS
        // 5WHY: monospace font + fixed minimum width prevents the bar
        // from resizing horizontally as time digits change (e.g. 0→1
        // character width differences in proportional fonts).
        Label {
            id: elapsedLabel
            text: "00:00:00"
            font.family: T.ThemeEngine.monoFont
            font.pixelSize: 13; font.weight: Font.DemiBold
            color: T.ThemeEngine.textPrimary
            Layout.minimumWidth: 72
            horizontalAlignment: Text.AlignHCenter
        }

        // Thin separator (between time and screenshot group)
        // 5WHY: In RecordingOnly mode the screenshot group is hidden — this
        // separator must also hide, otherwise it becomes an orphaned visual
        // element floating between the elapsed time and the task icon.
        Rectangle {
            implicitWidth: 1; implicitHeight: 16
            color: Qt.alpha(T.ThemeEngine.textSecondary, 0.20)
            visible: showScreenshotGroup
        }

        // ── Screenshot group (hidden in RecordingOnly mode) ──────────
        // Screenshot icon — camera (Lucide, MIT), simple body + lens
        AppIcon {
            name: "camera"
            size: 15
            color: T.ThemeEngine.cyan
            Layout.leftMargin: 1
            visible: showScreenshotGroup
        }

        // Screenshot count — right-aligned, minimum 2-digit width
        Label {
            id: countLabel
            text: " 0"
            font.family: T.ThemeEngine.monoFont
            font.pixelSize: 13; font.weight: Font.DemiBold
            color: T.ThemeEngine.cyan
            Layout.minimumWidth: 22
            horizontalAlignment: Text.AlignRight
            visible: showScreenshotGroup
        }

        // Thin separator (between screenshot group and task group)
        Rectangle {
            implicitWidth: 1; implicitHeight: 16
            color: Qt.alpha(T.ThemeEngine.textSecondary, 0.20)
            visible: showScreenshotGroup
        }

        // Task icon — checklist (Lucide, MIT), lines + checks = task progress
        AppIcon {
            name: "list-checks"
            size: 15
            color: T.ThemeEngine.textSecondary
            Layout.leftMargin: 1
        }

        // Task progress: current / total
        // 5WHY: Both numbers use monospace right-aligned with fixed
        // minimum width so " 1/12" and "10/12" align properly.
        RowLayout {
            spacing: 1
            Label {
                id: stepCurrentLabel
                text: " 0"
                font.family: T.ThemeEngine.monoFont
                font.pixelSize: 13; font.weight: Font.DemiBold
                color: T.ThemeEngine.cyan
                Layout.minimumWidth: 18
                horizontalAlignment: Text.AlignRight
            }
            Label {
                text: "/"
                font.family: T.ThemeEngine.monoFont
                font.pixelSize: 11
                color: T.ThemeEngine.textSecondary
            }
            Label {
                id: stepTotalLabel
                text: " 0"
                font.family: T.ThemeEngine.monoFont
                font.pixelSize: 13; font.weight: Font.DemiBold
                color: T.ThemeEngine.textPrimary
                Layout.minimumWidth: 18
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    // ── Tap anywhere on the bar to cancel capture ────────────────────
    // 5WHY: The entire bar is a single tap target.  On mobile, a
    // dedicated Cancel button inside a 36px bar would be ~40px wide —
    // difficult to tap reliably.  Making the whole bar tappable is more
    // ergonomic and consistent with iOS/Android status-bar patterns.
    MouseArea {
        anchors.fill: parent
        // 5WHY: When suppressOverlay is true (during screenshots) the
        // parent Rectangle's opacity is 0 but MouseArea still intercepts
        // clicks — a tap in the top-right corner would silently cancel
        // the capture.  Disable the MouseArea when the bar is hidden so
        // clicks pass through to the underlying UI.
        // 5WHY: In Qt6, MouseArea inherits from Item and has its own
        // opacity property (default 1.0).  Unqualified "opacity" resolves
        // to MouseArea.opacity, NOT root.opacity — so the enabled check
        // was always true.  Explicitly scope to root.opacity.
        enabled: root.opacity > 0
        onClicked: root.cancelled()
    }

    // ── Zero-pad helper: "0" + n for n<10, else n as string ────────
    // 5WHY: String.prototype.padStart is ES2017 — unavailable on Qt 5.x
    // QML engines (ES5-only).  ThemeEngine.qml explicitly documents this
    // limitation and provides pad2() for space-padding.  Zero-padding
    // for HH:MM:SS time display needs its own ES5-compatible helper.
    function padZero(n) { return (n < 10 ? "0" : "") + n }

    // ── Poll elapsed time every second ───────────────────────────────
    // 5WHY: Timer runs only while the overlay is active.  QML Loader
    // destroys the item when source changes, so the Timer stops
    // naturally when the overlay is replaced.
    Timer {
        id: elapsedTimer
        interval: 1000; repeat: true; running: true
        onTriggered: updateElapsed()
    }

    // 5WHY: Extracted from the Timer's onTriggered so
    // Component.onCompleted can also call it — see below.
    function updateElapsed() {
        if (captureOrchestrator) {
            var secs = captureOrchestrator.elapsedSeconds
            var h = Math.floor(secs / 3600)
            var m = Math.floor((secs % 3600) / 60)
            var s = secs % 60
            elapsedLabel.text = padZero(h) + ":" + padZero(m) + ":" + padZero(s)
        }
    }

    // ── Initialize from current C++ state on load ────────────────────
    // 5WHY: The overlay may load mid-capture (e.g. after preflight).
    // stepChanged and captureCountChanged signals only fire on
    // subsequent changes — the initial values must be read directly
    // so the status bar shows correct data from the first frame.
    //
    // 5WHY: ThemeEngine.pad2() is the shared ES5-compatible pad helper
    // (ThemeEngine.qml line 145).  Use it instead of a local copy so
    // the source-of-truth stays in the theme module.
    Component.onCompleted: {
        if (captureOrchestrator) {
            // 5WHY: executeNextStep() emits stepChanged(0, total) and
            // increments m_currentStep BEFORE stateChanged fires.  By the
            // time this overlay loads, m_currentStep has already advanced
            // past step 0.  Using currentStep (without +1) recovers the
            // 1-indexed display value that the missed stepChanged(0, ...)
            // would have produced.  The Connections handler still applies
            // +1 for subsequent stepChanged signals.
            stepCurrentLabel.text = T.ThemeEngine.pad2(captureOrchestrator.currentStep)
            stepTotalLabel.text = T.ThemeEngine.pad2(captureOrchestrator.totalSteps)
            countLabel.text = T.ThemeEngine.pad2(captureOrchestrator.captureCount)
            // 5WHY: The elapsed timer fires 1 s after start — during the
            // first second the bar would show stale "00:00:00".  Seed the
            // label with the current value so it is correct from frame 0.
            updateElapsed()
        }
    }

    // ── Update status from C++ signals ───────────────────────────────
    Connections {
        target: captureOrchestrator
        function onStepChanged(current, total) {
            // 5WHY: C++ emits 0-indexed current, totalSteps.  Display
            // 1-indexed for the user (step 1 of N).
            stepCurrentLabel.text = T.ThemeEngine.pad2(current + 1)
            stepTotalLabel.text = T.ThemeEngine.pad2(total)
        }
        function onCaptureCountChanged(count) {
            countLabel.text = T.ThemeEngine.pad2(count)
        }
    }

    // ── Wire Flickable for scroll steps ──────────────────────────────
    // 5WHY: ScrollController needs a Flickable reference to perform
    // smooth scrolls during recording-mode capture.  Without this,
    // Scroll steps silently no-op (ScrollController::m_flickable==nullptr).
    // The AppContent onStepChanged handler calls this every step so the
    // ScrollController always has the current page's Flickable.
    function wireFlickable(flickable) {
        if (flickable && captureOrchestrator) {
            captureOrchestrator.setScrollFlickable(flickable)
        }
    }
}
