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
    border { width: 1; color: isRunning ? Qt.alpha(ThemeEngine.cyan, 0.4) : ThemeEngine.colors.borderCard }

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
                Rectangle { width:3; height:24; radius:2; color:isRunning?ThemeEngine.cyan:ThemeEngine.infoBlue }
                ColumnLayout { spacing:1
                    Label { Layout.fillWidth:true; text:"G"+(groupIndex+1)+": "+(Tr.groupName(groupIndex)); font.family:ThemeEngine.monoFont; font.pixelSize:13; font.weight:Font.DemiBold; color:ThemeEngine.colors.textPrimary; elide:Text.ElideRight }
                    Label { visible:isRunning; text:"Running: "+(appState.currentDiagLabel||"")+"..."; font.family:ThemeEngine.monoFont; font.pixelSize:10; font.italic:true; color:ThemeEngine.cyan; elide:Text.ElideRight }
                }
                Item { Layout.fillWidth:true }
                Label { visible:isRunning||completedCount>0; text:completedCount+"/"+enabledCount; font.family:ThemeEngine.monoFont; font.pixelSize:11; font.weight:Font.Medium; color:ThemeEngine.colors.textSecondary }
                // Badges inline — desktop only (wide enough to fit)
                RowLayout { spacing: 4; visible: !compact
                    StatusBadge { accent: ThemeEngine.passGreen;  iconName: "badge-check";   count: groupPass }
                    StatusBadge { accent: ThemeEngine.infoBlue; iconName: "badge-info";    count: groupInfo }
                    StatusBadge { accent: ThemeEngine.warnYellow; iconName: "badge-warning"; count: groupWarn }
                    StatusBadge { accent: ThemeEngine.failRed;    iconName: "badge-close";   count: groupFail }
                    StatusBadge { accent: ThemeEngine.skipGray;   iconName: "badge-skip";    count: groupSkip }
                }
                Label { text:expanded?"▼":"▶"; font.pixelSize:10; color:ThemeEngine.colors.textSecondary }
            }

            // Row 2 — result badges on their own line (phone portrait only)
            RowLayout {
                spacing: 4; visible: compact
                // Indent to align with group name (accent bar 3px + spacing 8px = 11px)
                Item { width: 11 }
                StatusBadge { accent: ThemeEngine.passGreen;  iconName: "badge-check";   count: groupPass }
                StatusBadge { accent: ThemeEngine.infoBlue; iconName: "badge-info";    count: groupInfo }
                StatusBadge { accent: ThemeEngine.warnYellow; iconName: "badge-warning"; count: groupWarn }
                StatusBadge { accent: ThemeEngine.failRed;    iconName: "badge-close";   count: groupFail }
                StatusBadge { accent: ThemeEngine.skipGray;   iconName: "badge-skip";    count: groupSkip }
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
                color: isRunning ? ThemeEngine.cyan : ThemeEngine.passGreen
            }
        }

        // ── Expanded body — Flutter ExpansionTile children ────────────
        ColumnLayout {
            Layout.fillWidth: true; Layout.topMargin: 6
            visible: expanded
            spacing: 0
            Rectangle { Layout.fillWidth:true; implicitHeight:1; color:ThemeEngine.colors.borderCard }
            Repeater {
                model: root._modelVersion >= 0 ? root.itemsModel : []
                delegate: Item {
                    Layout.fillWidth: true
                    implicitHeight: testItem.implicitHeight
                    // TreeView connector: vertical line + horizontal stub
                    Rectangle {
                        anchors { top:parent.top; bottom:parent.bottom; left:parent.left; leftMargin:6 }
                        width:2; color:ThemeEngine.colors.borderCard
                    }
                    DiagResultItem {
                        id: testItem
                        anchors { left:parent.left; leftMargin:20; right:parent.right }
                        itemData: modelData
                        onDetailClicked: function(data) { root.detailClicked(data) }
                    }
                }
            }
        }
    }

    // 5WHY: group header had no keyboard access or screen-reader label.
    // Keyboard-only users could not expand/collapse diagnostic groups.
    MouseArea {
        anchors { top:parent.top; left:parent.left; right:parent.right }
        height: 40
        cursorShape: Qt.PointingHandCursor
        onClicked: { _userToggled=true; expanded=!expanded }
    }
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            _userToggled = true; expanded = !expanded; event.accepted = true
        }
    }
    Accessible.name: Tr.groupName(groupIndex) + (expanded ? " — expanded" : " — collapsed")
    Accessible.role: Accessible.Button

    signal detailClicked(var data)

    // ── Status badge: colored icon + count ────────────────────
    // 5WHY: icon size 10→14 (M3 iconXs + bold-stroke SVG compensation),
    // font 10→12 (paired with icon), .slice(-2)→manual zero-pad (no truncation,
    // ES5-compatible — padStart is ES2017, unavailable on embedded Qt builds)
    component StatusBadge: RowLayout {
        property color accent: ThemeEngine.passGreen
        property string iconName: "badge-info"
        property int count: 0
        spacing: 2
        AppIcon { name: iconName; size: 14; color: accent }
        Label {
            text: ThemeEngine.pad2(count)
            font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.Bold; color: accent
        }
    }
}
