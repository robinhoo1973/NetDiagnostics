import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    title: "NetDiagnostics"
    visible: true
    flags: Qt.FramelessWindowHint
    minimumWidth: 360; minimumHeight: 400
    color: Theme.bgDark

    Component.onCompleted: {
        // Defer screen geometry read until the window is mapped to a screen
        // (root.screen is null before the window is shown).
        Qt.callLater(function() {
            var scr = root.screen
            if (!scr) return  // guard: window not yet assigned to a screen

            // Use availableSize for the screen the window lives on.
            // DO NOT use desktopAvailableWidth/Height — those span the
            // entire virtual desktop across all monitors, causing the
            // window to overshoot the current screen on multi-monitor
            // setups (the root cause of the "oversize window" bug).
            var as = scr.availableSize
            var sw = as.width
            var sh = as.height

            // 90% of screen, clamped to never exceed the screen bounds.
            width  = Math.min(sw * 0.9, sw)
            height = Math.min(sh * 0.9, sh)

            // Center on the current screen (use availableGeometry origin
            // so the window aligns to the correct monitor on multi-head).
            var ag = scr.availableGeometry
            x = ag.x + (sw - width)  / 2
            y = ag.y + (sh - height) / 2
        })
    }

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