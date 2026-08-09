import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts

// ── Flutter TestGroupPanel 1:1 — no Loaders, direct visibility toggle ──
Rectangle {
    id: root
    property int groupIndex: 0
    property bool expanded: false
    property bool _userToggled: false
    // Phone portrait: badges move to a second line below the title bar
    // so the 5 status icons + counts don't get clipped in the title row.
    property bool compact: ThemeEngine.isMobile

    height: cardColumn.implicitHeight + 16
    radius: 10
    color: ThemeEngine.colors.card
    border {
        width: activeFocus ? 2 : 1
        color: activeFocus ? ThemeEngine.colors.borderFocused
              : isRunning ? Qt.alpha(ThemeEngine.colors.cyan, 0.4)
              : ThemeEngine.colors.borderCard
    }

    // ── Computed state — single C++ call, shared JS object (was 7 calls) ──
    property var _gstat: { var _v=_modelVersion; var s=appState.groupStats(groupIndex)
        return { total:s.total||0, completed:s.completed||0, pass:s.pass||0,
                 warn:s.warn||0, fail:s.fail||0, skip:s.skip||0, info:s.info||0 } }
    property int enabledCount: _gstat.total
    property int completedCount: _gstat.completed
    property bool isRunning: appState.runStatus===1 && completedCount<enabledCount
    property int groupPass: _gstat.pass
    property int groupWarn: _gstat.warn
    property int groupFail: _gstat.fail
    property int groupSkip: _gstat.skip
    property int groupInfo: _gstat.info

    onIsRunningChanged: if(!_userToggled)expanded=isRunning||completedCount>0
    onCompletedCountChanged: if(!_userToggled&&completedCount>0)expanded=true

    // Refresh the item model whenever a diagnostic completes. Driven by
    // appState.progressChanged (which also bumps resultsVersion). The previous
    // version targeted a non-existent `page._TotalCompleted` signal, so the
    // model never refreshed during a run.
    Connections {
        target: appState
        function onProgressChanged() { reloadModel() }
    }

    property int _modelVersion: 0
    property var itemsModel: []
    function reloadModel() {
        // 5WHY: itemsModel=[] then itemsModel=fresh caused Repeater delegate
        // churn — all children destroyed then recreated (ARM64 flicker).
        // Single assignment + version bump triggers one re-evaluation.
        itemsModel = appState.allDiagsForGroup(groupIndex)
        _modelVersion++
    }
    Component.onCompleted: reloadModel()

    ColumnLayout {
        id: cardColumn
        anchors { fill: parent; leftMargin: 12; rightMargin: 12; topMargin: 8; bottomMargin: 8 }
        spacing: 0

        // ── Header ────────────────────────────────────────────────────
        // Desktop: single row with badges inline.
        // Phone portrait: title + count on row 1, badges on row 2
        // so the 5 status icons + counts don't get clipped on narrow screens.
        ColumnLayout {
            spacing: 2

            // Row 1 — title + count + expand arrow
            RowLayout {
                spacing: 8
                Rectangle { width:3; height:24; radius:2; color:isRunning?ThemeEngine.colors.cyan:ThemeEngine.colors.infoBlue }
                ColumnLayout { spacing:1
                    AppLabel { Layout.fillWidth:true; text:T.groupPrefix(groupIndex)+": "+T.groupName(groupIndex); font.family:ThemeEngine.monoFont; font.pixelSize:13; font.weight:Font.DemiBold; color:ThemeEngine.colors.textPrimary; elide:T.textElideStart }
                    AppLabel {
                        visible: isRunning
                        text: T.tr("runningPrefix") + (appState.currentDiagLabel||"") + "..."
                        font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.italic: true
                        color: ThemeEngine.colors.cyan; elide: T.textElideStart
                        Accessible.name: T.tr("runningPrefix") + (appState.currentDiagLabel || "")
                        // 5WHY (yellow floating bar): ToolTip.visible/text here
                        // showed Qt's DEFAULT ToolTip chrome — a yellow
                        // native-looking bar that clashed with the app theme
                        // on every platform (iOS/Android/desktop) whenever the
                        // running group header was hovered.  The running label
                        // already shows the diag name inline, so the ToolTip
                        // only duplicated it.  Removed (all platforms).
                    }
                }
                Item { Layout.fillWidth:true }
                Label { visible:isRunning||completedCount>0; text:completedCount+"/"+enabledCount; font.family:ThemeEngine.monoFont; font.pixelSize:11; font.weight:Font.Medium; color:ThemeEngine.colors.textSecondary }
                // Badges inline — desktop only (wide enough to fit)
                RowLayout { spacing: 4; visible: !compact
                    StatusBadge { statusCode: 0; count: groupPass }
                    StatusBadge { statusCode: 5; count: groupInfo }
                    StatusBadge { statusCode: 1; count: groupWarn }
                    StatusBadge { statusCode: 2; count: groupFail }
                    StatusBadge { statusCode: 3; count: groupSkip }
                }
                // 5WHY: Replaced ▼/▶ Unicode arrows with chevron SVG icons
                // for consistent iconography across the app.
                // 5WHY: In RTL (Arabic) the collapsed chevron must point LEFT
                // along the reading direction.  AppIcon.mirror flips the glyph
                // horizontally; chevron-down (expanded) is vertical and
                // direction-neutral so it never needs mirroring.
                AppIcon {
                    name: expanded ? "chevron-down" : "chevron-right"
                    mirror: !expanded && T.isRtl
                    size: 14; color: ThemeEngine.colors.textSecondary
                }
            }

            // Row 2 — result badges on their own line (phone portrait only)
            RowLayout {
                spacing: 4; visible: compact
                // Indent to align with group name (accent bar 3px + spacing 8px = 11px)
                Item { width: 11 }
                StatusBadge { statusCode: 0; count: groupPass }
                StatusBadge { statusCode: 5; count: groupInfo }
                StatusBadge { statusCode: 1; count: groupWarn }
                StatusBadge { statusCode: 2; count: groupFail }
                StatusBadge { statusCode: 3; count: groupSkip }
            }
        }

        // ── Progress bar ──────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 4; Layout.topMargin: 6
            visible: isRunning || completedCount > 0
            radius: 2; color: ThemeEngine.colors.borderCard
            Rectangle {
                height: 4; radius: 2
                width: enabledCount>0 ? parent.width*(completedCount*1.0/enabledCount) : 0  // *1.0 forces float division (JS int math truncates)
                color: isRunning ? ThemeEngine.colors.cyan : ThemeEngine.colors.passGreen
            }
        }

        // ── Expanded body — Flutter ExpansionTile children ────────────
        ColumnLayout {
            Layout.fillWidth: true; Layout.topMargin: 6
            visible: expanded
            spacing: 0
            Rectangle { Layout.fillWidth:true; implicitHeight:1; color:ThemeEngine.colors.borderCard }
            Repeater {
                model: root.itemsModel
                delegate: Item {
                    Layout.fillWidth: true
                    // 5WHY: Must mirror testItem's own visible-based collapse
                    // (DiagResultItem.implicitHeight: visible ? 32 : 0) — otherwise
                    // a hidden (skipped) row still reserves 2px and renders a
                    // stray tree-connector tick even though nothing is visible.
                    implicitHeight: testItem.visible ? testItem.implicitHeight + 2 : 0
                    // TreeView connector: vertical line + horizontal stub
                    Rectangle {
                        anchors { top:parent.top; bottom:parent.bottom; left:parent.left; leftMargin:6 }
                        width:2; color:ThemeEngine.colors.borderCard
                    }
                    DiagResultItem {
                        id: testItem
                        anchors { left:parent.left; leftMargin:20; right:parent.right }
                        itemData: modelData
                        // Reactive running flag (bound to appState.runStatus)
                        // drives the per-row spinner — see DiagResultItem 5WHY.
                        testRunning: root.isRunning
                        onDetailClicked: function(data) { root.detailClicked(data) }
                    }
                }
            }
        }
    }

    // 5WHY: group header had no keyboard access or screen-reader label.
    // Keyboard-only users could not expand/collapse diagnostic groups.
    MouseArea {
        id: headerTapArea
        anchors { top:parent.top; left:parent.left; right:parent.right }
        height: 40
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: { _userToggled=true; expanded=!expanded }
    }
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            _userToggled = true; expanded = !expanded; event.accepted = true
        }
    }
    Accessible.name: T.groupName(groupIndex) + (expanded ? T.tr("accExpanded") : T.tr("accCollapsed"))
    Accessible.role: Accessible.Button

    signal detailClicked(var data)

    // StatusBadge → shared component (src/Common/View/widgets/StatusBadge.qml)
    // 5WHY: was inline — now imported from shared file, single source of truth.
}
