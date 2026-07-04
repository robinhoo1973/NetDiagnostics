import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    title: "NetDiagnostics"
    visible: true
    flags: Qt.FramelessWindowHint
    color: Theme.bgDark

    // Maximize to fill the current screen's available area (respects
    // taskbar/dock).  Handles multi-monitor and screen changes natively —
    // no manual geometry math needed.
    visibility: Window.Maximized

    // ── Monospace font — loaded once at root, inherited by all child Labels ──
    // Setting font.family on the Window propagates to every Item/Label in the
    // QML tree. Individual Labels that set their own font.family override it.
    // DejaVu Sans Mono first — it has 128 box-drawing glyphs + symbols (✓ ✗ ⚠)
    // that JetBrains Mono lacks. Without DejaVu, Qt 6.8's contextFontMerging
    // (default on) pulls glyphs from proportional system fonts → breaks alignment.
    font.family: "DejaVu Sans Mono, JetBrains Mono, monospace"

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
        compact: Qt.platform.os === "ios" || Qt.platform.os === "android"
        onCloseRequested: root.close()
    }
}