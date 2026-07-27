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
// 5WHY (progress bar invisible — root-cause analysis):
//
//   WHY 1 — The floating progress bar was completely invisible during
//           recording, not just "hard to read."
//     → The root Rectangle had color:"transparent" with no border, and
//       ALL visual weight was delegated to child drop-shadows.  ShadowIcon,
//       ShadowSeparator, and blinking-dot shadow all used anchors.rightMargin
//       / bottomMargin which SHRINK the shadow layer (not offset it), so the
//       shadow stayed at (0,0) — fully covered by foreground elements.  Only
//       Text.Raised text had any visible effect.  90% of bar pixels were
//       transparent; the 10% text-only was unreadable against bright backgrounds.
//
//   WHY 2 — Why were all three shadow implementations broken identically?
//     → The mental model of anchors.rightMargin was inverted.  anchors.fill
//       + anchors.leftMargin:N shifts the child RIGHT (peeking out from
//       behind the foreground at bottom-right).  anchors.rightMargin:N
//       shrinks from the right edge — child stays at (0,0), fully overlapped.
//       This wrong template was copy-pasted from ShadowIcon → ShadowSeparator
//       → blinking dot shadow, propagating the error to all non-text elements.
//
//   WHY 3 — Why was the wrong margin pattern accepted without validation?
//     → No visual testing was performed against any real diagnostic screen
//       background (white logs, colored charts, dark report pages) after the
//       transparent-background refactoring.  The original design was an opaque
//       modal card (Qt.alpha(surface, 0.72)) with no shadows needed.  When it
//       was refactored to a compact transparent floating pill (~e0cac1e), MultiEffect
//       shadows were removed (QtQuick.Effects absent on iOS) and the inline
//       shadow-component replacement was coded but never validated visually.
//
//   WHY 4 — Why wasn't the absence of visible content noticed in code review?
//     → The 5WHY comment block (originally "Label → Text.Raised") only analyzed
//       the text rendering path.  It correctly identified that Label lacks
//       text-shadow support and replaced all Labels with Text + style:Text.Raised.
//       But the icon/separator/dot shadow components were defined BELOW that
//       comment block and were never independently evaluated.  The comment's
//       conclusion — "shadows alone provide readability against any background" —
//       was accepted as fact without verification.
//
//   WHY 5 (ROOT CAUSE) — Single point of failure for visibility.  By setting
//     color:"transparent" and removing the border, 100% of the bar's visual
//     presence depends on child-element drop-shadows working correctly.  When
//     shadows fail (anchor bug, platform rendering difference, GPU driver issue,
//     future refactoring regression), there is ZERO fallback — the bar is
//     invisible.  No semi-transparent background, no border, no container-level
//     contrast mechanism.
//
//     FIX (defense-in-depth):
//       1. Shadow direction fix (already applied): anchors.leftMargin/topMargin
//          correctly offsets shadow to bottom-right for ShadowIcon, ShadowSeparator,
//          and blinking dot.
//       2. DEFENSIVE FALLBACK (this fix): Replace color:"transparent" with a
//          subtle 45%-opaque surface-color background.  Even if all shadows were
//          to fail, the bar retains a visible pill shape on any background.
//          Diagnostics content still shows through the semi-transparent layer.
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
// Inline components — eliminate duplicated shadow-layer patterns
// ═════════════════════════════════════════════════════════════════════════════

// 5WHY: The "AppIcon + 1px-offset shadow AppIcon" pattern was duplicated
// identically for camera and list-checks icons.  If shadow offset, color,
// or rendering approach changes, updating both sites risks drift.  Extract
// once so the icon shadow is a single design decision.
component ShadowIcon: Item {
    property string iconName: ""
    property color  foregroundColor: "white"
    property color  shadowColor: "#80000000"
    property int    iconSize: 15
    // 5WHY: 1 logical pixel = Screen.devicePixelRatio physical pixels.
    // At 2× DPR → 2 phys px, 3× → 3 phys px.  More visible on HiDPI,
    // not less.  Qt Quick handles this automatically via logical coords.
    property int    shadowOffset: 1

    implicitWidth: iconSize; implicitHeight: iconSize

    // Drop-shadow layer — offset to bottom-right matches Text.Raised
    // light-source convention (light from top-left).
    // 5WHY: anchors.fill + leftMargin:N shrinks the child to W-N × H-N
    // (here: 14×14) which is fully covered by the 15×15 foreground AppIcon
    // at (0,0) — invisible shadow.  Anchor only left+top with explicit
    // width/height so the shadow keeps full parent dimensions at offset
    // (1,1)→(15,15), peeking out 1px beyond foreground at (0,0)→(14,14).
    AppIcon {
        anchors.left: parent.left
        anchors.leftMargin: parent.shadowOffset
        anchors.top: parent.top
        anchors.topMargin: parent.shadowOffset
        width: parent.width
        height: parent.height
        name: parent.iconName; size: parent.iconSize
        color: parent.shadowColor
    }
    // Foreground icon
    AppIcon {
        anchors.fill: parent
        name: parent.iconName; size: parent.iconSize
        color: parent.foregroundColor
    }
}

// 5WHY: The thin vertical separator with a 1px-offset shadow child was
// duplicated at both separator positions.  If the separator style changes
// (thickness, color, shadow direction), updating both sites risks drift.
//
// 5WHY (import alias in component body): The import "../theme" as T alias
// MAY NOT resolve inside inline component default property bindings on some
// Qt 6.x versions (especially static iOS builds).  All other inline components
// in this codebase reference ThemeEngine directly without an alias.  To avoid
// a silent QML compilation failure that causes the entire overlay to be blank,
// we defer T.ThemeEngine references to the call site, where the alias IS
// guaranteed to resolve correctly in the root Rectangle's scope.
component ShadowSeparator: Rectangle {
    property color lineColor: "white"    // required — overridden at call site
    property color shadowColor: "white"  // required — overridden at call site

    implicitWidth: 1; implicitHeight: 16
    color: lineColor

    // Drop-shadow — offset to bottom-right matches Text.Raised convention.
    // Anchor only left+top with margins so the shadow keeps the parent's
    // width and is visible (anchors.fill + leftMargin:1 would give the
    // shadow 0 width when the parent is 1px wide — invisible shadow).
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 1; anchors.topMargin: 1
        width: parent.width
        height: parent.height
        color: parent.shadowColor
    }
}

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
    anchors.margins: 12
    width: Math.max(0, statusRow.implicitWidth + leftPadding + rightPadding)
    height: 36
    radius: 10
    clip: false  // 5WHY: clip=false so Text.Raised shadow renders outside bounds

    leftPadding: 14
    rightPadding: 14

    // 5WHY (root-cause fix): A fully transparent background is a single
    // point of failure — if ANY shadow fails (anchor bug, platform render
    // difference, GPU driver), the bar is invisible.  Use a subtle
    // semi-transparent surface-color background as a defensive fallback.
    // At 45% opacity, diagnostic content still shows through while the
    // pill shape remains visually anchored against any background.
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.45)

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
        // 5WHY: Text (QtQuick) replaces Label (QtQuick.Controls) so we
        // can use style: Text.Raised for a built-in drop-shadow — no
        // QtQuick.Effects import needed.
        Text {
            text: d ? d.elapsedDisplay : "00:00:00"
            font.family: T.ThemeEngine.monoFont
            font.pixelSize: 13; font.weight: Font.DemiBold
            color: T.ThemeEngine.textPrimary
            style: Text.Raised
            styleColor: "#80000000"
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
        Text {
            text: d ? d.countDisplay : " 0"
            font.family: T.ThemeEngine.monoFont
            font.pixelSize: 13; font.weight: Font.DemiBold
            color: T.ThemeEngine.cyan
            style: Text.Raised
            styleColor: "#80000000"
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
            Text {
                text: d ? d.stepDisplay : " 0"
                font.family: T.ThemeEngine.monoFont
                font.pixelSize: 13; font.weight: Font.DemiBold
                color: T.ThemeEngine.cyan
                style: Text.Raised
                styleColor: "#80000000"
                Layout.minimumWidth: 18
                horizontalAlignment: Text.AlignRight
            }
            Text {
                text: "/"
                font.family: T.ThemeEngine.monoFont
                font.pixelSize: 11
                color: T.ThemeEngine.textSecondary
                style: Text.Raised
                styleColor: "#80000000"
            }
            Text {
                text: d ? d.totalDisplay : " 0"
                font.family: T.ThemeEngine.monoFont
                font.pixelSize: 13; font.weight: Font.DemiBold
                color: T.ThemeEngine.textPrimary
                style: Text.Raised
                styleColor: "#80000000"
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
