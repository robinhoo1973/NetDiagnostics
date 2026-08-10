import QtQuick
import "../theme"
import "../theme/AnimationTokens.js" as Tokens
import "../detail/KeyMetric.js" as KeyMetric
import QtQuick.Controls
import QtQuick.Layouts

// ── DiagBlock.qml — Premium diagnostic tile (Living Diagnostics L3 v2) ──
// Visual redesign (2026-08-10): layered card depth, subtle gradient accent,
// elapsed-time dot indicator replacing text duration, icon-centered layout.
//
// States: Pending(gray) → Running(jiggle+glow) → Done(status tint+settle pop)

Item {
    id: root

    // ── Public API ───────────────────────────────────────────────────────
    property var itemData: ({})
    property bool testRunning: false
    property real blockSize: 108
    property bool compact: false
    signal clicked(var data)

    // ── Derived state ────────────────────────────────────────────────────
    readonly property bool _isPending: itemData.isPending === true
    readonly property bool _isDisabled: itemData.isDisabled === true
    // Use C++ model's isDone field (single source of truth) instead of re-deriving
    // from status. Avoids (itemData.status || 0) falsy-zero pitfall where
    // undefined status incorrectly resolves to 0 (done).
    readonly property bool _isDone: itemData.isDone === true
    readonly property int _status: itemData.status !== undefined ? itemData.status : -1
    readonly property string _statusIcon: _isDone ? (ThemeEngine.statusIconNames[_status] || "badge-skip") : ""
    readonly property color _statusColor: _isDone ? (ThemeEngine.statusColors[_status] || ThemeEngine.colors.skipGray) : "transparent"
    readonly property bool _isRunning: root.testRunning && !root._isDisabled
    property bool _settle: false

    visible: _isPending || (itemData.status !== 3)
    implicitWidth: visible ? blockSize : 0
    implicitHeight: visible ? blockSize : 0

    // ── Key metric (headline number, no duration text) ────────────────────
    // 5WHY: was a SECOND independent implementation of DetailPage's metric
    // logic (different key set, different formatting) — the two drifted.
    // Both now consume the shared KeyMetric.js module (single source).
    readonly property string _keyMetric: {
        if (!_isDone) return ""
        var km = KeyMetric.keyMetric(itemData.data, itemData.durationMs || 0)
        if (!km.ok) return ""
        // 5WHY: dedup — precision + min:sec formatting is the shared
        // KeyMetric.formatNumber() (MetricCard uses the same helper).
        var val = KeyMetric.formatNumber(km.value, km.precision, km.format)
        return val + (km.unitKey ? " " + T.tr(km.unitKey) : "") + km.trailing
    }

    // ── Elapsed-time indicator — top-left: color-coded dot + seconds text ──
    // 5WHY: coin-change (multiple denomination dots) was visually noisy on a
    // 108×108 tile.  Replaced with a single dot colored by elapsed time plus
    // the actual seconds — industry standard for running-time indicators.
    // Dot persists after completion so the user can always read the elapsed time.
    property int _elapsed: 0
    property bool _showTimer: false
    Timer {
        id: elapsedTimer
        interval: 1000; repeat: true
        running: _isRunning
        onTriggered: {
            root._elapsed++
            if (!root._showTimer) root._showTimer = true
        }
        onRunningChanged: {
            if (running) {
                // Test (re)started — reset
                root._elapsed = 0
                root._showTimer = false
            } else if (root._elapsed > 0) {
                // Test completed — keep indicator visible
                root._showTimer = true
            }
        }
    }
    // Color thresholds: <5s green → 5-9s yellow → 10-20s orange → >20s red
    readonly property string _timerColor: {
        if (_elapsed > 20) return "#DC2626"       // red
        if (_elapsed >= 10) return "#EA580C"      // orange
        if (_elapsed >= 5)  return "#F59E0B"      // yellow
        return "#10B981"                           // green
    }

    // ── Premium card ─────────────────────────────────────────────────────
    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: 3
        radius: 16
        // Layered background: subtle gradient for depth
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: _isDone ? Qt.alpha(_statusColor, 0.08)
                       : _isRunning ? Qt.alpha(ThemeEngine.colors.primary, 0.06)
                       : ThemeEngine.colors.card
            }
            GradientStop {
                position: 1.0
                color: _isDone ? Qt.alpha(_statusColor, 0.04)
                       : ThemeEngine.colors.card
            }
        }
        // Inner shadow effect (simulated via layered rect — no ShaderEffect)
        Rectangle {
            anchors.fill: parent; radius: parent.radius
            color: "transparent"
            border { width: 1; color: Qt.alpha("#FFFFFF", 0.04) }
        }
        // Main border
        border {
            width: _isRunning ? 1.5 : 1
            color: _isRunning ? Qt.alpha(ThemeEngine.colors.primary, 0.5)
                   : _isDone   ? Qt.alpha(_statusColor, 0.25)
                   : Qt.alpha(ThemeEngine.colors.borderCard, 0.7)
        }

        // Scale: settle pop + hover lift
        scale: root._settle ? 0.9
               : (mouseArea.containsMouse && !_isPending && !_isDisabled ? 1.03 : 1.0)
        Behavior on scale {
            NumberAnimation {
                // settleDown: scale goes 1.0→0.9 (OutBack), settleUp: 0.9→1.0 (OutQuad)
                // Use root._settle instead of card.scale to avoid circular dependency
                duration: root._settle ? Tokens.tokens.settleDuration : Tokens.tokens.transitionDuration
                easing.type: root._settle ? Easing.OutBack : Easing.OutQuad
                easing.overshoot: root._settle ? 0.3 : 0
            }
        }

        // Hover glow layer
        Rectangle {
            anchors.fill: parent; radius: parent.radius
            color: "transparent"
            border {
                width: 1.5
                color: mouseArea.containsMouse && !_isPending && !_isDisabled
                       ? Qt.alpha(ThemeEngine.colors.primary, 0.3) : "transparent"
            }
            Behavior on border.color { ColorAnimation { duration: 150 } }
        }

        // ── Elapsed-time indicator — top-left corner ────────────────────
        // Color-coded dot (thresholds see _timerColor above) + seconds label.
        Row {
            z: 5
            anchors { top: parent.top; left: parent.left; topMargin: 4; leftMargin: 4 }
            spacing: 4
            visible: _showTimer
            Rectangle {
                width: 8; height: 8; radius: 4
                anchors.verticalCenter: parent.verticalCenter
                color: _timerColor
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: String(_elapsed) + "s"
                font.family: ThemeEngine.monoFont
                font.pixelSize: 10; font.weight: Font.DemiBold
                color: _timerColor
            }
        }

        // ── Status badge — top-right corner ───────────────────────────────
        AppIcon {
            z: 5
            anchors { top: parent.top; right: parent.right; topMargin: 5; rightMargin: 5 }
            name: _statusIcon; size: 14; color: _statusColor
            visible: _isDone
        }

        // ── Icon area — centered with slight upward offset ────────────────
        // 5WHY: geometric center (anchors.centerIn) looks off-center when
        // bottom text pushes visual weight downward.  The offset is now
        // PROPORTIONAL to blockSize (5%) instead of a magic -6px that did
        // not scale for small tiles.  Explicit z-index keeps the title and
        // metric readable above the animated icon — Jiggle can scale the
        // icon past its bounds during the running state, so title/metric
        // must stack above the icon well.
        Item {
            id: iconWell
            z: 1
            anchors {
                horizontalCenter: parent.horizontalCenter
                verticalCenter: parent.verticalCenter
                verticalCenterOffset: -Math.round(root.blockSize * 0.05)
            }
            width: Math.max(32, Math.round(root.blockSize * 0.48))
            height: width

            // Icon glow (running state)
            Rectangle {
                anchors.fill: parent; radius: width / 2
                color: _isRunning ? Qt.alpha(ThemeEngine.colors.primary, 0.10) : "transparent"
                Behavior on color { ColorAnimation { duration: 200 } }
            }

            // L4 Animation overlay — rendered BELOW the diagnostic icon so
            // animations (Pulse glow, Bounce dots, Lock stamp, etc.) never
            // obscure the test-type icon itself.
            DiagAnimator {
                id: blockAnim
                anchors.fill: parent
                diagId: itemData.diagId !== undefined ? itemData.diagId : -1
                running: _isRunning
                targetItem: blockIcon
            }

            // Diagnostic icon — dead center of icon well, TOPMOST layer
            // within the well (z:2); the well itself is z:1 under the title.
            AppIcon {
                id: blockIcon
                z: 2
                anchors.centerIn: parent
                name: appState.diagIconName(itemData.diagId) || "circle"
                size: Math.max(22, Math.round(parent.width * 0.75))
                // 5WHY: textMuted(#64748B) @ 0.55 on dark card → nearly invisible.
                // textSecondary(#94A3B8) @ 0.80 matches group-header icon contrast
                // while preserving pending-state visual hierarchy.
                color: _isRunning ? ThemeEngine.colors.primary
                       : _isDone  ? _statusColor
                       : ThemeEngine.colors.textSecondary
                opacity: _isDisabled ? 0.3 : _isPending ? 0.80 : 1.0
                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
        }

        // ── Test name — bottom edge ───────────────────────────────────────
        // 5WHY: metricLine removed — tiles are icon+name-only per L3 design
        // spec.  The top-left elapsed indicator already carries the timing
        // information, so the bottom metric text was redundant.
        AppLabel {
            id: nameLabel
            z: 3
            anchors {
                left: parent.left; right: parent.right
                leftMargin: 4; rightMargin: 4
                bottom: parent.bottom; bottomMargin: 6
            }
            horizontalAlignment: Text.AlignHCenter
            text: T.diagName(itemData.diagId) || itemData.displayName || ("#" + itemData.diagId)
            font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Normal
            color: _isDisabled || _isPending ? ThemeEngine.colors.textMuted
                   : _status === 2 ? ThemeEngine.colors.failRed
                   : ThemeEngine.colors.textSecondary
            elide: Text.ElideRight; maximumLineCount: 1
        }

        // ── Done settle animation ─────────────────────────────────────────
        property bool _settlePlayed: false
        Timer {
            id: settleTimer; interval: 16; repeat: false
            onTriggered: { root._settle = true; releaseTimer.start() }
        }
        Timer {
            id: releaseTimer
            interval: Tokens.tokens.settleDuration; repeat: false
            onTriggered: root._settle = false
        }
    }

    // ── Settle trigger + re-run reset ─────────────────────────────────────
    on_IsDoneChanged: {
        if (_isDone && !card._settlePlayed) {
            card._settlePlayed = true
            settleTimer.start()
        } else if (!_isDone) {
            card._settlePlayed = false  // 5WHY: reset so settle replays on re-run
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
            root.clicked(itemData); event.accepted = true
        }
    }
    Accessible.name: T.diagName(itemData.diagId) || itemData.displayName || ""
    Accessible.role: Accessible.Button
}
