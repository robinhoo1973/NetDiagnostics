import QtQuick
import NetDiagnostics.App 1.0

// ── DiagAnimator.qml — Living Diagnostics L4 animation dispatcher ──────
// Delegates `diagId → QRC animation URL` to C++ (AppState.diagAnimationUrl)
// so the enum→URL mapping lives in one place.  Uses Loader + source URL for
// platform-safe deferred loading (5WHY #4: inline Component → iOS crash).
//
// Usage: DiagAnimator { anchors.fill: parent; diagId: itemData.diagId; running: testRunning }

Item {
    id: root
    property int diagId: -1
    property bool running: false
    // 5WHY (review B5): lets JiggleAnimation rotate/scale the icon WELL
    // itself (real iOS-style icon jiggle) instead of a detached faint ring.
    // DiagBlock passes its icon-well Item; other animations ignore it.
    property var targetItem: null

    Loader {
        anchors.fill: parent
        active: root.running && root.diagId >= 0
        // C++ resolves DiagId → animation URL — no QML-side switch needed
        source: root.running && root.diagId >= 0
                ? AppState.diagAnimationUrl(root.diagId)
                : ""
        onLoaded: {
            if (item) {
                item.running = Qt.binding(function() { return root.running })
                item.targetItem = Qt.binding(function() { return root.targetItem })
            }
        }
        // 5WHY: onLoaded one-frame race — item.running is set AFTER
        // Component.onCompleted of the animation fires. If the animation
        // checks its own 'running' in onCompleted, it sees false and skips
        // init. Mitigation: animation components check running reactively
        // (via binding or onRunningChanged), not in onCompleted.
    }
}
