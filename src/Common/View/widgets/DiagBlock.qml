import QtQuick
import "../theme"
import "../theme/AnimationTokens.js" as Tokens
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
    readonly property string _keyMetric: {
        if (!_isDone) return ""
        var d = itemData.data; if (!d) return ""
        if (d.rttAvgMs !== undefined)  return Number(d.rttAvgMs).toFixed(0) + "ms"
        if (d.hopCount !== undefined)  return String(d.hopCount) + " hops"
        if (d.totalMs !== undefined)   return Number(d.totalMs).toFixed(0) + "ms"
        if (d.daysLeft !== undefined)  return String(d.daysLeft) + "d"
        if (d.connectedCount !== undefined) return d.connectedCount + "/" + (d.totalPorts || "?")
        return ""
    }

    // ── Elapsed-time coin indicator (top-left, Running + grace period) ──
    // Denominations: 50 / 20 / 10 / 5 / 1 — coin-change to minimize dots.
    // Each dot is a colored circle with the value printed inside.
    // Shows after elapsed >= 2s.  Persists for 2s grace period after test
    // completes so the user can read the final elapsed time.
    property int _elapsed: 0
    property bool _showCoins: false
    Timer {
        id: elapsedTimer
        interval: 1000; repeat: true
        running: _isRunning
        onTriggered: {
            root._elapsed++
            if (root._elapsed >= 2 && !root._showCoins)
                root._showCoins = true
        }
        onRunningChanged: {
            if (!running && root._elapsed >= 2) {
                // Keep coins visible for 2s grace period after completion
                graceTimer.start()
            } else if (!running && root._elapsed < 2) {
                root._elapsed = 0
                root._showCoins = false
            }
        }
    }
    Timer {
        id: graceTimer
        interval: 2000; repeat: false
        onTriggered: {
            root._elapsed = 0
            root._showCoins = false
        }
    }
    // Coin-change: greedy on [50,20,10,5,1] — always optimal for canonical coin systems
    readonly property var _coins: {
        var secs = Math.max(1, _elapsed)  // <1s → 1s
        var result = []
        var denoms = [50, 20, 10, 5, 1]
        for (var i = 0; i < denoms.length; i++) {
            var d = denoms[i]
            while (secs >= d) { result.push(d); secs -= d }
        }
        return result
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

        // ── Elapsed-time coin dots — top-left corner ────────────────────
        // Coin-change display: fewest dots, value printed inside each.
        // Denominations: 50(rose) 20(amber) 10(purple) 5(cyan) 1(slate)
        Row {
            anchors { top: parent.top; left: parent.left; topMargin: 4; leftMargin: 4 }
            spacing: 2
            visible: _showCoins
            Repeater {
                model: _coins
                Rectangle {
                    required property int modelData
                    readonly property int _denom: modelData
                    readonly property color _dotColor: {
                        switch (_denom) {
                            case 50: return "#FB7185"  // rose
                            case 20: return "#F59E0B"  // amber
                            case 10: return "#818CF8"  // purple
                            case 5:  return "#60C8F8"  // cyan
                            default: return "#64748B"  // slate
                        }
                    }
                    readonly property int _dotSize: _denom >= 10 ? 16 : 14
                    width: _dotSize; height: _dotSize; radius: _dotSize / 2
                    color: _dotColor
                    Label {
                        anchors.centerIn: parent
                        text: String(_denom)
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: _denom >= 10 ? 8 : 9
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                    }
                }
            }
        }

        // ── Status badge — top-right corner ───────────────────────────────
        AppIcon {
            anchors { top: parent.top; right: parent.right; topMargin: 5; rightMargin: 5 }
            name: _statusIcon; size: 14; color: _statusColor
            visible: _isDone
        }

        // ── Icon area — centered with slight upward offset ────────────────
        // 5WHY: geometric center (anchors.centerIn) looks off-center when
        // bottom text pushes visual weight downward.  Offset -6px compensates.
        Item {
            id: iconWell
            anchors {
                horizontalCenter: parent.horizontalCenter
                verticalCenter: parent.verticalCenter
                verticalCenterOffset: -6
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
            AppIcon {
                id: blockIcon
                anchors.centerIn: parent
                name: appState.diagIconName(itemData.diagId) || "circle"
                size: Math.max(22, Math.round(parent.width * 0.75))
                color: _isRunning ? ThemeEngine.colors.primary
                       : _isDone  ? _statusColor
                       : ThemeEngine.colors.textMuted
                opacity: _isDisabled ? 0.3 : _isPending ? 0.55 : 1.0
                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
        }

        // ── Test name — bottom edge ───────────────────────────────────────
        AppLabel {
            id: nameLabel
            anchors {
                left: parent.left; right: parent.right
                leftMargin: 4; rightMargin: 4
                bottom: metricLine.visible ? metricLine.top : parent.bottom
                bottomMargin: metricLine.visible ? 0 : 6
            }
            horizontalAlignment: Text.AlignHCenter
            text: T.diagName(itemData.diagId) || itemData.displayName || ("#" + itemData.diagId)
            font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Normal
            color: _isDisabled || _isPending ? ThemeEngine.colors.textMuted
                   : _status === 2 ? ThemeEngine.colors.failRed
                   : ThemeEngine.colors.textSecondary
            elide: Text.ElideRight; maximumLineCount: 1
        }

        // ── Metric line — bottom ──────────────────────────────────────────
        Label {
            id: metricLine
            anchors {
                left: parent.left; right: parent.right
                bottom: parent.bottom; bottomMargin: 5
            }
            horizontalAlignment: Text.AlignHCenter
            text: _isRunning ? "" : _isPending ? "" : _keyMetric
            font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.Medium
            color: _statusColor
            elide: Text.ElideRight
            visible: text !== "" && !root.compact
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
