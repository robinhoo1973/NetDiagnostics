import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── Flutter TestResultItem 1:1 — 2-line collapsed + pending + detail click ──
Item {
    id: root
    property var itemData: ({})          // from allTestsForGroup entry
    property var detailResult: ({})      // from getDetailResult (on click)
    property var appState: null           // injected by parent

    implicitHeight: contentLayout.implicitHeight + 6
    signal detailClicked(var data)

    // ── Pending item (Flutter _buildPendingItem) ──────────────────────
    RowLayout {
        id: pendingRow
        anchors { fill: parent; leftMargin: 4; rightMargin: 4 }
        visible: itemData.isPending
        spacing: 8
        // Spinner icon (⟳ rotates) or gray dot (⊖)
        Label {
            text: itemData.isRunning ? "⟳" : "⊖"
            font.pixelSize: 12
            color: itemData.isRunning ? "#00BCD4" : "#555555"
            RotationAnimation on rotation {
                running: itemData.isRunning; from: 0; to: 360; duration: 1000; loops: Animation.Infinite
            }
        }
        Label {
            text: itemData.displayName || ("#" + itemData.testId)
            font.family: "JetBrains Mono"; font.pixelSize: 12
            color: "#666666"
            Layout.fillWidth: true; elide: Text.ElideRight
        }
        Label {
            visible: itemData.isRunning
            text: "Running..."; font.family: "JetBrains Mono"; font.pixelSize: 10
            font.italic: true; color: "#00BCD4"
        }
    }

    // ── Completed item (Flutter _buildResultItem) ─────────────────────
    ColumnLayout {
        id: contentLayout
        anchors { fill: parent; leftMargin: 4; rightMargin: 4 }
        visible: !itemData.isPending
        spacing: 1

        // Main row: status icon + name + duration (Flutter: InkWell → onTap _showDetailDialog)
        RowLayout {
            spacing: 8
            // Status icon: ✓ Pass, ✗ Fail, ⚠ Warning, ⊖ Skipped, ⓘ Info
            Label {
                text: {
                    var s = itemData.status
                    if (s === 0) return "✓"; if (s === 2) return "✗";
                    if (s === 1) return "⚠"; if (s === 3) return "⊖"; return "●"
                }
                font.pixelSize: 12
                color: {
                    var s = itemData.status
                    if (s === 0) return "#4ADE80"; if (s === 2) return "#EF4444";
                    if (s === 1) return "#FACC15"; if (s === 3) return "#888888"; return "#888888"
                }
            }
            // Name (Flutter: fontSize 12 w500, color from status)
            Label {
                text: itemData.displayName || ("#" + itemData.testId)
                font.family: "JetBrains Mono"; font.pixelSize: 12; font.weight: Font.Medium
                color: {
                    var s = itemData.status
                    return (s === 0) ? "#E0E0E0" : (s === 2 ? "#EF4444" : (s === 1 ? "#FACC15" : "#A0A0B8"))
                }
                Layout.fillWidth: true; elide: Text.ElideRight
            }
            // Duration badge — matches Flutter container badge
            Rectangle {
                visible: (itemData.durationMs || 0) > 0
                implicitWidth: durLabel.implicitWidth + 12; implicitHeight: 20; radius: 4
                color: "#2A2A4A"
                Label {
                    id: durLabel
                    anchors.centerIn: parent
                    text: _fmtDur(itemData.durationMs || 0)
                    font.family: "JetBrains Mono"; font.pixelSize: 10; color: "#A0A0B8"
                }
            }
        }

        // Summary line (Flutter: fontSize 10, textSecondary, maxLines 2)
        Label {
            visible: itemData.summary || ""
            text: itemData.summary || ""; font.family: "JetBrains Mono"
            font.pixelSize: 10; color: "#A0A0B8"
            Layout.fillWidth: true; elide: Text.ElideRight; maximumLineCount: 2
        }

        // Inline properties (Flutter: first 3 properties in a Wrap)
        Repeater {
            model: (itemData.properties || []).slice(0, 3)
            delegate: RowLayout {
                Layout.leftMargin: 24
                spacing: 4
                Label {
                    text: modelData.label ? (modelData.label + ":") : ""
                    font.family: "JetBrains Mono"; font.pixelSize: 9; font.weight: Font.Medium; color: "#606080"
                }
                Label {
                    text: modelData.value || ""; font.family: "JetBrains Mono"; font.pixelSize: 9; color: "#A0A0B8"
                    Layout.fillWidth: true; elide: Text.ElideRight
                }
            }
        }
    }

    // Click area covers the entire item
    MouseArea {
        anchors.fill: parent
        enabled: !itemData.isPending
        onClicked: root.detailClicked(itemData)
    }

    // ── Helpers ───────────────────────────────────────────────────────
    function _fmtDur(ms) {
        if (ms < 1000) return ms + "ms"
        if (ms < 60000) return (ms/1000).toFixed(1) + "s"
        var m = Math.floor(ms/60000); var s = Math.floor((ms%60000)/1000)
        return m + "m" + s + "s"
    }
}
