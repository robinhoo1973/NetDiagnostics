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

    height: cardColumn.implicitHeight + 16
    radius: 10
    color: "#16213E"
    border { width: 1; color: isRunning ? Qt.alpha("#00BCD4", 0.4) : "#2A2A4A" }

    // ── Computed state — all from C++ groupStats (single source of truth) ──
    property var allItems: { let _v = appState.resultsVersion; return appState.allDiagsForGroup(groupIndex) }
    // ── Computed state — all from C++ groupStats (single source of truth) ──
    property int enabledCount: { let _v = _modelVersion; var s=appState.groupStats(groupIndex); return s.total||0 }
    property int completedCount: { let _v = _modelVersion; var s=appState.groupStats(groupIndex); return s.completed||0 }
    property bool isRunning: appState.runStatus===1 && completedCount<enabledCount && completedCount>0
    property int groupPass: { let _v = _modelVersion; var s=appState.groupStats(groupIndex); return s.pass||0 }
    property int groupWarn: { let _v = _modelVersion; var s=appState.groupStats(groupIndex); return s.warn||0 }
    property int groupFail: { let _v = _modelVersion; var s=appState.groupStats(groupIndex); return s.fail||0 }
    property int groupSkip: { let _v = _modelVersion; var s=appState.groupStats(groupIndex); return s.skip||0 }
    property int groupInfo: { let _v = _modelVersion; var s=appState.groupStats(groupIndex); return s.info||0 }

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
        // Force fresh reference: clear first, then assign
        // On ARM64 Qt 6.8.2, QML does not detect internal array changes
        var fresh = appState.allDiagsForGroup(groupIndex)
        itemsModel = []
        itemsModel = fresh
        _modelVersion++
    }
    Component.onCompleted: reloadModel()

    ColumnLayout {
        id: cardColumn
        anchors { fill: parent; leftMargin: 12; rightMargin: 12; topMargin: 8; bottomMargin: 8 }
        spacing: 0

        // ── Header ────────────────────────────────────────────────────
        RowLayout {
            spacing: 8
            Rectangle { width:3; height:24; radius:2; color:isRunning?"#00BCD4":"#0078D4" }
            ColumnLayout { spacing:1
                Label { text:"G"+(groupIndex+1)+": "+(Tr.groupName(groupIndex)); font.family:ThemeEngine.monoFont; font.pixelSize:13; font.weight:Font.DemiBold; color:"#E0E0E0" }
                Label { visible:isRunning; text:"Running: "+(appState.currentDiagLabel||"")+"..."; font.family:ThemeEngine.monoFont; font.pixelSize:10; font.italic:true; color:"#00BCD4"; elide:Text.ElideRight }
            }
            Item { Layout.fillWidth:true }
            Label { visible:isRunning||completedCount>0; text:completedCount+"/"+enabledCount; font.family:ThemeEngine.monoFont; font.pixelSize:11; font.weight:Font.Medium; color:"#A0A0B8" }
            Rectangle { visible:groupPass>0; implicitWidth:26; implicitHeight:18; radius:4; color:Qt.alpha("#4ADE80",0.15); Label { anchors.centerIn:parent; text:groupPass; font.family:ThemeEngine.monoFont; font.pixelSize:10; color:"#4ADE80"; font.weight:Font.Bold } }
            Rectangle { visible:groupWarn>0; implicitWidth:26; implicitHeight:18; radius:4; color:Qt.alpha("#FACC15",0.15); Label { anchors.centerIn:parent; text:groupWarn; font.family:ThemeEngine.monoFont; font.pixelSize:10; color:"#FACC15"; font.weight:Font.Bold } }
            Rectangle { visible:groupFail>0; implicitWidth:26; implicitHeight:18; radius:4; color:Qt.alpha("#EF4444",0.15); Label { anchors.centerIn:parent; text:groupFail; font.family:ThemeEngine.monoFont; font.pixelSize:10; color:"#EF4444"; font.weight:Font.Bold } }
            Rectangle { visible:groupSkip>0; implicitWidth:26; implicitHeight:18; radius:4; color:Qt.alpha("#888888",0.15); Label { anchors.centerIn:parent; text:groupSkip; font.family:ThemeEngine.monoFont; font.pixelSize:10; color:"#888888"; font.weight:Font.Bold } }
            Rectangle { visible:groupInfo>0; implicitWidth:26; implicitHeight:18; radius:4; color:Qt.alpha("#0078D4",0.15); Label { anchors.centerIn:parent; text:groupInfo; font.family:ThemeEngine.monoFont; font.pixelSize:10; color:"#0078D4"; font.weight:Font.Bold } }
            Label { text:expanded?"▼":"▶"; font.pixelSize:10; color:"#A0A0B8" }
        }

        // ── Progress bar ──────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 4; Layout.topMargin: 6
            visible: isRunning || completedCount > 0
            radius: 2; color: "#2A2A4A"
            Rectangle {
                height: 4; radius: 2
                width: enabledCount>0 ? parent.width*(completedCount/enabledCount) : 0
                color: isRunning ? "#00BCD4" : "#4ADE80"
            }
        }

        // ── Expanded body — Flutter ExpansionTile children ────────────
        ColumnLayout {
            Layout.fillWidth: true; Layout.topMargin: 6
            visible: expanded
            spacing: 0
            Rectangle { Layout.fillWidth:true; implicitHeight:1; color:"#2A2A4A" }
            Repeater {
                model: root._modelVersion >= 0 ? root.itemsModel : []
                delegate: Item {
                    Layout.fillWidth: true
                    implicitHeight: testItem.implicitHeight
                    // TreeView connector: vertical line + horizontal stub
                    Rectangle {
                        anchors { top:parent.top; bottom:parent.bottom; left:parent.left; leftMargin:6 }
                        width:2; color:"#2A2A4A"
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

    // Click header to toggle
    MouseArea {
        anchors { top:parent.top; left:parent.left; right:parent.right }
        height: 40
        onClicked: { _userToggled=true; expanded=!expanded }
    }

    signal detailClicked(var data)
}
