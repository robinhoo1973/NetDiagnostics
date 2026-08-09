import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts

// ── TestResultItem — collapsed row + detailClicked signal ──────────────
Item {
    id: root
    property var itemData: ({})
    // 5WHY: spinner must NOT read itemData.isRunning — itemData is a JS object
    // snapshot from appState.allDiagsForGroup(); the QML binding engine cannot
    // track changes to a JS object's properties, so isRunning was frozen at
    // delegate-creation time. On iOS (different reload timing) the row showed
    // a static spinner that never spun. testRunning is a REACTIVE property
    // fed from DiagGroupPanel.isRunning (bound to appState.runStatus), so the
    // spinner appears and rotates the moment the group starts running.
    property bool testRunning: false
    // Hide skipped tests — they provide no actionable information.
    // Pending items (status == -1) are always visible.
    visible: itemData.isPending || (itemData.status !== 3)
    implicitHeight: visible ? 32 : 0
    signal detailClicked(var data)

    // 5WHY: Switched from static property var snapshot to dynamic
    // switch functions — static snapshots don't update when
    // ThemeEngine.applyTheme() changes palette colors.
    // However, JS functions called from QML bindings mask dependency
    // tracking: the QML binding engine cannot trace into JavaScript
    // function bodies, so color: _statusColor(itemData.status) only
    // re-evaluates when itemData.status changes — NEVER when the
    // user switches themes.  The icon name stays correct but the
    // color is stale.
    //
    // Fix: _statusColors is a property var array whose binding
    // EXPRESSION directly references ThemeEngine.colors.xxx — QML
    // CAN track these dependencies, so the entire array is rebuilt
    // on every theme switch.  The color binding then reads
    // _statusColors[status], which depends on both the array AND
    // the index, guaranteeing re-evaluation on theme switches.
    //
    // _statusIcon is a JS function (not a tracked property) because
    // icon names are static — they do not depend on theme colors.
    // 5WHY: Both _statusColors and _statusIcon() were replaced by
    // centralized ThemeEngine.statusColors[] and statusIconNames[].
    // Adding a 7th status now only needs one edit site (ThemeEngine.qml).

    // ── Pending item ──────────────────────────────────────────────────
    RowLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        visible: root.itemData.isPending; spacing: 8
        // 5WHY (2026-08-09): per-test semantic icon (muted, 14px — smaller
        // than the 16px status glyph so status stays the visual anchor).
        AppIcon {
            name: appState.diagIconName(itemData.diagId)
            size: 14
            color: ThemeEngine.colors.textMuted
        }
        AppIcon {
            id: pendingSpinner
            name: root.testRunning ? "spinner" : "badge-skip"; size: 16
            color: root.testRunning ? ThemeEngine.colors.primary : ThemeEngine.colors.textMuted
            RotationAnimation on rotation {
                running: root.testRunning; from:0; to:360; duration:1000; loops:Animation.Infinite
                // 5WHY: Reset rotation when spinner stops so badge-skip icon isn't skewed.
                onStopped: pendingSpinner.rotation = 0
            }
        }
        // 5WHY: itemData.displayName comes from C++ ::diagDisplayName()
        // which is always English. Route through T.diagName() so
        // test names follow the active language. diagName() always
        // returns a non-empty string for valid diagIds (0-45) — the
        // t() function provides English fallback for any language.
        // The || fallback is a safety net for out-of-range IDs only.
        AppLabel {
            text: T.diagName(itemData.diagId) || itemData.displayName || ("#" + itemData.diagId)
            font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.colors.textSecondary
            Layout.fillWidth: true; elide: T.textElideStart
        }
        Label {
            visible: root.testRunning; text: T.tr("diagRunning")
            font.family:ThemeEngine.monoFont; font.pixelSize:11; font.italic:true; color:ThemeEngine.colors.primary
        }
    }

    // ── Completed row ─────────────────────────────────────────────────
    RowLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        visible: !itemData.isPending; spacing: 8
        // 5WHY (2026-08-09): per-test semantic icon (14px, muted) before the
        // status glyph so each row is identifiable at a glance.
        AppIcon {
            name: appState.diagIconName(itemData.diagId)
            size: 14
            color: ThemeEngine.colors.textMuted
        }
        // 5WHY: Error(4) showed infoBlue (same as Info), not visually distinct.
        // Now Error→errorRed, Info(5)→infoBlue, Skipped(3)→skipGray.
        // Icon size 12 → 16 per M3 iconSm — doubles the visible area (64→256 px²)
        // for significantly better status recognition at a glance.
        // 5WHY: color: _statusColors[status] uses the tracked property var
        // array above — QML can detect when ThemeEngine.colors changes
        // and re-evaluate the binding, fixing theme-switch color updates.
        AppIcon {
            name: ThemeEngine.statusIconNames[itemData.status] || "badge-skip"; size: 16
            color: ThemeEngine.statusColors[itemData.status] || ThemeEngine.colors.skipGray
        }
        // 5WHY: Same translation issue as the pending row — displayName
        // is always English from C++. Use T.diagName() for i18n.
        AppLabel {
            text: T.diagName(itemData.diagId) || itemData.displayName || ("#" + itemData.diagId)
            font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.Medium
            color: { var s=itemData.status; return s===0?ThemeEngine.colors.textPrimary:(s===2?ThemeEngine.colors.failRed:ThemeEngine.colors.textSecondary) }
            Layout.fillWidth: true; elide: T.textElideStart
        }
        Rectangle {
            visible: (itemData.durationMs||0)>0; implicitWidth:durText.implicitWidth+12; implicitHeight:20; radius:4
            color: ThemeEngine.colors.borderCard
            Label { id:durText; anchors.centerIn:parent; text:ThemeEngine.formatDuration(itemData.durationMs||0); font.family:ThemeEngine.monoFont; font.pixelSize:11; color:ThemeEngine.colors.textSecondary }
        }
    }

    // 5WHY: had no keyboard access or screen-reader label — keyboard
    // users could not view test result details (WCAG 2.1 SC 2.1.1).
    MouseArea {
        anchors.fill: parent
        enabled: !itemData.isPending
        cursorShape: Qt.PointingHandCursor
        onClicked: root.detailClicked(itemData)
    }
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Space) && !itemData.isPending) {
            root.detailClicked(itemData)
            event.accepted = true
        }
    }
    Accessible.name: T.diagName(itemData.diagId) || itemData.displayName || (T.tr("testIdPrefix") + itemData.diagId)
    Accessible.role: Accessible.Button
}
