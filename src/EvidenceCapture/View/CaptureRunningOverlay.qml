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
//
// 5WHY (progress bar invisible): color:"transparent" delegated 100% of visual
// presence to drop-shadows.  ShadowIcon/ShadowSeparator/blinking-dot used
// anchors.rightMargin (shrinks) instead of leftMargin (offsets) — shadows
// stayed at (0,0) fully covered by foreground.  Root cause: no defensive
// background or border.  Fix: Qt.alpha(surface, 0.45) background + explicit
// left+top anchors with full width/height.  Full analysis in the design doc.
//
// 5WHY (High-DPI shadow offset — Fix 7):
//
//   QML uses logical (device-independent) pixels.  1 logical pixel maps
//   to `Screen.devicePixelRatio` physical pixels: 1px at 1× = 1 phys px,
//   2 phys px at 2× (Retina), 3 phys px at 3× (Retina HD).  The shadow
//   offset of 1 logical pixel therefore grows MORE visible on high-DPI
//   displays, not less.  The kShadowOffset constant documents this design
//   invariant — no DPI scaling is needed; Qt handles it automatically.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T
import "../widgets"

// ═════════════════════════════════════════════════════════════════════════════
// Shadow components (ShadowIcon, ShadowSeparator, ShadowText) are now in
// src/Common/View/widgets/ — imported via "../widgets" above.  Qt 6.8.3's
// qmlimportscanner does not support inline `component` declarations in QRC
// builds, producing "Syntax error" during qt-cmake configure.  Moving them
// to standalone QML files resolves the scanner limitation while preserving
// the single-edit design decision for shadow conventions.
// ═════════════════════════════════════════════════════════════════════════════
// Root — semi-transparent floating HUD with shadow-augmented child elements
// ═════════════════════════════════════════════════════════════════════════════
// 5WHY (defense-in-depth): The background uses a subtle 45%-opaque surface
// colour as a fallback visibility layer.  Drop-shadows on child text/icons/
// separators/dot provide readability enhancement; the background ensures the
// bar remains a visible pill shape even if every shadow were to fail.

Rectangle {
    id: root
    z: 2100

    signal cancelled()

    // ── Shorthand for the display model ──────────────────────────────
    readonly property var d: captureOrchestrator
        ? captureOrchestrator.sessionDisplay : null

    // ── Layout: top-right aligned, auto-width, fixed height ──────────
    anchors.top: parent.top
    anchors.right: parent.right
    // 5WHY: Android system status bar is ~24dp tall.  A 12px top margin
    // places the floating pill behind status bar icons (signal, battery).
    // On iOS, the status bar icons float over content, so 12px is fine.
    // Use platform-aware top margin: 36px on Android, 12px elsewhere.
    // Qt 6 always returns a string from Qt.platform.os — String() coercion removed.
    anchors.topMargin: Qt.platform.os === "android" ? 36 : 12
    anchors.rightMargin: 12
    // 5WHY: On narrow screens (<320px) with both screenshot+recording mode
    // active, the RowLayout's implicitWidth can exceed the parent width,
    // pushing the bar past the left edge.  Cap width to parent width minus
    // the right margin so the bar never overflows off-screen.
    width: Math.min(parent.width - anchors.rightMargin * 2,
                    Math.max(0, statusRow.implicitWidth + leftPadding + rightPadding))
    height: 36
    radius: 10
    clip: false  // 5WHY: clip=false so Text.Raised shadow renders outside bounds

    leftPadding: 14
    rightPadding: 14

    // 5WHY (root-cause fix, round 2): Qt.alpha(surface, 0.45) on a surface
    // background is nearly invisible — same hue, 45% opacity blends into the
    // diagnostic page.  Use card color at higher opacity for visible contrast:
    // card (#1E293B) is distinctly lighter than surface (#0F172A) in dark theme.
    color: Qt.alpha(T.ThemeEngine.colors.card, 0.88)

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
        // 5WHY: Shadow is a sibling (not child) of the blinking dot so it
        // does NOT inherit the opacity animation — shadow stays fully visible
        // while the red dot blinks between 1.0 and 0.2 opacity.
        Item {
            implicitWidth: 8; implicitHeight: 8
            visible: d ? d.showRecordingDot : false
            // Drop-shadow layer (no animation — constant opacity)
            // Offset to bottom-right matches Text.Raised convention.
            // 5WHY: Same anchors.fill + margin bug as ShadowIcon/ShadowSeparator.
            // Explicit left+top anchors with full width/height so the 8×8 shadow
            // at (1,1)→(8,8) peeks 1px beyond the 8×8 foreground dot at (0,0).
            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: 1
                anchors.top: parent.top
                anchors.topMargin: 1
                width: parent.width
                height: parent.height
                radius: 4
                color: "#80000000"
            }
            // Blinking foreground dot
            Rectangle {
                anchors.fill: parent
                radius: 4
                color: T.ThemeEngine.failRed
                SequentialAnimation on opacity {
                    running: parent.visible; loops: Animation.Infinite
                    NumberAnimation { from: 1.0; to: 0.2; duration: 600 }
                    NumberAnimation { from: 0.2; to: 1.0; duration: 600 }
                }
            }
        }

        // Elapsed time: HH:MM:SS — formatted by C++ Display Model
        // 5WHY: ShadowText (inline component) provides Text.Raised drop-shadow
        ShadowText {
            text: d ? d.elapsedDisplay : "00:00:00"
            
            font.pixelSize: 13; font.weight: Font.DemiBold
            color: T.ThemeEngine.textPrimary
            Layout.minimumWidth: 72
            horizontalAlignment: Text.AlignHCenter
        }

        // Thin separator (time → screenshot group)
        ShadowSeparator {
            lineColor: Qt.alpha(T.ThemeEngine.textSecondary, 0.20)
            shadowColor: Qt.alpha("#000000", 0.25)
            visible: d ? d.showScreenshotGroup : false
        }

        // ── Screenshot group (hidden in RecordingOnly mode) ──────────
        ShadowIcon {
            iconName: "camera"
            iconSize: 15
            foregroundColor: T.ThemeEngine.cyan
            Layout.leftMargin: 1
            visible: d ? d.showScreenshotGroup : false
        }

        // Screenshot count
        ShadowText {
            text: d ? d.countDisplay : " 0"
            
            font.pixelSize: 13; font.weight: Font.DemiBold
            color: T.ThemeEngine.cyan
            Layout.minimumWidth: 22
            horizontalAlignment: Text.AlignRight
            visible: d ? d.showScreenshotGroup : false
        }

        // Thin separator (screenshot group → task group)
        ShadowSeparator {
            lineColor: Qt.alpha(T.ThemeEngine.textSecondary, 0.20)
            shadowColor: Qt.alpha("#000000", 0.25)
            visible: d ? d.showScreenshotGroup : false
        }

        // Task icon with drop-shadow
        ShadowIcon {
            iconName: "list-checks"
            iconSize: 15
            foregroundColor: T.ThemeEngine.textSecondary
            Layout.leftMargin: 1
        }

        // Task progress: current / total — formatted by C++ Display Model
        RowLayout {
            spacing: 1
            ShadowText {
                text: d ? d.stepDisplay : " 0"
                
                font.pixelSize: 13; font.weight: Font.DemiBold
                color: T.ThemeEngine.cyan
                Layout.minimumWidth: 18
                horizontalAlignment: Text.AlignRight
            }
            ShadowText {
                text: "/"
                
                font.pixelSize: 11
                color: T.ThemeEngine.textSecondary
            }
            ShadowText {
                text: d ? d.totalDisplay : " 0"
                
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
