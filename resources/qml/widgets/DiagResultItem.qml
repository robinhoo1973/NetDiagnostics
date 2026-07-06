import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts

// ── TestResultItem — collapsed row + detailClicked signal ──────────────
Item {
    id: root
    property var itemData: ({})
    implicitHeight: 28
    signal detailClicked(var data)

    // ── Pending item ──────────────────────────────────────────────────
    RowLayout {
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 4 }
        visible: itemData.isPending; spacing: 8
        AppIcon {
            name: itemData.isRunning ? "spinner" : "badge-skip"; size: 12
            color: itemData.isRunning ? ThemeEngine.colors.primary : ThemeEngine.textMuted
            RotationAnimation on rotation { running: itemData.isRunning; from:0; to:360; duration:1000; loops:Animation.Infinite }
        }
        Label {
            text: itemData.displayName || ("#" + itemData.diagId)
            font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textSecondary
            Layout.fillWidth: true; elide: Text.ElideRight
        }
        Label {
            visible: itemData.isRunning; text: Tr.diagRunning
            font.family:ThemeEngine.monoFont; font.pixelSize:10; font.italic:true; color:ThemeEngine.colors.primary
        }
    }

    // ── Completed row ─────────────────────────────────────────────────
    RowLayout {
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 4 }
        visible: !itemData.isPending; spacing: 8
        AppIcon {
            name: { var s=itemData.status; if(s===0)return"badge-check"; if(s===2)return"badge-close"; if(s===1)return"badge-warning"; if(s===4)return"badge-error"; if(s===5)return"badge-info"; return"badge-skip" }
            size: 12
            color: { var s=itemData.status; return s===0?ThemeEngine.passGreen:(s===1?ThemeEngine.warnYellow:(s===2?ThemeEngine.failRed:(s===3?ThemeEngine.skipGray:ThemeEngine.infoBlue))) }
        }
        Label {
            text: itemData.displayName || ("#" + itemData.diagId)
            font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.Medium
            color: { var s=itemData.status; return(s===0)?"#E0E0E0":(s===2?"#EF4444":"#A0A0B8") }
            Layout.fillWidth: true; elide: Text.ElideRight
        }
        Rectangle {
            visible: (itemData.durationMs||0)>0; implicitWidth:durText.implicitWidth+12; implicitHeight:20; radius:4
            color: "#2A2A4A"
            Label { id:durText; anchors.centerIn:parent; text:_fmtDur(itemData.durationMs||0); font.family:ThemeEngine.monoFont; font.pixelSize:10; color:"#A0A0B8" }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: !itemData.isPending
        onClicked: root.detailClicked(itemData)
    }

    function _fmtDur(ms) {
        if (ms<1000) return ms+"ms"
        if (ms<60000) return (ms/1000).toFixed(1)+"s"
        var m=Math.floor(ms/60000); var s=Math.floor((ms%60000)/1000)
        return m+"m"+s+"s"
    }
}
