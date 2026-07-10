// =============================================================================
// SimulatorScreen.qml — Refactored simulator main window (Phase 1).
//
// Architecture per build/simulator.md §三:
//   MenuBar + ToolBar → DeviceViewport ← Profile Panel / Test Panel / Log Panel
//
// The DeviceViewport is the central rendering surface for simulated OS/device
// UI.  ScreenshotService captures only this region.
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

    // ── Phase 2 startup: init skip policy for default device ──────────
    Component.onCompleted: {
        // Trigger onCurrentDeviceChanged to wire skip rules on first load
        Qt.callLater(function() {
            var os = curOS()
            if (typeof simConfig !== 'undefined' && simConfig) {
                simConfig.setActivePlatform(os)
                if (typeof appState !== 'undefined' && appState)
                    appState.setSkipRules(simConfig.policyRules || [])
            }
        })
    }

    // ── Phase 3: Auto-capture on diagnostic lifecycle ─────────────────
    Connections {
        target: appState
        function onDiagFailed(diagIdInt) {
            if (typeof screenshotSvc === 'undefined' || !screenshotSvc) return
            var testName = "diag_" + diagIdInt
            screenshotSvc.captureOnFailure(diagIdInt, testName, curOS(), curDeviceId())
        }
        function onRunStatusChanged() {
            if (typeof screenshotSvc === 'undefined' || !screenshotSvc) return
            if (appState.runStatus === 2) {
                screenshotSvc.captureForTest(-1, "run_complete", curOS(), curDeviceId(), "complete")
            }
        }
    }

    // ── Menu bar (build/simulator.md §五.1) ─────────────────────────────
    menuBar: SimulatorMenuBar {
        onFileExit:              page.close()
        onDeviceSelectRequested: devicePopup.open()
        onDeviceOrientationToggle:page.portrait = !page.portrait
        onDeviceFitToWindow:     viewport.recalcScale(body.width, body.height)
        onCaptureScreenshot: {
            var path = screenshotSvc.makeFilename(curOS(), curDeviceId(), "manual")
            screenshotSvc.capture(path)
            toast.show("Screenshot saved: " + (screenshotSvc.lastCapturePath || ""))
        }
        onCaptureStartRecording:  screenshotSvc.startRecording()
        onCaptureStopRecording:   screenshotSvc.stopRecording()
        onTestRunSelected:        appState.runDiagnostics()
        onTestRunSuite: {
            if (typeof matrixOrch === 'undefined' || !matrixOrch) return
            var osList = (typeof simConfig !== 'undefined' && simConfig) ? simConfig.osList : [{id:curOS()}]
            var devList = devices
            var tgtList = [{targetUrl: profileTarget.text || "localhost", port: appState.targetPort, protocolSchema: appState.targetScheme || "https"}]
            var testList = ["G4Ping", "G4Traceroute", "G3DnsServers"]
            matrixOrch.generate(osList, devList, tgtList, testList)
            showToast("Matrix: " + matrixOrch.totalSteps + " combinations generated")
        }
        onTestRunFullMatrix: {
            if (typeof matrixOrch === 'undefined' || !matrixOrch) return
            var osList = (typeof simConfig !== 'undefined' && simConfig) ? simConfig.osList : [{id:curOS()}]
            matrixOrch.generate(osList, devices, [{targetUrl: profileTarget.text || "localhost"}], [])
            showToast("Full matrix: " + matrixOrch.totalSteps + " combinations")
        }
        onTestStop:              appState.cancel()
        onTestClearResults:      appState.reset()
        onViewToggleLog:         logPanel.visible = !logPanel.visible
        onViewToggleParameters:  leftPanel.visible = !leftPanel.visible
        onViewToggleResults:     rightPanel.visible = !rightPanel.visible
        onViewFitDevice:         viewport.recalcScale(body.width, body.height)
        onViewZoomIn:            viewport.scale = Math.min(viewport.scale * 1.1, 3.0)
        onViewZoomOut:           viewport.scale = Math.max(viewport.scale * 0.9, 0.15)
    }

    // ══════════════════════════════════════════════════════════════════════
    // Device definitions (fallback — overridden by SimulatorConfig if loaded)
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

    // ── Phase 2: Dynamic skip-policy wiring ────────────────────────────
    // When the simulated device/OS changes, update the active platform
    // in SimulatorConfig and push the new skip rules to AppState so the
    // policy engine enforces them during test execution.
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

    // ══════════════════════════════════════════════════════════════════════
    // Toast notification
    property string toastMsg: ""
    function showToast(msg) { toastMsg = msg; toastTimer.restart() }
    Timer { id: toastTimer; interval: 3000; onTriggered: toastMsg = "" }

    // ══════════════════════════════════════════════════════════════════════
    // Main layout:  [Left Panel] | [DeviceViewport] | [Right Panel]
    //               [          Log Panel (bottom)            ]
    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // ── Top tool bar ────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 48
            color: ThemeEngine.colors.navBar; z: 10
            border { width: 1; color: ThemeEngine.colors.borderCard }
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Label { text:"NetDiagnostics Simulator"; font.family:ThemeEngine.monoFont; font.pixelSize:15; font.weight:Font.DemiBold; color:ThemeEngine.colors.textPrimary }
                Item { Layout.fillWidth: true }

                // OS badge
                Rectangle { implicitWidth:100; implicitHeight:30; radius:6; color:Qt.alpha(osColor(curOS()),0.12)
                    RowLayout { anchors.centerIn:parent; spacing:6
                        AppIcon { name:osIcon(curOS()); size:14; color:osColor(curOS()) }
                        Label { text:osLabel(curOS()); font.family:ThemeEngine.monoFont; font.pixelSize:11; font.weight:Font.DemiBold; color:osColor(curOS()) }
                    }
                }
                Item { width:8 }

                // Device selector
                Rectangle { implicitWidth:200; implicitHeight:34; radius:6; color:Qt.alpha(ThemeEngine.colors.textPrimary,0.08); border{width:1;color:Qt.alpha(ThemeEngine.colors.textPrimary,0.15)}
                    RowLayout { anchors.fill:parent; anchors.margins:8
                        AppIcon { name:osIcon(cur().os); size:14; color:osColor(cur().os) }
                        Label { Layout.fillWidth:true; text:cur().name; font.family:ThemeEngine.monoFont; font.pixelSize:12; color:ThemeEngine.colors.textPrimary; elide:Text.ElideRight }
                        Label { text:"▾"; font.pixelSize:14; color:Qt.alpha(ThemeEngine.colors.textPrimary,0.6) }
                    }
                    MouseArea { anchors.fill:parent; onClicked:devicePopup.open() }
                }
                Item { width:8 }

                // Rotate button
                Rectangle { implicitWidth:34; implicitHeight:34; radius:6; color:"transparent"; border{width:1;color:Qt.alpha(ThemeEngine.colors.textPrimary,0.2)}
                    AppIcon { anchors.centerIn:parent; name:"refresh"; size:16; color:Qt.alpha(ThemeEngine.colors.textPrimary,0.7) }
                    MouseArea { anchors.fill:parent; onClicked: portrait = !portrait }
                }
                Item { width:6 }

                // Zoom indicator
                Label {
                    text: Math.round(viewport.scale * 100) + "%"
                    font.family: ThemeEngine.monoFont; font.pixelSize: 10
                    color: Qt.alpha(ThemeEngine.colors.textPrimary, 0.5)
                    Layout.preferredWidth: 40
                    horizontalAlignment: Text.AlignHCenter
                }

                // Run/Stop button (§五.2: toolbar Run)
                Rectangle { implicitWidth:36; implicitHeight:30; radius:6
                    color: appState.runStatus===1 ? ThemeEngine.warnYellow : (appState.canRun() ? ThemeEngine.accentBlue : Qt.alpha(ThemeEngine.accentBlue,0.3))
                    Label { anchors.centerIn:parent; text: appState.runStatus===1 ? "■" : "▶"
                        font.family:ThemeEngine.monoFont; font.pixelSize:12; color:"white" }
                    MouseArea { anchors.fill:parent; enabled: appState.runStatus===1||appState.canRun()
                        onClicked: { if(appState.runStatus===1)appState.cancel(); else{appState.target=profileTarget.text.trim();appState.runDiagnostics()} }}
                }
                Item { width:4 }

                // Screenshot button
                Rectangle { implicitWidth:34; implicitHeight:34; radius:6; color:"transparent"; border{width:1;color:Qt.alpha(ThemeEngine.colors.textPrimary,0.2)}
                    AppIcon { anchors.centerIn:parent; name:"config"; size:16; color:Qt.alpha(ThemeEngine.colors.textPrimary,0.7) }
                    MouseArea { anchors.fill:parent; onClicked: {
                        var path = screenshotSvc.makeFilename(curOS(), curDeviceId(), "manual")
                        screenshotSvc.capture(path)
                        showToast("Screenshot: " + (screenshotSvc.lastCapturePath || "failed"))
                    }}
                }
            }
        }

        // ── Body: three-column layout ───────────────────────────────────
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

            // ── Left: Profile Panel ─────────────────────────────────────
            Rectangle {
                id: leftPanel
                Layout.preferredWidth: 220; Layout.fillHeight: true
                visible: true; color: ThemeEngine.colors.sidebar
                border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout {
                    anchors { fill: parent; margins: 12 }; spacing: 8
                    Label { text:"TARGET"; font.family:ThemeEngine.monoFont; font.pixelSize:10; font.weight:Font.Bold; color:ThemeEngine.colors.primary }

                    // Target URL / host
                    Rectangle { Layout.fillWidth:true; implicitHeight:34; radius:6; color:ThemeEngine.bgInput; border{width:1;color:ThemeEngine.colors.borderCard}
                        TextField {
                            id: profileTarget
                            anchors { fill:parent; leftMargin:8; rightMargin:8 }
                            font.family:ThemeEngine.monoFont; font.pixelSize:11; color:ThemeEngine.textPrimary
                            placeholderText: "example.com or https://example.com"
                            placeholderTextColor: Qt.alpha(ThemeEngine.textSecondary,0.4)
                            verticalAlignment: TextInput.AlignVCenter; background: Item {}
                            onTextChanged: appState.target = text.trim()
                        }
                    }
                    // Structured fields (§四.6): protocol + port row
                    RowLayout {
                        spacing: 6
                        // Protocol schema
                        Rectangle { Layout.preferredWidth: 80; implicitHeight: 30; radius: 6
                            color: ThemeEngine.bgInput; border{width:1;color:ThemeEngine.colors.borderCard}
                            ComboBox {
                                id: schemaCombo
                                anchors.fill: parent; flat: true
                                font.family: ThemeEngine.monoFont; font.pixelSize: 10
                                model: appState.supportedSchemes
                                currentIndex: {
                                    var s = appState.targetScheme || "https"
                                    for (var i = 0; i < model.length; i++)
                                        if (model[i] === s) return i
                                    return 0
                                }
                                onCurrentTextChanged: appState.targetScheme = currentText
                                background: Item {}
                                contentItem: Label { text: schemaCombo.currentText + "://"
                                    font: schemaCombo.font; color: ThemeEngine.textPrimary
                                    verticalAlignment: Text.AlignVCenter }
                            }
                        }
                        // Port
                        Rectangle { Layout.preferredWidth: 56; implicitHeight: 30; radius: 6
                            color: ThemeEngine.bgInput; border{width:1;color:ThemeEngine.colors.borderCard}
                            TextField {
                                id: portField
                                anchors { fill:parent; leftMargin:6; rightMargin:4 }
                                font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.textPrimary
                                placeholderText: appState.defaultPortForScheme > 0 ? ""+appState.defaultPortForScheme : "port"
                                placeholderTextColor: Qt.alpha(ThemeEngine.textSecondary,0.4)
                                text: appState.targetPort > 0 ? ""+appState.targetPort : ""
                                verticalAlignment: TextInput.AlignVCenter; background: Item {}
                                onTextChanged: { var v = parseInt(text); appState.targetPort = isNaN(v) ? -1 : v }
                            }
                        }
                    }
                    // Run button
                    Rectangle { Layout.fillWidth:true; implicitHeight:36; radius:6; color:appState.runStatus===1?ThemeEngine.warnYellow:ThemeEngine.accentBlue
                        Label { anchors.centerIn:parent; text:appState.runStatus===1?"■ Stop":"▶ Run Diagnostics"
                            font.family:ThemeEngine.monoFont; font.pixelSize:12; color:"white" }
                        MouseArea { anchors.fill:parent; onClicked: { if(appState.runStatus===1)appState.cancel(); else{appState.target=profileTarget.text.trim();appState.runDiagnostics()} }}
                    }
                    Item { Layout.preferredHeight:8 }
                    // Skip-rule list — dynamic, updates with simulated OS
                    Label { text:"SKIP RULES · " + curOS().toUpperCase()
                        font.family:ThemeEngine.monoFont; font.pixelSize:10; font.weight:Font.Bold; color:ThemeEngine.textMuted }
                    ListView {
                        Layout.fillWidth:true; Layout.fillHeight:true; clip:true
                        model: (typeof simConfig !== 'undefined' && simConfig) ? simConfig.policyRules : []
                        delegate: RowLayout {
                            width: ListView.view.width; spacing: 4
                            AppIcon { name:"badge-skip"; size:8; color:ThemeEngine.skipGray }
                            Label { Layout.fillWidth:true; text:modelData.reason||""; font.family:ThemeEngine.monoFont; font.pixelSize:9; color:ThemeEngine.textSecondary; wrapMode:Text.WordWrap }
                        }
                    }
                }
            }

            // ── Center: Device / OS Viewport ────────────────────────────
            Rectangle {
                id: body
                Layout.fillWidth: true; Layout.fillHeight: true
                color: ThemeEngine.colors.surface
                clip: true

                // Centered DeviceViewport with proportional scaling
                DeviceViewport {
                    id: viewport
                    objectName: "deviceViewport"
                    deviceProfile: cur()
                    portrait: page.portrait

                    Component.onCompleted: {
                        // Bind viewport to ScreenshotService so captures
                        // target ONLY this region (build/simulator.md §四.7)
                        if (typeof screenshotSvc !== 'undefined' && screenshotSvc)
                            screenshotSvc.setViewport(viewport)
                    }

                    // Center horizontally, offset vertically for top bar space
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

            // ── Right: Test Panel ───────────────────────────────────────
            Rectangle {
                id: rightPanel
                Layout.preferredWidth: 240; Layout.fillHeight: true
                visible: true; color: ThemeEngine.colors.sidebar
                border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout {
                    anchors { fill: parent; margins: 12 }; spacing: 6
                    Label { text:"TEST STATUS"; font.family:ThemeEngine.monoFont; font.pixelSize:10; font.weight:Font.Bold; color:ThemeEngine.textMuted }
                    RowLayout {
                        Label { text:"Status:"; font.family:ThemeEngine.monoFont; font.pixelSize:12; color:ThemeEngine.textSecondary }
                        Label {
                            text: ({0:"Idle",1:"Running…",2:"Complete",3:"Cancelled",4:"Error"})[appState.runStatus]||"Idle"
                            font.family:ThemeEngine.monoFont; font.pixelSize:12; font.weight:Font.DemiBold
                            color: appState.runStatus===1?ThemeEngine.cyan:(appState.runStatus===2?ThemeEngine.passGreen:(appState.runStatus===3||appState.runStatus===4?ThemeEngine.failRed:ThemeEngine.textSecondary))
                        }
                    }
                    Label { Layout.fillWidth:true
                        visible: appState.totalDiags > 0
                        text:"Progress: " + appState.totalCompleted + " / " + appState.totalDiags
                        font.family:ThemeEngine.monoFont; font.pixelSize:11; color:ThemeEngine.textSecondary }
                    LiveProgressPanel { Layout.fillWidth: true }
                }
            }
        }

        // ── Bottom: Log Panel ───────────────────────────────────────────
        Rectangle {
            id: logPanel
            Layout.fillWidth: true; implicitHeight: 120
            visible: true; color: ThemeEngine.colors.navBar
            border { width: 1; color: ThemeEngine.colors.borderCard }
            ColumnLayout {
                anchors { fill: parent; margins: 8 }; spacing: 4
                Label { text:"LOG / EVIDENCE"; font.family:ThemeEngine.monoFont; font.pixelSize:10; font.weight:Font.Bold; color:ThemeEngine.textMuted }
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    ListView {
                        id: evidenceListView
                        model: (typeof screenshotSvc !== 'undefined' && screenshotSvc)
                               ? screenshotSvc.evidenceLog : []
                        delegate: RowLayout {
                            width: ListView.view.width; spacing: 6
                            Label { text: modelData.time || ""; font.family:ThemeEngine.monoFont; font.pixelSize:9; color:ThemeEngine.textMuted; Layout.preferredWidth: 80 }
                            Label { text: "[" + (modelData.trigger||"?") + "]"; font.family:ThemeEngine.monoFont; font.pixelSize:9; color:ThemeEngine.cyan; Layout.preferredWidth: 60 }
                            Label { text: modelData.test || ""; font.family:ThemeEngine.monoFont; font.pixelSize:9; color:ThemeEngine.textSecondary; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: modelData.path || ""; font.family:ThemeEngine.monoFont; font.pixelSize:8; color:ThemeEngine.textMuted; Layout.fillWidth: true; elide: Text.ElideLeft }
                        }
                    }
                }
            }
        }
    }

    // ── Toast overlay (window-level — does not cover viewport content) ──
    Label {
        id: toastLabel
        anchors { bottom: parent.bottom; bottomMargin: 16; horizontalCenter: parent.horizontalCenter }
        text: toastMsg; visible: toastMsg !== ""
        font.family: ThemeEngine.monoFont; font.pixelSize: 11
        color: ThemeEngine.cyan
        background: Rectangle { radius:6; color:Qt.alpha(ThemeEngine.bgDark,0.85)
            border { width:1; color:Qt.alpha(ThemeEngine.cyan,0.3) } }
        padding: 8
        z: 1000
    }

    // ══════════════════════════════════════════════════════════════════════
    // Device popup (same as before)
    Popup {
        id: devicePopup
        y: 90; x: Math.max(page.width-340,0)
        closePolicy: Popup.CloseOnEscape|Popup.CloseOnPressOutside
        width: 310; height: Math.min(500, devices.length*54+16); padding: 8
        background: Rectangle { radius:10; color:ThemeEngine.bgDark; border{width:1;color:ThemeEngine.colors.borderCard} }
        ListView {
            anchors.fill:parent; clip:true
            model: devices
            // Group by OS
            section.property: "os"
            section.delegate: Rectangle { width:ListView.view.width; implicitHeight:22; color:Qt.alpha(osColor(section),0.08)
                Label { anchors{left:parent.left;leftMargin:8;verticalCenter:parent.verticalCenter}; text:osLabel(section); font.family:ThemeEngine.monoFont; font.pixelSize:10; font.weight:Font.Bold; color:osColor(section) } }
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
