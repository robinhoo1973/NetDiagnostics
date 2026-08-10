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
    property real blockSize: 130  // set by parent grid

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
        scale: mouseArea.containsMouse && !_isPending && !_isDisabled ? 1.03 : 1.0
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

        // ── Content ──────────────────────────────────────────────────────
        ColumnLayout {
            anchors {
                fill: parent
                margins: 10
            }
            spacing: 6

            // ── Icon well ────────────────────────────────────────────────
            Item {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
                Layout.topMargin: 4

                // Semantic icon (large, colored)
                AppIcon {
                    id: blockIcon
                    anchors.centerIn: parent
                    name: appState.diagIconName(itemData.diagId) || "circle"
                    size: 48
                    color: _isRunning ? ThemeEngine.colors.primary
                           : _isDone  ? _statusColor
                           : ThemeEngine.colors.textMuted
                    opacity: _isDisabled ? 0.3 : _isPending ? 0.4 : 1.0
                }

                // ── Status badge overlay (bottom-right of icon) ──────────
                AppIcon {
                    anchors { right: parent.right; bottom: parent.bottom; rightMargin: -2; bottomMargin: -2 }
                    name: _statusIcon
                    size: 18
                    color: _statusColor
                    visible: _isDone
                }

                // ── L4 Animation engine (replaces old spinner) ──────────
                DiagAnimator {
                    anchors.fill: parent
                    diagId: itemData.diagId || -1
                    running: _isRunning
                }
            }

            // ── Test name ─────────────────────────────────────────────────
            AppLabel {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: T.diagName(itemData.diagId) || itemData.displayName || ("#" + itemData.diagId)
                font.family: ThemeEngine.monoFont
                font.pixelSize: 11
                font.weight: Font.Medium
                color: _isDisabled ? ThemeEngine.colors.textMuted
                       : _isPending  ? ThemeEngine.colors.textMuted
                       : _status === 2 ? ThemeEngine.colors.failRed
                       : ThemeEngine.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            // ── Key metric ────────────────────────────────────────────────
            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: _isRunning ? "..." : _isPending ? "" : _keyMetric
                font.family: ThemeEngine.monoFont
                font.pixelSize: 14
                font.weight: Font.Bold
                color: _isRunning ? ThemeEngine.colors.primary : ThemeEngine.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                visible: text !== ""
            }
        }

        // ── Done settle animation trigger ─────────────────────────────────
        // When _isDone transitions to true, briefly set scale to 0.9 so
        // the Behavior on scale animates it back to 1.0 with OutBack pop.
        property bool _settlePlayed: false

        Timer {
            id: settleTimer
            interval: 16  // next frame — let the layout render first
            repeat: false
            onTriggered: { card.scale = 0.9 }
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
