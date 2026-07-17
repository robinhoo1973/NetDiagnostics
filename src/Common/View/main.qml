import QtQuick
import QtQuick.Controls
import "theme"
import "widgets"

ApplicationWindow {
    id: root
    title: "NetDiagnostics"
    visible: true
    flags: Qt.FramelessWindowHint
    color: ThemeEngine.bgDark

    // Window maximization is handled by C++ showMaximized() in main.cpp —
    // it maps the window in maximized state from the first frame, avoiding
    // the QML property-order race where visibility: Window.Maximized can be
    // silently ignored for frameless windows.

    // ── Monospace font — loaded once at root, inherited by all child Labels ──
    // Setting font.family on the Window propagates to every Item/Label in the
    // QML tree. Individual Labels that set their own font.family override it.
    // DejaVu Sans Mono first — it has 128 box-drawing glyphs + symbols (✓ ✗ ⚠)
    // that JetBrains Mono lacks. Without DejaVu, Qt 6.8's contextFontMerging
    // (default on) pulls glyphs from proportional system fonts → breaks alignment.
    font.family: "DejaVu Sans Mono"

    FontLoader {
        id: appFontRegular
        source: "qrc:/fonts/JetBrainsMono-Regular.ttf"
        // Direct onStatusChanged — avoids Connections document-order race
        onStatusChanged: {
            if (appFontRegular.status === FontLoader.Ready) {
                console.log("[font] JetBrains Mono loaded — family name:",
                            appFontRegular.name)
            } else if (appFontRegular.status === FontLoader.Error) {
                console.error("[font] FAILED to load JetBrainsMono-Regular.ttf!"
                    + " Check qrc:/fonts/ path and resources.qrc.")
            }
        }
    }
    FontLoader {
        id: appFontBold
        source: "qrc:/fonts/JetBrainsMono-Bold.ttf"
    }
    // System monospace with full box-drawing coverage (128 glyphs: ├─└│ etc.)
    FontLoader {
        id: dejavuMono
        source: "qrc:/fonts/DejaVuSansMono.ttf"
        onStatusChanged: {
            if (status === FontLoader.Ready)
                console.log("[FONT] DejaVu Sans Mono loaded, family name:", name)
            else if (status === FontLoader.Error)
                console.error("[FONT] FAILED to load DejaVu Sans Mono from", source)
        }
    }

    AppContent {
        anchors.fill: parent
        compact: ThemeEngine.isMobile
        onCloseRequested: root.close()
    }

    // ── Close button (top-right corner, theme-aware) ────────────────
    // Desktop only — mobile platforms use native window chrome / gesture close.
    // 5WHY: The close button was completely invisible on the dark background
    // (#0F172A). Users had to accidentally hover over the top-right corner to
    // discover it — WCAG 2.1 SC 1.4.1 (Use of Color) failure.  Now shows a
    // subtle border at rest and a visible 44x44px touch target (Apple HIG min).
    Rectangle {
        visible: !ThemeEngine.isMobile
        anchors { top: parent.top; right: parent.right; topMargin: 6; rightMargin: 10 }
        width: 44; height: 44; radius: 8
        color: closeArea.containsMouse ? Qt.alpha(ThemeEngine.colors.borderCard, 0.5) : "transparent"
        border { width: 1; color: closeArea.containsMouse ? ThemeEngine.colors.borderCard : Qt.alpha(ThemeEngine.colors.borderCard, 0.25) }
        AppIcon {
            anchors.centerIn: parent
            name: "close"; size: 16
            color: closeArea.containsMouse ? ThemeEngine.colors.textPrimary : ThemeEngine.colors.textSecondary
        }
        MouseArea {
            id: closeArea
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            onClicked: root.close()
        }
        Accessible.name: "Close window"
        Accessible.role: Accessible.Button
    }
}
