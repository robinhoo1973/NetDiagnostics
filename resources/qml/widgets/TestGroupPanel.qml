import QtQuick
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

    // ── Computed state ────────────────────────────────────────────────
    property var allItems: { let _v = appState.resultsVersion; return appState.allTestsForGroup(groupIndex) }
    property int enabledCount: { var c=0; for(var i=0;i<allItems.length;i++)c++;return c }
    property int completedCount: { var c=0; for(var i=0;i<allItems.length;i++)if(allItems[i].isDone)c++;return c }
    property bool isRunning: appState.runStatus===1 && completedCount<enabledCount && completedCount>0
    property int groupPass: { var c=0; for(var i=0;i<allItems.length;i++)if(allItems[i].isDone&&allItems[i].status===0)c++;return c }
    property int groupWarn: { var c=0; for(var i=0;i<allItems.length;i++)if(allItems[i].isDone&&allItems[i].status===1)c++;return c }
    property int groupFail: { var c=0; for(var i=0;i<allItems.length;i++)if(allItems[i].isDone&&allItems[i].status===2)c++;return c }
    property int groupSkip: { var c=0; for(var i=0;i<allItems.length;i++)if(allItems[i].isDone&&allItems[i].status===3)c++;return c }

    onIsRunningChanged: if(!_userToggled)expanded=isRunning||completedCount>0
    onCompletedCountChanged: if(!_userToggled&&completedCount>0)expanded=true

    Timer {
        id: pollTimer
        interval: 300
        running: isRunning || appState.runStatus === 1
        repeat: true
        onTriggered: reloadModel()
    }
    property var itemsModel: []
    function reloadModel() {
        itemsModel = appState.allTestsForGroup(groupIndex)
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
                Label { text:appState.groupLabels[groupIndex]||("Group"+(groupIndex+1)); font.family:"JetBrains Mono"; font.pixelSize:13; font.weight:Font.DemiBold; color:"#E0E0E0" }
                Label { visible:isRunning; text:"Running: "+(appState.currentTestLabel||"")+"..."; font.family:"JetBrains Mono"; font.pixelSize:10; font.italic:true; color:"#00BCD4"; elide:Text.ElideRight }
            }
            Item { Layout.fillWidth:true }
            Label { visible:isRunning||completedCount>0; text:completedCount+"/"+enabledCount; font.family:"JetBrains Mono"; font.pixelSize:11; font.weight:Font.Medium; color:"#A0A0B8" }
            Rectangle { visible:groupPass>0; implicitWidth:26; implicitHeight:18; radius:4; color:Qt.alpha("#4ADE80",0.15); Label { anchors.centerIn:parent; text:groupPass; font.family:"JetBrains Mono"; font.pixelSize:10; color:"#4ADE80"; font.weight:Font.Bold } }
            Rectangle { visible:groupWarn>0; implicitWidth:26; implicitHeight:18; radius:4; color:Qt.alpha("#FACC15",0.15); Label { anchors.centerIn:parent; text:groupWarn; font.family:"JetBrains Mono"; font.pixelSize:10; color:"#FACC15"; font.weight:Font.Bold } }
            Rectangle { visible:groupFail>0; implicitWidth:26; implicitHeight:18; radius:4; color:Qt.alpha("#EF4444",0.15); Label { anchors.centerIn:parent; text:groupFail; font.family:"JetBrains Mono"; font.pixelSize:10; color:"#EF4444"; font.weight:Font.Bold } }
            Rectangle { visible:groupSkip>0; implicitWidth:26; implicitHeight:18; radius:4; color:Qt.alpha("#888888",0.15); Label { anchors.centerIn:parent; text:groupSkip; font.family:"JetBrains Mono"; font.pixelSize:10; color:"#888888"; font.weight:Font.Bold } }
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
                model: root.itemsModel
                delegate: Item {
                    Layout.fillWidth: true
                    implicitHeight: testItem.implicitHeight
                    // TreeView connector: vertical line + horizontal stub
                    Rectangle {
                        anchors { top:parent.top; bottom:parent.bottom; left:parent.left; leftMargin:6 }
                        width:2; color:"#2A2A4A"
                    }
                    TestResultItem {
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
