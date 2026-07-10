// =============================================================================
// SimulatorScreen.qml — Redesigned per build/simulator-imp.md.
//
// Layout:
//   Toolbar (Device | OS | Orientation | Start Test | Screenshot | Record)
//   ┌──────────────────────────────────────┬─────────────────────────┐
//   │  Simulated Device / OS Viewport     │  Log & Evidence Panel   │
//   │                                      │  ├ Status Summary       │
//   │                                      │  ├ Logs (scrollable)    │
//   │                                      │  └ Evidence (files)     │
//   └──────────────────────────────────────┴─────────────────────────┘
//
// Key changes from v1:
//   - Menu bar REMOVED — all actions are toolbar buttons
//   - Bottom Log/Evidence panel REMOVED — merged into right panel
//   - Left Profile panel REMOVED — target input moved to toolbar area
//   - Right panel transformed to Log & Evidence Panel
//   - Start Test as single test entry point
//   - Recording button toggles Start/Stop
//   - Rotation disabled during recording (方案一)
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"
import "../theme"
import "../utils"
import "../simulator"
import "../"

ApplicationWindow {
    id: page
    objectName: "simulator"
    title: "NetDiagnostics Simulator"
    visible: true
    color: ThemeEngine.bgDark
    visibility: Window.Maximized

    AppFonts {}

    // ══════════════════════════════════════════════════════════════════════
    // Phase 2 startup: init skip policy for default device
    Component.onCompleted: {
        Qt.callLater(function() {
            var os = curOS()
            if (typeof simConfig !== 'undefined' && simConfig) {
                simConfig.setActivePlatform(os)
                if (typeof appState !== 'undefined' && appState)
                    appState.setSkipRules(simConfig.policyRules || [])
            }
        })
    }

    // ══════════════════════════════════════════════════════════════════════
    // Auto-capture on diagnostic lifecycle
    Connections {
        target: appState
        function onDiagFailed(diagIdInt) {
            if (typeof screenshotSvc === 'undefined' || !screenshotSvc) return
            screenshotSvc.captureOnFailure(diagIdInt, "diag_"+diagIdInt, curOS(), curDeviceId())
        }
        function onRunStatusChanged() {
            if (typeof screenshotSvc === 'undefined' || !screenshotSvc) return
            if (appState.runStatus === 2)
                screenshotSvc.captureForTest(-1, "run_complete", curOS(), curDeviceId(), "complete")
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // Device definitions (fallback to hardcoded if SimulatorConfig unavailable)
    property var devices: (typeof simConfig !== 'undefined' && simConfig && simConfig.deviceCount > 0)
        ? simConfig.devices
        : [
            { id:"win-x64",name:"Windows 11 (x64)",os:"windows",w:1024,h:640,bezel:0,island:false,radius:8 },
            { id:"ios-iphone15pm",name:"iPhone 15 Pro Max",os:"ios",w:430,h:932,bezel:12,island:true,radius:55 },
            { id:"android-pixel9",name:"Pixel 9 Pro",os:"android",w:448,h:1008,bezel:7,island:false,radius:20 }
          ]
    property int currentDevice: 0
    property bool portrait: true

    function curDeviceId() { var d = devices[currentDevice]; return d ? (d.id || "") : "" }
    function curOS()        { var d = devices[currentDevice]; return d ? (d.os || "")  : "" }
    function cur()          { return devices[currentDevice] || devices[0] }

    // Phase 2: Dynamic skip-policy wiring on device change
    onCurrentDeviceChanged: {
        var os = curOS()
        if (typeof simConfig !== 'undefined' && simConfig) {
            simConfig.setActivePlatform(os)
            if (typeof appState !== 'undefined' && appState)
                appState.setSkipRules(simConfig.policyRules || [])
        }
    }

    // ── OS metadata ────────────────────────────────────────────────────
    readonly property var osMeta: ({
        linux:{icon:"linux",color:"#FCC624",label:"Linux"}, windows:{icon:"windows",color:"#00A4EF",label:"Windows"},
        macos:{icon:"apple",color:"#007AFF",label:"macOS"}, ios:{icon:"apple",color:"#007AFF",label:"iOS"},
        android:{icon:"android",color:"#3DDC84",label:"Android"}
    })
    function osIcon(os)  { var m=osMeta[os]; return m?m.icon:"circle" }
    function osColor(os) { var m=osMeta[os]; return m?m.color:"#888"  }
    function osLabel(os) { var m=osMeta[os]; return m?m.label:os      }

    // ── Toast ──────────────────────────────────────────────────────────
    property string toastMsg: ""
    function showToast(msg) { toastMsg = msg; toastTimer.restart() }
    Timer { id: toastTimer; interval: 3000; onTriggered: toastMsg = "" }

    // ══════════════════════════════════════════════════════════════════════
    // Recording state — portrait locked during recording (§七 方案一)
    property bool wasPortrait: true
    onPortraitChanged: {
        if (typeof screenshotSvc !== 'undefined' && screenshotSvc && screenshotSvc.recording) {
            portrait = wasPortrait  // revert
            showToast("Screen rotation disabled during recording")
        }
        wasPortrait = portrait
    }

    // ══════════════════════════════════════════════════════════════════════
    // Main layout: Toolbar + [Viewport | Log&Evidence Panel]
    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // ── Toolbar (§八) ─────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 44
            color: ThemeEngine.colors.navBar; z: 10
            border { width: 1; color: ThemeEngine.colors.borderCard }
            RowLayout {
                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }; spacing: 6

                // Device selector
                Rectangle { implicitWidth: 170; implicitHeight: 32; radius: 6
                    color: Qt.alpha(ThemeEngine.colors.textPrimary,0.08)
                    border { width:1; color:Qt.alpha(ThemeEngine.colors.textPrimary,0.15) }
                    RowLayout { anchors.fill:parent; anchors.margins:6; spacing:6
                        AppIcon { name:osIcon(cur().os); size:14; color:osColor(cur().os) }
                        Label { Layout.fillWidth:true; text:cur().name; font.family:ThemeEngine.monoFont; font.pixelSize:11; color:ThemeEngine.colors.textPrimary; elide:Text.ElideRight }
                        Label { text:"▾"; font.pixelSize:12; color:Qt.alpha(ThemeEngine.colors.textPrimary,0.5) }
                    }
                    MouseArea { anchors.fill:parent; onClicked:devicePopup.open() }
                }

                // OS badge
                Rectangle { implicitWidth: 80; implicitHeight: 32; radius: 6
                    color: Qt.alpha(osColor(curOS()),0.12)
                    Label { anchors.centerIn:parent; text:osLabel(curOS()); font.family:ThemeEngine.monoFont; font.pixelSize:11; font.weight:Font.DemiBold; color:osColor(curOS()) }
                }

                // Orientation toggle
                Rectangle { implicitWidth: 32; implicitHeight: 32; radius: 6
                    color: "transparent"; border { width:1; color:Qt.alpha(ThemeEngine.colors.textPrimary,0.2) }
                    opacity: (typeof screenshotSvc !== 'undefined' && screenshotSvc && screenshotSvc.recording) ? 0.3 : 1.0
                    AppIcon { anchors.centerIn:parent; name:"refresh"; size:15; color:Qt.alpha(ThemeEngine.colors.textPrimary,0.7) }
                    MouseArea { anchors.fill:parent
                        enabled: !(typeof screenshotSvc !== 'undefined' && screenshotSvc && screenshotSvc.recording)
                        onClicked: portrait = !portrait }
                }

                Item { Layout.preferredWidth: 4 }

                // Start Test (§四)
                Rectangle { implicitWidth: 80; implicitHeight: 32; radius: 6
                    color: appState.runStatus===1 ? ThemeEngine.warnYellow : (appState.canRun() ? ThemeEngine.accentBlue : Qt.alpha(ThemeEngine.accentBlue,0.3))
                    Label { anchors.centerIn:parent
                        text: appState.runStatus===1 ? "■ Stop" : "▶ Start"
                        font.family:ThemeEngine.monoFont; font.pixelSize:11; color:"white" }
                    MouseArea { anchors.fill:parent; enabled: appState.runStatus===1||appState.canRun()
                        onClicked: {
                            if (appState.runStatus===1) { appState.cancel(); return }
                            appState.target = profileTarget.text.trim()
                            appState.runDiagnostics()
                        }
                    }
                }

                // Screenshot (§五)
                Rectangle { implicitWidth: 80; implicitHeight: 32; radius: 6
                    color: Qt.alpha(ThemeEngine.accentBlue, 0.12)
                    border { width:1; color:Qt.alpha(ThemeEngine.accentBlue, 0.3) }
                    Label { anchors.centerIn:parent; text:"Screenshot"; font.family:ThemeEngine.monoFont; font.pixelSize:10; color:ThemeEngine.accentBlue }
                    MouseArea { anchors.fill:parent; onClicked: {
                        var path = screenshotSvc.makeFilename(curOS(), curDeviceId(), "manual")
                        screenshotSvc.capture(path)
                        showToast("Screenshot: " + (screenshotSvc.lastCapturePath || "failed"))
                    }}
                }

                // Record (§六)
                Rectangle { id: recBtn; implicitWidth: 70; implicitHeight: 32; radius: 6
                    property bool isRec: (typeof screenshotSvc !== 'undefined' && screenshotSvc && screenshotSvc.recording)
                    color: isRec ? Qt.alpha(ThemeEngine.failRed, 0.15) : Qt.alpha(ThemeEngine.passGreen, 0.1)
                    border { width:1; color: isRec ? Qt.alpha(ThemeEngine.failRed,0.4) : Qt.alpha(ThemeEngine.passGreen,0.3) }
                    Label { anchors.centerIn:parent
                        text: isRec ? "■ Stop" : "● Rec"
                        font.family:ThemeEngine.monoFont; font.pixelSize:10
                        color: isRec ? ThemeEngine.failRed : ThemeEngine.passGreen }
                    MouseArea { anchors.fill:parent; onClicked: {
                        if (typeof screenshotSvc === 'undefined' || !screenshotSvc) return
                        if (screenshotSvc.recording) { screenshotSvc.stopRecording() }
                        else { screenshotSvc.startRecording() }
                    }}
                }

                Item { Layout.fillWidth: true }

                // Zoom indicator
                Label {
                    text: Math.round(viewport.scale * 100) + "%"
                    font.family: ThemeEngine.monoFont; font.pixelSize: 9
                    color: Qt.alpha(ThemeEngine.colors.textPrimary, 0.4)
                }

                // Target input (compact, inline)
                Rectangle { implicitWidth: 160; implicitHeight: 30; radius: 6
                    color: ThemeEngine.bgInput
                    border { width:1; color: appState.targetValidationError()!=="" ? ThemeEngine.failRed : ThemeEngine.colors.borderCard }
                    TextField {
                        id: profileTarget
                        anchors { fill:parent; leftMargin:6; rightMargin:4 }
                        font.family:ThemeEngine.monoFont; font.pixelSize:10; color:ThemeEngine.textPrimary
                        placeholderText: "target URL or host"
                        placeholderTextColor: Qt.alpha(ThemeEngine.textSecondary,0.35)
                        verticalAlignment: TextInput.AlignVCenter; background: Item {}
                        text: appState.target
                        onTextChanged: appState.target = text.trim()
                    }
                }
            }
        }

        // ── Body: Viewport | Log & Evidence Panel (§八) ──────────────
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

            // ── Device / OS Viewport ──────────────────────────────────
            Rectangle {
                id: body
                Layout.fillWidth: true; Layout.fillHeight: true
                color: ThemeEngine.colors.surface
                clip: true

                DeviceViewport {
                    id: viewport
                    objectName: "deviceViewport"
                    deviceProfile: cur()
                    portrait: page.portrait

                    Component.onCompleted: {
                        if (typeof screenshotSvc !== 'undefined' && screenshotSvc)
                            screenshotSvc.setViewport(viewport)
                    }
                    x: Math.max(0, (body.width  - frameW * scale) / 2)
                    y: Math.max(0, (body.height - frameH * scale) / 2)

                    Connections {
                        target: page
                        function onWidthChanged()  { viewport.recalcScale(body.width, body.height) }
                        function onHeightChanged() { viewport.recalcScale(body.width, body.height) }
                        function onPortraitChanged(){ viewport.recalcScale(body.width, body.height) }
                        function onCurrentDeviceChanged() { viewport.recalcScale(body.width, body.height) }
                    }
                }
            }

            // ── Log & Evidence Panel (§二.2) ──────────────────────────
            Rectangle {
                id: rightPanel
                Layout.preferredWidth: 280; Layout.fillHeight: true
                color: ThemeEngine.colors.sidebar
                border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout {
                    anchors { fill: parent; margins: 10 }; spacing: 6

                    // ── Status Summary (§二.1) ────────────────────────
                    Rectangle { Layout.fillWidth:true; implicitHeight: summaryCol.implicitHeight+12; radius:8
                        color: Qt.alpha(ThemeEngine.colors.primary,0.06)
                        border { width:1; color:Qt.alpha(ThemeEngine.colors.primary,0.1) }
                        ColumnLayout {
                            id: summaryCol
                            anchors { fill:parent; margins:10 }; spacing: 2
                            RowLayout {
                                Label { text:"Status:"; font.family:ThemeEngine.monoFont; font.pixelSize:10; color:ThemeEngine.textMuted }
                                Label {
                                    text: ({0:Tr.idleStatus,1:Tr.runningStatus,2:Tr.completeStatus,3:Tr.cancelledStatus,4:Tr.errorStatus})[appState.runStatus]||Tr.idleStatus
                                    font.family:ThemeEngine.monoFont; font.pixelSize:10; font.weight:Font.DemiBold
                                    color: appState.runStatus===1?ThemeEngine.cyan:(appState.runStatus===2?ThemeEngine.passGreen:(appState.runStatus>=3?ThemeEngine.failRed:ThemeEngine.textSecondary))
                                }
                                Item { Layout.fillWidth:true }
                                Label { visible: appState.totalDiags>0
                                    text: appState.totalCompleted+"/"+appState.totalDiags
                                    font.family:ThemeEngine.monoFont; font.pixelSize:10; color:ThemeEngine.textSecondary }
                            }
                            Label { text:"Device: "+cur().name; font.family:ThemeEngine.monoFont; font.pixelSize:9; color:ThemeEngine.textSecondary; elide:Text.ElideRight }
                            Label { text:"OS: "+osLabel(curOS()); font.family:ThemeEngine.monoFont; font.pixelSize:9; color:ThemeEngine.textSecondary }
                            Label { visible: appState.currentDiagLabel!==""
                                text:"Test: "+appState.currentDiagLabel; font.family:ThemeEngine.monoFont; font.pixelSize:9; color:ThemeEngine.cyan; elide:Text.ElideRight }
                            Label { visible: appState.errorMessage!==""
                                text:Tr.errorPrefix+appState.errorMessage; font.family:ThemeEngine.monoFont; font.pixelSize:9; color:ThemeEngine.failRed; wrapMode:Text.WordWrap; maximumLineCount:2 }
                        }
                    }

                    // ── Log output (§二.2) ────────────────────────────
                    Label { text:"LOG"; font.family:ThemeEngine.monoFont; font.pixelSize:10; font.weight:Font.Bold; color:ThemeEngine.textMuted }
                    // Filter row
                    RowLayout {
                        id: logFilter
                        property string filter: "All"
                        Repeater {
                            model: ["All","Info","Warn","Error","Skip","Evidence"]
                            delegate: Rectangle { implicitWidth:32; implicitHeight:20; radius:4
                                color: logFilter.filter===modelData ? Qt.alpha(ThemeEngine.cyan,0.15) : "transparent"
                                Label { anchors.centerIn:parent; text:modelData; font.family:ThemeEngine.monoFont; font.pixelSize:8
                                    color: logFilter.filter===modelData?ThemeEngine.cyan:ThemeEngine.textMuted }
                                MouseArea { anchors.fill:parent; onClicked: logFilter.filter=modelData }
                            }
                        }
                    }
                    ListView {
                        id: logList
                        Layout.fillWidth:true; Layout.fillHeight:true; clip:true
                        model: evidenceListModel
                        delegate: RowLayout {
                            width: ListView.view.width; spacing:4
                            Rectangle { implicitWidth:6; implicitHeight:6; radius:3
                                color: ({auto:"#4ADE80",manual:"#38BDF8",failure:"#EF4444",recording:"#FBBF24",complete:"#4ADE80"})[modelData.trigger]||"#888" }
                            Label { text:modelData.time||""; font.family:ThemeEngine.monoFont; font.pixelSize:8; color:ThemeEngine.textMuted; Layout.preferredWidth:48 }
                            Label { Layout.fillWidth:true; text:modelData.test||modelData.path||""; font.family:ThemeEngine.monoFont; font.pixelSize:8; color:ThemeEngine.textSecondary; elide:Text.ElideRight }
                        }
                    }
                    property var evidenceListModel: (typeof screenshotSvc !== 'undefined' && screenshotSvc) ? screenshotSvc.evidenceLog : []

                    // ── Evidence (§二.3) ──────────────────────────────
                    Label { text:"EVIDENCE"; font.family:ThemeEngine.monoFont; font.pixelSize:10; font.weight:Font.Bold; color:ThemeEngine.textMuted }
                    ListView {
                        Layout.fillWidth:true; Layout.preferredHeight:60; clip:true
                        model: evidenceListModel
                        delegate: Label {
                            width: ListView.view.width
                            text:(modelData.trigger||"?")+" · "+(modelData.path||"")
                            font.family:ThemeEngine.monoFont; font.pixelSize:8; color:ThemeEngine.skipGray; elide:Text.ElideMiddle
                            MouseArea { anchors.fill:parent; onClicked: {
                                if (modelData.path) Qt.openUrlExternally("file:///"+modelData.path)
                            }}
                        }
                    }
                }
            }
        }
    }

    // ── Toast overlay (window-level) ────────────────────────────────────
    Label {
        id: toastLabel
        anchors { bottom: parent.bottom; bottomMargin: 16; horizontalCenter: parent.horizontalCenter }
        text: toastMsg; visible: toastMsg !== ""
        font.family: ThemeEngine.monoFont; font.pixelSize: 11
        color: ThemeEngine.cyan
        background: Rectangle { radius:6; color:Qt.alpha(ThemeEngine.bgDark,0.85)
            border { width:1; color:Qt.alpha(ThemeEngine.cyan,0.3) } }
        padding: 8; z: 1000
    }

    // ══════════════════════════════════════════════════════════════════════
    // Device popup
    Popup {
        id: devicePopup
        y: 72; x: Math.max(page.width-340,0)
        closePolicy: Popup.CloseOnEscape|Popup.CloseOnPressOutside
        width: 310; height: Math.min(500, devices.length*54+16); padding: 8
        background: Rectangle { radius:10; color:ThemeEngine.bgDark; border{width:1;color:ThemeEngine.colors.borderCard} }
        ListView {
            anchors.fill:parent; clip:true
            model: devices
            section.property: "os"
            section.delegate: Rectangle { width:ListView.view.width; implicitHeight:22; color:Qt.alpha(osColor(section),0.08)
                Label { anchors{left:parent.left;leftMargin:8;verticalCenter:parent.verticalCenter} text:osLabel(section); font.family:ThemeEngine.monoFont; font.pixelSize:10; font.weight:Font.Bold; color:osColor(section) } }
            delegate: ItemDelegate {
                width:ListView.view.width; implicitHeight:46
                contentItem: RowLayout { spacing:8
                    Rectangle { implicitWidth:28; implicitHeight:28; radius:6; color:Qt.alpha(osColor(modelData.os),0.15)
                        AppIcon { anchors.centerIn:parent; name:osIcon(modelData.os); size:16; color:osColor(modelData.os) } }
                    ColumnLayout { spacing:0
                        Label { text:modelData.name; font.family:ThemeEngine.monoFont; font.pixelSize:12; color:"white" }
                        Label { text:(modelData.w)+"×"+(modelData.h)+" · "+osLabel(modelData.os); font.family:ThemeEngine.monoFont; font.pixelSize:9; color:Qt.alpha("white",0.4) }
                    }
                }
                background: Rectangle { color:index===currentDevice?Qt.alpha("#0078D4",0.2):"transparent"; radius:6 }
                onClicked: { currentDevice=index; devicePopup.close() }
            }
        }
    }
}
