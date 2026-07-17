// ── ZoomBar.qml — Unified floating zoom control for report preview ─
// 5WHY: Each preview tier (QtPdf, NativePdf, WebView) had different zoom
// controls or none at all — no pinch on mobile PDF, no buttons on desktop
// PDF, inconsistent zoom ranges.  This shared component provides:
//   • Geometric √2 zoom steps (Qt-recommended best practice)
//   • Zoom in/out/reset buttons with percentage display
//   • Keyboard shortcuts Ctrl+= / Ctrl+- / Ctrl+0 (desktop only)
//   • Consistent 0.25–5.0 range across all preview tiers
//   • Semi-transparent floating overlay at bottom-right
import QtQuick
import QtQuick.Controls
import "../theme"

Item {
    id: root

    // ── Public API ─────────────────────────────────────────────────────
    property real zoomLevel: 1.0
    readonly property real stepFactor: 1.414  // Math.sqrt(2) — geometric
    property real minimumZoom: 0.25
    property real maximumZoom: 5.0

    // Signals for hosts that need to react (e.g. re-render at new resolution)
    signal zoomInTriggered()
    signal zoomOutTriggered()
    signal zoomResetTriggered()

    readonly property string zoomPercent: Math.round(root.zoomLevel * 100) + "%"

    // Detect mobile for larger touch targets
    readonly property bool isMobile: ThemeEngine.isMobile
    readonly property int btnSize: isMobile ? 44 : 28

    // ── Public functions ───────────────────────────────────────────────
    function zoomIn() {
        var next = zoomLevel * stepFactor
        zoomLevel = Math.min(maximumZoom, next)
        zoomInTriggered()
    }
    function zoomOut() {
        var next = zoomLevel / stepFactor
        zoomLevel = Math.max(minimumZoom, next)
        zoomOutTriggered()
    }
    function zoomReset() {
        zoomLevel = 1.0
        zoomResetTriggered()
    }

    // ── Keyboard shortcuts (desktop only) ──────────────────────────────
    Shortcut {
        sequence: StandardKey.ZoomIn
        onActivated: root.zoomIn()
    }
    Shortcut {
        sequence: StandardKey.ZoomOut
        onActivated: root.zoomOut()
    }
    Shortcut {
        sequence: "Ctrl+0"
        onActivated: root.zoomReset()
    }

    // ── Layout ─────────────────────────────────────────────────────────
    implicitWidth: zoomRow.implicitWidth + 12
    implicitHeight: btnSize + 8

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: Qt.alpha(ThemeEngine.colors.card, 0.92)
        border { width: 1; color: ThemeEngine.colors.borderCard }

        Row {
            id: zoomRow
            anchors.centerIn: parent
            spacing: 4

            // ── Zoom out [−] ──────────────────────────────────────────
            Rectangle {
                width: btnSize; height: btnSize; radius: 5
                color: zoomOutMa.containsMouse ? Qt.alpha(ThemeEngine.cyan, 0.20)
                                              : Qt.alpha(ThemeEngine.cyan, 0.08)
                Label {
                    anchors.centerIn: parent
                    text: "−"  // minus sign
                    font.family: ThemeEngine.monoFont
                    font.pixelSize: root.isMobile ? 16 : 13
                    font.weight: Font.Bold
                    color: ThemeEngine.textPrimary
                }
                MouseArea {
                    id: zoomOutMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: root.zoomOut()
                }
            }

            // ── Zoom percentage ───────────────────────────────────────
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: root.zoomPercent
                font.family: ThemeEngine.monoFont
                font.pixelSize: root.isMobile ? 13 : 11
                color: ThemeEngine.textSecondary
                horizontalAlignment: Text.AlignHCenter
                width: root.isMobile ? 48 : 40
            }

            // ── Zoom in [+] ───────────────────────────────────────────
            Rectangle {
                width: btnSize; height: btnSize; radius: 5
                color: zoomInMa.containsMouse ? Qt.alpha(ThemeEngine.cyan, 0.20)
                                              : Qt.alpha(ThemeEngine.cyan, 0.08)
                Label {
                    anchors.centerIn: parent
                    text: "+"
                    font.family: ThemeEngine.monoFont
                    font.pixelSize: root.isMobile ? 16 : 13
                    font.weight: Font.Bold
                    color: ThemeEngine.textPrimary
                }
                MouseArea {
                    id: zoomInMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: root.zoomIn()
                }
            }

            // ── Reset [1:1] ───────────────────────────────────────────
            Rectangle {
                width: btnSize; height: btnSize; radius: 5
                color: zoomResetMa.containsMouse ? Qt.alpha(ThemeEngine.cyan, 0.20)
                                                 : "transparent"
                Label {
                    anchors.centerIn: parent
                    text: "1:1"
                    font.family: ThemeEngine.monoFont
                    font.pixelSize: root.isMobile ? 11 : 9
                    font.weight: Font.Bold
                    color: ThemeEngine.textSecondary
                }
                MouseArea {
                    id: zoomResetMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: root.zoomReset()
                }
            }
        }
    }
}
