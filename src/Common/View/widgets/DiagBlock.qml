import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts

// ── DiagBlock.qml — Square diagnostic block card (Living Diagnostics L3) ──
// Replaces the 32px-list-row DiagResultItem with a square card that shows a
// large colored icon, test name, and a single key metric.  4 visual states:
//   Pending  → static gray, no icon
//   Running  → colored icon + jiggle animation (L4), primary border pulse
//   Done     → status-color tint + settle pop (scale 0.9→1.0 OutBack 300ms)
//   Disabled → dimmed, not interactive
//
// Hover (desktop): subtle elevation — border glow + scale 1.02.  Tap/click
// opens the L5 detail page via the clicked() signal (D13: hybrid card feel).

Item {
    id: root

    // ── Public API ───────────────────────────────────────────────────────
    property var itemData: ({})
    property bool testRunning: false
    // 5WHY (UI redesign 2026-08-10): fixed compact tile (was derived
    // backwards from column count → 620px on wide desktops).  108px keeps a
    // dense tile wall; icon scales with the tile (44%).
    property real blockSize: 108
    // 5WHY (review B7 / doc D11): compact mode for summary views (Dashboard)
    // — smaller icon well, tighter margins, metric label hidden.  The full
    // size stays on the Diagnostic screen; Dashboard reuses the same block.
    property bool compact: false

    signal clicked(var data)

    // 5WHY: the old DiagResultItem hid skipped tests (status=3) — they
    // provide no actionable information.  Keep the visibility gate in
    // DiagBlock so the Flow grid isn't cluttered with dimmed skip cards.
    visible: _isPending || (itemData.status !== 3)
    implicitWidth: visible ? blockSize : 0
    implicitHeight: visible ? blockSize : 0

    // ── Derived state ────────────────────────────────────────────────────
    readonly property bool _isPending: itemData.isPending === true
    readonly property bool _isDisabled: itemData.isDisabled === true
    readonly property bool _isDone: !itemData.isPending && (itemData.status || 0) >= 0
    readonly property int _status: itemData.status !== undefined ? itemData.status : -1
    readonly property string _statusIcon: _isDone ? (ThemeEngine.statusIconNames[_status] || "badge-skip") : ""
    readonly property color _statusColor: _isDone ? (ThemeEngine.statusColors[_status] || ThemeEngine.colors.skipGray) : "transparent"
    // 5WHY: _isRunning was inside card Rectangle (line 184) — all other state
    // properties (_isPending, _isDisabled, _isDone) are at root level.
    // Move here for consistency and to reduce scope-chain fragility.
    readonly property bool _isRunning: root.testRunning && !root._isDisabled
    // 5WHY (review B2): Done-settle pop state.  Lives at root so the card
    // scale binding AND the in-card settle timers share one source.
    property bool _settle: false
    // ── Key metric extraction ────────────────────────────────────────────
    // Each diagnostic provides a different "headline" number from r.data.
    // Fallback: durationMs.
    readonly property string _keyMetric: {
        if (!_isDone) return ""
        var d = itemData.data
        if (!d) return ""
        // Ping / latency: show avg RTT
        if (d.rttAvgMs !== undefined) return d.rttAvgMs.toFixed(0) + "ms"
        // Traceroute: show hop count
        if (d.hopCount !== undefined) return d.hopCount + " hops"
        // HTTP: show total time
        if (d.totalMs !== undefined) return d.totalMs + "ms"
        // Certificate: show days left
        if (d.daysLeft !== undefined) return d.daysLeft + "d"
        // Connections: show count
        if (d.connectedCount !== undefined) return d.connectedCount + "/" + (d.totalPorts || "?")
        // Generic: use duration
        var dur = itemData.durationMs || 0
        if (dur > 0) return ThemeEngine.formatDuration(dur)
        return ""
    }

    // ── Visual ───────────────────────────────────────────────────────────
    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: 4
        radius: ThemeEngine.radius.lg  // 12
        color: _isDisabled ? Qt.alpha(ThemeEngine.colors.card, 0.4)
               : _isPending  ? Qt.alpha(ThemeEngine.colors.card, 0.6)
               : ThemeEngine.colors.card

        border {
            width: _isRunning ? 1.5 : 1
            color: _isRunning ? Qt.alpha(ThemeEngine.colors.primary, 0.6)
                   : _isDone   ? Qt.alpha(_statusColor, 0.3)
                   : ThemeEngine.colors.borderCard
        }

        // ── Scale behavior: settle pop on Done + hover lift ──────────────
        // 5WHY (review B2, 2026-08-10): the settleTimer previously assigned
        // card.scale = 0.9 directly — a JS assignment REMOVES the QML
        // binding on card.scale, permanently killing the hover-lift
        // expression.  Drive the pop through a _settle flag that participates
        // IN the binding instead; Behavior animates the 0.9→1.0 return.
        scale: root._settle ? 0.9
               : (mouseArea.containsMouse && !_isPending && !_isDisabled ? 1.03 : 1.0)
        Behavior on scale {
            NumberAnimation {
                duration: _isDone && card.scale === 1.0 ? 300 : 150
                easing.type: _isDone ? Easing.OutBack : Easing.OutQuad
                easing.overshoot: _isDone ? 0.3 : 0
            }
        }

        // ── Hover glow (desktop) ─────────────────────────────────────────
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: "transparent"
            border {
                width: 1.5
                color: mouseArea.containsMouse && !_isPending && !_isDisabled
                       ? Qt.alpha(ThemeEngine.colors.primary, 0.35) : "transparent"
            }
            Behavior on border.color { ColorAnimation { duration: 150 } }
        }

        // ── Content — icon-centered tile (UI redesign 2026-08-10) ────────
        // Best practice (iOS home-screen tiles): the semantic icon sits at
        // the EXACT geometric center of the tile; the status badge floats at
        // the tile's top-RIGHT corner; name + metric pin to the bottom edge.
        // This keeps the icon the unambiguous hero of each tile.

        // ── Status badge — top-right corner of the TILE (not the icon) ───
        AppIcon {
            anchors { top: parent.top; right: parent.right; topMargin: 5; rightMargin: 5 }
            name: _statusIcon
            size: 14
            color: _statusColor
            visible: _isDone
        }

        // ── Semantic icon — dead-center of the tile ──────────────────────
        AppIcon {
            id: blockIcon
            anchors.centerIn: parent
            name: appState.diagIconName(itemData.diagId) || "circle"
            // 42% of tile ≈ 45px on a 108px tile — centered hero
            size: Math.max(24, Math.round(root.blockSize * 0.42))
            color: _isRunning ? ThemeEngine.colors.primary
                   : _isDone  ? _statusColor
                   : ThemeEngine.colors.textMuted
            opacity: _isDisabled ? 0.3 : _isPending ? 0.4 : 1.0
        }

        // ── L4 Animation engine — centered on the icon ───────────────────
        // 5WHY (review B3, 2026-08-10): `itemData.diagId || -1` coerced
        // diagId=0 (G1NetworkAdapters, first enum value) to -1, so the app's
        // first test got the default Jiggle instead of its Pulse animation.
        // Strict undefined check (same fix as DetailPage).
        DiagAnimator {
            id: blockAnim
            anchors.centerIn: parent
            // Animations draw their own glyphs (bounce dot, hop nodes, …)
            // in a square slightly larger than the icon.
            width: Math.max(30, Math.round(root.blockSize * 0.56))
            height: width
            diagId: itemData.diagId !== undefined ? itemData.diagId : -1
            running: _isRunning
            // 5WHY (review B5): rotate the ICON itself for the iOS-style
            // busy state (JiggleAnimation) — not a separate ring.
            targetItem: blockIcon
        }

        // ── Test name — pinned to bottom, quiet single line ─────────────
        AppLabel {
            id: nameLabel
            anchors {
                left: parent.left; right: parent.right
                bottom: metricLine.visible ? metricLine.top : parent.bottom
                bottomMargin: metricLine.visible ? 0 : 4
            }
            horizontalAlignment: Text.AlignHCenter
            text: T.diagName(itemData.diagId) || itemData.displayName || ("#" + itemData.diagId)
            font.family: ThemeEngine.monoFont
            font.pixelSize: 10
            font.weight: Font.Normal
            // Muted footer: name never competes with the centered icon
            color: _isDisabled || _isPending ? ThemeEngine.colors.textMuted
                   : _status === 2 ? ThemeEngine.colors.failRed
                   : ThemeEngine.colors.textSecondary
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        // ── Metric sub-line — bottom edge, thin (running = "···") ───────
        Label {
            id: metricLine
            anchors {
                left: parent.left; right: parent.right
                bottom: parent.bottom; bottomMargin: 4
            }
            horizontalAlignment: Text.AlignHCenter
            text: _isRunning ? "···" : _isPending ? "" : _keyMetric
            font.family: ThemeEngine.monoFont
            font.pixelSize: 11
            font.weight: Font.Medium
            color: _isRunning ? ThemeEngine.colors.primary : ThemeEngine.colors.textMuted
            elide: Text.ElideRight
            visible: text !== "" && !root.compact
        }

        // ── Done settle animation trigger ─────────────────────────────────
        // When _isDone transitions to true, briefly raise root._settle so the
        // scale binding dips to 0.9; the Behavior animates back to the
        // hover/normal scale with OutBack pop (binding stays intact).
        property bool _settlePlayed: false

        Timer {
            id: settleTimer
            interval: 16  // next frame — let the layout render first
            repeat: false
            onTriggered: {
                root._settle = true
                releaseTimer.start()
            }
        }
        Timer {
            id: releaseTimer
            interval: 300  // match the OutBack settle duration
            repeat: false
            onTriggered: root._settle = false
        }
    }

    // Called when Done state is first reached
    on_IsDoneChanged: {
        if (_isDone && !card._settlePlayed) {
            card._settlePlayed = true
            settleTimer.start()
        }
    }

    // ── Interaction ──────────────────────────────────────────────────────
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: !_isPending && !_isDisabled
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        hoverEnabled: true
        onClicked: root.clicked(itemData)
    }
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Space)
            && !_isPending && !_isDisabled) {
            root.clicked(itemData)
            event.accepted = true
        }
    }
    Accessible.name: T.diagName(itemData.diagId) || itemData.displayName || ""
    Accessible.role: Accessible.Button
}
