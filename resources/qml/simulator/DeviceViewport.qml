// =============================================================================
// DeviceViewport.qml — Simulated device / OS viewport.
//
// Renders a device frame (bezel, rounded corners, dynamic island) around the
// production AppContent, scaled proportionally to fit the available space.
//
// This is the "Device / OS Viewport" described in build/simulator.md §四.2.
// Screenshots and recordings target ONLY this item (not the whole window).
// =============================================================================
import QtQuick
import "../theme"
import "../widgets"
import "../"

Rectangle {
    id: viewport
    objectName: "deviceViewport"

    // ── Public API ──────────────────────────────────────────────────────
    property var deviceProfile: ({})
    property bool portrait: true
    property bool isMobile: deviceProfile.os === "ios" || deviceProfile.os === "android"

    // Computed geometry (natural pixels)
    readonly property int screenW: portrait ? deviceProfile.w : deviceProfile.h
    readonly property int screenH: portrait ? deviceProfile.h : deviceProfile.w
    readonly property int bezelW:  deviceProfile.bezel || 0
    readonly property int devRadius: deviceProfile.radius || 0
    readonly property int frameW: screenW + bezelW * 2
    readonly property int frameH: screenH + bezelW * 2 + 36  // 36px top bar

    // Scaling
    property real scale: 1.0

    color: "transparent"

    // ── Scale transform ─────────────────────────────────────────────────
    transform: Scale { xScale: viewport.scale; yScale: viewport.scale }

    // Requested natural size
    implicitWidth: frameW
    implicitHeight: frameH

    // ── OS metadata ─────────────────────────────────────────────────────
    readonly property var osMeta: ({
        linux:   { icon:"linux",   color:"#FCC624", label:"Linux"   },
        windows: { icon:"windows", color:"#00A4EF", label:"Windows" },
        macos:   { icon:"apple",   color:"#007AFF", label:"macOS"   },
        ios:     { icon:"apple",   color:"#007AFF", label:"iOS"     },
        android: { icon:"android", color:"#3DDC84", label:"Android" }
    })

    function osEntry(osId) {
        var m = osMeta[osId]
        return m || { icon:"circle", color:"#888", label:osId||"Unknown" }
    }

    // ── Device frame (outer bezel) ──────────────────────────────────────
    Rectangle {
        id: deviceFrame
        width: frameW; height: frameH
        radius: isMobile ? devRadius + bezelW : devRadius + 2
        color: isMobile ? "#0A0A0A" : ThemeEngine.bgDark
        border {
            width: isMobile ? 0 : 1
            color: ThemeEngine.colors.borderCard
        }

        // ── Screen area ─────────────────────────────────────────────────
        Rectangle {
            id: screenRect
            x: bezelW; y: bezelW
            width: screenW; height: screenH + 36
            radius: devRadius
            color: ThemeEngine.bgDark
            clip: true

            // ── Dynamic Island / notch ──────────────────────────────────
            Rectangle {
                visible: deviceProfile.island || false
                anchors { top: parent.top; topMargin: bezelW > 0 ? 6 : 0; horizontalCenter: parent.horizontalCenter }
                width: Math.min(120, parent.width * 0.35); height: 28; radius: 14
                color: "#0A0A0A"
            }

            // ── Production AppContent ────────────────────────────────────
            AppContent {
                id: appContent
                anchors.fill: parent
                onCloseRequested: { /* no-op in simulator */ }
                compact: isMobile
            }
        }
    }

    // ── Recalculate scale to fit container ──────────────────────────────
    function recalcScale(containerW, containerH) {
        if (containerW <= 0 || containerH <= 0) return
        var s = Math.max(0.1, Math.min(
            (containerW - 16) / frameW,
            (containerH - 16) / frameH
        ))
        scale = s
    }
}
