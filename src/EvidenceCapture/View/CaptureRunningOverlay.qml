// =============================================================================
// CaptureRunningOverlay.qml — Minimal floating status bar during capture
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §2.1
//
// Architecture (Display Model pattern):
//   All display formatting (pad2, pad0, HH:MM:SS) is done in C++ by
//   CaptureSessionDisplay.  QML binds declaratively — zero JavaScript,
//   zero Component.onCompleted, zero Timer polling, zero Connections.
//   Single source of truth, no fragile C++/QML initialization coupling.
//
// Layout:  ● 00:01:23 │ camera  N │ list-checks  N/M
//           ↑          ↑  ↑   ↑       ↑        ↑
//           blinking   │  │   │       │        └─ totalDisplay (right-aligned)
//           red dot    │  │   │       └─ task icon (Lucide MIT, textSecondary)
//                      │  │   └─ countDisplay (right-aligned, cyan)
//                      │  └─ screenshot icon (camera SVG, Lucide MIT, cyan)
//                      └─ elapsedDisplay (HH:MM:SS, monospace)
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// 5WHY: QtQuick.Effects MultiEffect requires Qt6::QuickEffects which is not
// linked in iOS static builds (cmake/dependencies.cmake only has Core,
// Concurrent, Quick, QuickControls2, Network).  The `import QtQuick.Effects`
// caused the Loader to enter Error state silently — the status bar never
// rendered.  Remove the import and use border for visual separation.
// 5WHY (round-2): Changing root from Rectangle to Item broke overlay
// loading on iOS.  All other overlays (CaptureModePanel, CapturePreflight,
// CaptureResultSummary) use Rectangle root and load correctly.  Keep
// Rectangle as root for consistency with the working overlays.
import "../theme" as T
import "../widgets"

Rectangle {
    id: root
    z: 2100

    signal cancelled()

    // ── Shorthand for the display model ──────────────────────────────
    readonly property var d: captureOrchestrator
        ? captureOrchestrator.sessionDisplay : null

    // 5WHY: The old overlay was a large centered modal card (~400x300px)
    // that blocked the entire app UI behind it — navigation, diagnostics,
    // and scrolling were all occluded.  Replace with a compact floating
    // status bar anchored to the top-right corner with transparent
    // background, so the automated capture steps can execute visibly.

    // ── Layout: top-right aligned, auto-width, fixed height ──────────
    anchors.top: parent.top
    anchors.right: parent.right
    anchors.margins: 12
    width: Math.max(0, statusRow.implicitWidth + leftPadding + rightPadding)
    height: 36
    radius: 10
    clip: true

    leftPadding: 14
    rightPadding: 14

    // ── Transparent background with subtle border ────────────────────
    // 5WHY: MultiEffect shadow removed (requires QtQuick.Effects, absent
    // on iOS).  Use a thin semi-transparent border for visual separation.
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.72)
    border.width: 1
    border.color: Qt.alpha("#000000", 0.12)

    // ── Hidden during screenshots ────────────────────────────────────
    opacity: captureOrchestrator && captureOrchestrator.suppressOverlay ? 0 : 1

    // ── Status row ───────────────────────────────────────────────────
    RowLayout {
        id: statusRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: parent.leftPadding
        anchors.rightMargin: parent.rightPadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        // Blinking red recording dot — visible only when recording
        Rectangle {
            implicitWidth: 8; implicitHeight: 8; radius: 4
            color: T.ThemeEngine.failRed
            visible: d ? d.showRecordingDot : false
            SequentialAnimation on opacity {
                running: visible; loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.2; duration: 600 }
                NumberAnimation { from: 0.2; to: 1.0; duration: 600 }
            }
        }

        // Elapsed time: HH:MM:SS — formatted by C++ Display Model
        Label {
            text: d ? d.elapsedDisplay : "00:00:00"
            font.family: T.ThemeEngine.monoFont
            font.pixelSize: 13; font.weight: Font.DemiBold
            color: T.ThemeEngine.textPrimary
            Layout.minimumWidth: 72
            horizontalAlignment: Text.AlignHCenter
        }

        // Thin separator (time → screenshot group)
        Rectangle {
            implicitWidth: 1; implicitHeight: 16
            color: Qt.alpha(T.ThemeEngine.textSecondary, 0.20)
            visible: d ? d.showScreenshotGroup : false
        }

        // ── Screenshot group (hidden in RecordingOnly mode) ──────────
        // Screenshot icon
        AppIcon {
            name: "camera"
            size: 15
            color: T.ThemeEngine.cyan
            Layout.leftMargin: 1
            visible: d ? d.showScreenshotGroup : false
        }

        // Screenshot count
        Label {
            text: d ? d.countDisplay : " 0"
            font.family: T.ThemeEngine.monoFont
            font.pixelSize: 13; font.weight: Font.DemiBold
            color: T.ThemeEngine.cyan
            Layout.minimumWidth: 22
            horizontalAlignment: Text.AlignRight
            visible: d ? d.showScreenshotGroup : false
        }

        // Thin separator (screenshot group → task group)
        Rectangle {
            implicitWidth: 1; implicitHeight: 16
            color: Qt.alpha(T.ThemeEngine.textSecondary, 0.20)
            visible: d ? d.showScreenshotGroup : false
        }

        // Task icon
        AppIcon {
            name: "list-checks"
            size: 15
            color: T.ThemeEngine.textSecondary
            Layout.leftMargin: 1
        }

        // Task progress: current / total — formatted by C++ Display Model
        RowLayout {
            spacing: 1
            Label {
                text: d ? d.stepDisplay : " 0"
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
                text: d ? d.totalDisplay : " 0"
                font.family: T.ThemeEngine.monoFont
                font.pixelSize: 13; font.weight: Font.DemiBold
                color: T.ThemeEngine.textPrimary
                Layout.minimumWidth: 18
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    // ── Tap anywhere on the bar to cancel capture ────────────────────
    MouseArea {
        anchors.fill: parent
        enabled: root.opacity > 0
        onClicked: root.cancelled()
    }

    // ── Wire Flickable for scroll steps ──────────────────────────────
    // 5WHY: Called by AppContent.onStepChanged on every step so
    // ScrollController always has the current page's Flickable.
    function wireFlickable(flickable) {
        if (flickable && captureOrchestrator) {
            captureOrchestrator.setScrollFlickable(flickable)
        }
    }
}
