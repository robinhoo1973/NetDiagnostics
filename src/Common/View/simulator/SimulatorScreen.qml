// =============================================================================
// SimulatorScreen.qml — V2 per build/updated-simulator.md
//
// Layout: Toolbar | Viewport + Status Center
//   ┌── Toolbar (Protocol Groups | Run Actions | Capture) ─────────────────┐
//   ├──────────────────────────────┬───────────────────────────────────────┤
//   │  Device / OS Viewport        │  Status Center                        │
//   │                              │  ├ Device/OS                          │
//   │                              │  ├ Current Test + Progress            │
//   │                              │  ├ Result Summary                     │
//   │                              │  ├ Evidence (Last Shot/Rec)           │
//   │                              │  └ Logs                               │
//   └──────────────────────────────┴───────────────────────────────────────┘
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

    property int  currentDevice: 0
    property int  currentOS: 0
    property bool portrait: true
    property string toastMsg: ""
    property int  activeProtocolGroup: -1

    // Device profiles
    property var devices: (typeof simConfig !== 'undefined' && simConfig && simConfig.deviceCount > 0)
        ? simConfig.devices
        : [{ id:"win-x64",name:"Windows 11",os:"windows",w:1024,h:640,bezel:0,island:false,radius:8 },
           { id:"ios-iphone15pm",name:"iPhone 15 Pro Max",os:"ios",w:430,h:932,bezel:12,island:true,radius:55 }]

    property var osProfiles: (typeof simConfig !== 'undefined' && simConfig && simConfig.osCount > 0)
        ? simConfig.osList
        : [{ id:"windows",name:"Windows",platform:"windows",icon:"windows",color:"#00A4EF" },
           { id:"ios",name:"iOS",platform:"ios",icon:"apple",color:"#007AFF" },
           { id:"android",name:"Android",platform:"android",icon:"android",color:"#3DDC84" }]

    function cur()     { return devices[currentDevice] || devices[0] }
    function curOS()   { var d=cur();return d?d.os:"windows" }
    function curDevId(){ var d=cur();return d?d.id:"unknown" }
    function osIcon(os){ var m=osProfiles.find(function(p){return p.id===os||p.platform===os});return m?m.icon:"circle" }
    function osColor(os){var m=osProfiles.find(function(p){return p.id===os||p.platform===os});return m?m.color:"#888" }
    function showToast(msg){toastMsg=msg;toastTimer.restart()}
    Timer{id:toastTimer;interval:3000;onTriggered:toastMsg=""}

    function syncPolicy(){
        var os=curOS()
        if(typeof policyEngine!=='undefined'&&policyEngine&&typeof simConfig!=='undefined'&&simConfig){
            var p=simConfig.policyForPlatform?simConfig.policyForPlatform(os):null
            policyEngine.setActivePlatform(os,p)
        }
        if(typeof simConfig!=='undefined'&&simConfig)simConfig.setActivePlatform(os)
        if(typeof appState!=='undefined'&&appState&&typeof policyEngine!=='undefined'&&policyEngine)
            appState.setSkipRules(policyEngine.toAppStateSkipRules())
    }
    Component.onCompleted:{Qt.callLater(syncPolicy)}
    onCurrentOSChanged:syncPolicy()

    Connections{target:appState
        function onDiagFailed(diagIdInt){if(typeof screenshotSvc!=='undefined'&&screenshotSvc)screenshotSvc.captureOnFailure(diagIdInt,"diag_"+diagIdInt,curOS(),curDevId())}
    }

    // ══════════════════════════════════════════════════════════════════════
    ColumnLayout{anchors.fill:parent;spacing:0
        // ── Toolbar (唯一入口) ──────────────────────────────────────────
        Rectangle{Layout.fillWidth:true;implicitHeight:88;color:ThemeEngine.colors.navBar
            ColumnLayout{anchors{fill:parent;margins:6};spacing:4
                // Row 1: Connect + Device/OS + Target
                RowLayout{spacing:6
                    // 5WHY: 30px touch targets + no keyboard/a11y on simulator toolbar
                    Rectangle{id:connectBtn;implicitWidth:70;implicitHeight:44;radius:6;color:ThemeEngine.passGreen
                        Label{anchors.centerIn:parent;text:"Connect";font.family:ThemeEngine.monoFont;font.pixelSize:10;color:ThemeEngine.textPrimary}
                        MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:showToast("Connected: "+cur().name)}
                        activeFocusOnTab:true;Keys.onPressed:function(e){if(e.key===Qt.Key_Return||e.key===Qt.Key_Space){showToast("Connected: "+cur().name);e.accepted=true}}
                        Accessible{name:"Connect to device";role:Accessible.Button}}
                    Rectangle{id:deviceBtn;implicitWidth:130;implicitHeight:44;radius:6;color:ThemeEngine.bgInput;border{width:1;color:ThemeEngine.colors.borderCard}
                        Label{anchors{left:parent.left;leftMargin:8;verticalCenter:parent.verticalCenter}text:cur().name;font.family:ThemeEngine.monoFont;font.pixelSize:10;color:ThemeEngine.textPrimary}
                        MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:devicePopup.open()}
                        activeFocusOnTab:true;Keys.onPressed:function(e){if(e.key===Qt.Key_Return||e.key===Qt.Key_Space){devicePopup.open();e.accepted=true}}
                        Accessible{name:"Select device: "+cur().name;role:Accessible.Button}}
                    // Target input
                    Rectangle{Layout.fillWidth:true;implicitHeight:30;radius:6;color:ThemeEngine.bgInput;border{width:1;color:ThemeEngine.colors.borderCard}
                        TextField{id:profileTarget;anchors{fill:parent;leftMargin:8;rightMargin:4}
                            font.family:ThemeEngine.monoFont;font.pixelSize:10;color:ThemeEngine.textPrimary
                            placeholderText:"target URL or host";placeholderTextColor:Qt.alpha(ThemeEngine.textSecondary,0.35)
                            verticalAlignment:TextInput.AlignVCenter;background:Item{};text:appState.target;onTextChanged:appState.target=text.trim()}}
                }
                // Row 2: Protocol Groups
                RowLayout{spacing:4
                    Repeater{model:(typeof protoReg!=='undefined'&&protoReg)?protoReg.groups:[]
                        Rectangle{id:pgBtn;implicitWidth:90;implicitHeight:30;radius:6
                            property int idx:index
                            color:activeProtocolGroup===index?Qt.alpha("#38BDF8",0.15):(pgMouse.containsMouse?Qt.alpha(ThemeEngine.colors.primary,0.08):"transparent")
                            border{width:activeProtocolGroup===index?1:0;color:activeProtocolGroup===index?"#38BDF8":"transparent"}
                            RowLayout{anchors{fill:parent;leftMargin:6;rightMargin:6};spacing:4
                                AppIcon{name:modelData.icon;size:14;color:activeProtocolGroup===index?ThemeEngine.primary:ThemeEngine.textSecondary}
                                Label{text:modelData.groupName;font.family:ThemeEngine.monoFont;font.pixelSize:9;color:ThemeEngine.textPrimary;Layout.fillWidth:true;elide:Text.ElideRight}
                            }
                            MouseArea{id:pgMouse;anchors.fill:parent;hoverEnabled:true;cursorShape:Qt.PointingHandCursor
                                onClicked:{activeProtocolGroup=activeProtocolGroup===index?-1:index}
                                onDoubleClicked:{if(typeof protoReg!=='undefined'&&protoReg){appState.target=protoReg.buildTargetForCase(index,0);appState.runDiagnostics();showToast("Running: "+modelData.groupName)}}
                            }
                        }
                    }
                }
                // 5WHY: all action buttons were 30px (below 44px min), no keyboard/a11y
                RowLayout{spacing:6
                    // Run/Stop — primary action
                    Rectangle{id:simRunBtn;implicitWidth:80;implicitHeight:44;radius:6
                        color:appState.runStatus===1?ThemeEngine.failRed:(appState.canRun()?ThemeEngine.accentBlue:Qt.alpha(ThemeEngine.accentBlue,0.3))
                        Label{anchors.centerIn:parent;text:appState.runStatus===1?"■ Stop":"▶ Run";font.family:ThemeEngine.monoFont;font.pixelSize:10;color:ThemeEngine.textPrimary}
                        MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:{if(appState.runStatus===1)appState.cancel();else appState.runDiagnostics()}}
                        activeFocusOnTab:true;Keys.onPressed:function(e){if(e.key===Qt.Key_Return||e.key===Qt.Key_Space){if(appState.runStatus===1)appState.cancel();else appState.runDiagnostics();e.accepted=true}}
                        Accessible{name:appState.runStatus===1?"Stop diagnostics":"Run diagnostics";role:Accessible.Button}}
                    // Run All
                    Rectangle{id:simRunAllBtn;implicitWidth:80;implicitHeight:44;radius:6;color:"transparent";border{width:1;color:ThemeEngine.colors.borderCard}
                        Label{anchors.centerIn:parent;text:"Run All";font.family:ThemeEngine.monoFont;font.pixelSize:9;color:ThemeEngine.textSecondary}
                        MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:{appState.target=profileTarget.text;appState.runDiagnostics()}}
                        activeFocusOnTab:true;Keys.onPressed:function(e){if(e.key===Qt.Key_Return||e.key===Qt.Key_Space){appState.target=profileTarget.text;appState.runDiagnostics();e.accepted=true}}
                        Accessible{name:"Run all diagnostics";role:Accessible.Button}}
                    // Validation
                    Rectangle{id:simValidateBtn;implicitWidth:80;implicitHeight:44;radius:6;color:Qt.alpha(ThemeEngine.passGreen,0.15);border{width:1;color:Qt.alpha(ThemeEngine.passGreen,0.3)}
                        Label{anchors.centerIn:parent;text:"Validation";font.family:ThemeEngine.monoFont;font.pixelSize:9;color:ThemeEngine.passGreen}
                        MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:{appState.target=profileTarget.text;appState.runDiagnostics();showToast("Full Validation started")}}
                        activeFocusOnTab:true;Keys.onPressed:function(e){if(e.key===Qt.Key_Return||e.key===Qt.Key_Space){appState.target=profileTarget.text;appState.runDiagnostics();showToast("Full Validation started");e.accepted=true}}
                        Accessible{name:"Run full validation";role:Accessible.Button}}
                    Item{Layout.preferredWidth:16}
                    // Screenshot
                    Rectangle{id:simShotBtn;implicitWidth:70;implicitHeight:44;radius:6;color:Qt.alpha(ThemeEngine.accentBlue,0.12);border{width:1;color:Qt.alpha(ThemeEngine.accentBlue,0.3)}
                        Label{anchors.centerIn:parent;text:"Shot";font.family:ThemeEngine.monoFont;font.pixelSize:10;color:ThemeEngine.accentBlue}
                        MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:{if(typeof screenshotSvc!=='undefined'&&screenshotSvc){var p=screenshotSvc.makeFilename(curOS(),curDevId(),"manual");screenshotSvc.capture(p)}}}
                        activeFocusOnTab:true;Keys.onPressed:function(e){if(e.key===Qt.Key_Return||e.key===Qt.Key_Space){if(typeof screenshotSvc!=='undefined'&&screenshotSvc){var p=screenshotSvc.makeFilename(curOS(),curDevId(),"manual");screenshotSvc.capture(p)}e.accepted=true}}
                        Accessible{name:"Take screenshot";role:Accessible.Button}}
                    // Record toggle
                    Rectangle{id:recBtn;implicitWidth:70;implicitHeight:44;radius:6
                        property bool isRec:typeof screenshotSvc!=='undefined'&&screenshotSvc&&screenshotSvc.recording
                        color:isRec?Qt.alpha(ThemeEngine.failRed,0.15):Qt.alpha(ThemeEngine.passGreen,0.1)
                        border{width:1;color:isRec?Qt.alpha(ThemeEngine.failRed,0.4):Qt.alpha(ThemeEngine.passGreen,0.3)}
                        Label{anchors.centerIn:parent;text:isRec?"■ Stop":"● Rec";font.family:ThemeEngine.monoFont;font.pixelSize:10;color:isRec?ThemeEngine.failRed:ThemeEngine.passGreen}
                        MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:{if(typeof screenshotSvc==='undefined'||!screenshotSvc)return;screenshotSvc.recording?screenshotSvc.stopRecording():screenshotSvc.startRecording()}}
                        activeFocusOnTab:true;Keys.onPressed:function(e){if(e.key===Qt.Key_Return||e.key===Qt.Key_Space){if(typeof screenshotSvc==='undefined'||!screenshotSvc)return;screenshotSvc.recording?screenshotSvc.stopRecording():screenshotSvc.startRecording();e.accepted=true}}
                        Accessible{name:recBtn.isRec?"Stop recording":"Start recording";role:Accessible.Button}}
                    Item{Layout.fillWidth:true}
                    Label{text:Math.round(viewport.scale*100)+"%";font.family:ThemeEngine.monoFont;font.pixelSize:9;color:Qt.alpha(ThemeEngine.colors.textPrimary,0.4)}
                }
            }
        }

        // ── Body: Viewport + Status Center ──────────────────────────────
        RowLayout{Layout.fillWidth:true;Layout.fillHeight:true;spacing:0
            // Viewport
            Rectangle{id:body;Layout.fillWidth:true;Layout.fillHeight:true;color:ThemeEngine.colors.surface;clip:true
                DeviceViewport{id:viewport;objectName:"deviceViewport";deviceProfile:cur();portrait:page.portrait
                    Component.onCompleted:{if(typeof screenshotSvc!=='undefined'&&screenshotSvc)screenshotSvc.setViewport(viewport)}
                    x:Math.max(0,(body.width-frameW*scale)/2);y:Math.max(0,(body.height-frameH*scale)/2)
                    Connections{target:page
                        function onWidthChanged(){viewport.recalcScale(body.width,body.height)}
                        function onHeightChanged(){viewport.recalcScale(body.width,body.height)}
                    }
                }
            }
            // Status Center
            Rectangle{Layout.preferredWidth:240;Layout.fillHeight:true;color:Qt.alpha(ThemeEngine.colors.sidebar,0.3)
                ColumnLayout{anchors{fill:parent;margins:8};spacing:6
                    // Device Status
                    Label{text:"STATUS";font.family:ThemeEngine.monoFont;font.pixelSize:8;font.weight:Font.Bold;color:ThemeEngine.textMuted}
                    RowLayout{spacing:6
                        Rectangle{width:8;height:8;radius:4;color:ThemeEngine.passGreen}
                        Label{text:cur().name+" · "+(appState.runStatus===1?"Running":"Idle");font.family:ThemeEngine.monoFont;font.pixelSize:9;color:ThemeEngine.textPrimary;elide:Text.ElideRight}
                    }
                    // Current Test
                    Item{Layout.preferredHeight:4}
                    Label{text:"CURRENT";font.family:ThemeEngine.monoFont;font.pixelSize:8;font.weight:Font.Bold;color:ThemeEngine.textMuted}
                    Label{text:appState.currentDiagLabel||"idle";font.family:ThemeEngine.monoFont;font.pixelSize:9;color:ThemeEngine.textSecondary;Layout.fillWidth:true;elide:Text.ElideRight}
                    Rectangle{Layout.fillWidth:true;implicitHeight:3;radius:2;color:Qt.alpha(ThemeEngine.colors.borderCard,0.5)
                        Rectangle{width:appState.totalDiags>0?parent.width*appState.totalCompleted/appState.totalDiags:0;height:3;radius:2;color:ThemeEngine.accentBlue}}
                    Label{text:appState.totalCompleted+"/"+appState.totalDiags;font.family:ThemeEngine.monoFont;font.pixelSize:8;color:ThemeEngine.textSecondary}
                    // Result
                    Item{Layout.preferredHeight:4}
                    Label{text:"RESULT";font.family:ThemeEngine.monoFont;font.pixelSize:8;font.weight:Font.Bold;color:ThemeEngine.textMuted}
                    RowLayout{spacing:8
                        Label{text:"P:"+safeStat("pass");font.family:ThemeEngine.monoFont;font.pixelSize:10;color:ThemeEngine.passGreen}
                        Label{text:"F:"+safeStat("fail");font.family:ThemeEngine.monoFont;font.pixelSize:10;color:ThemeEngine.failRed}
                        Label{text:"S:"+safeStat("skip");font.family:ThemeEngine.monoFont;font.pixelSize:10;color:ThemeEngine.skipGray}
                    }
                    function safeStat(k){var s=appState.allGroupStats;if(!s)return 0;return s.reduce(function(sum,r){return sum+(r[k]||0)},0)}
                    // Evidence
                    Item{Layout.preferredHeight:4}
                    Label{text:"EVIDENCE";font.family:ThemeEngine.monoFont;font.pixelSize:8;font.weight:Font.Bold;color:ThemeEngine.textMuted}
                    Label{text:(typeof screenshotSvc!=='undefined'&&screenshotSvc&&screenshotSvc.lastCapturePath)?screenshotSvc.lastCapturePath.split("/").pop():"No shots";font.family:ThemeEngine.monoFont;font.pixelSize:8;color:ThemeEngine.skipGray;elide:Text.ElideMiddle}
                    // Logs
                    Item{Layout.preferredHeight:4}
                    Label{text:"LOGS";font.family:ThemeEngine.monoFont;font.pixelSize:8;font.weight:Font.Bold;color:ThemeEngine.textMuted}
                    ListView{Layout.fillWidth:true;Layout.fillHeight:true;clip:true
                        model:(typeof screenshotSvc!=='undefined'&&screenshotSvc)?screenshotSvc.evidenceLog.slice(0,5):[]
                        delegate:Label{width:ListView.view.width;text:(modelData.trigger||"?")+" · "+(modelData.path||"").split("/").pop();font.family:ThemeEngine.monoFont;font.pixelSize:7;color:ThemeEngine.skipGray;elide:Text.ElideMiddle}
                    }
                }
            }
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // Toast overlay
    Label{id:toastLabel;anchors{bottom:parent.bottom;bottomMargin:16;horizontalCenter:parent.horizontalCenter}
        text:toastMsg;visible:toastMsg!=="";font.family:ThemeEngine.monoFont;font.pixelSize:11;color:ThemeEngine.cyan
        background:Rectangle{radius:6;color:Qt.alpha(ThemeEngine.bgDark,0.85);border{width:1;color:Qt.alpha(ThemeEngine.cyan,0.3)}}
        padding:8;z:1000}

    // Device Popup
    Popup{id:devicePopup;y:44;x:Math.max(page.width-340,0);closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        width:280;height:Math.min(400,devices.length*48+16);padding:8
        background:Rectangle{radius:10;color:ThemeEngine.bgDark;border{width:1;color:ThemeEngine.colors.borderCard}}
        ListView{anchors.fill:parent;clip:true;model:devices
            delegate:ItemDelegate{width:ListView.view.width;implicitHeight:42
                contentItem:RowLayout{spacing:8
                    AppIcon{name:osIcon(modelData.os);size:16;color:osColor(modelData.os)}
                    ColumnLayout{spacing:0
                        Label{text:modelData.name;font.family:ThemeEngine.monoFont;font.pixelSize:11;color:ThemeEngine.textPrimary}
                        Label{text:(modelData.w)+"×"+(modelData.h);font.family:ThemeEngine.monoFont;font.pixelSize:8;color:Qt.alpha(ThemeEngine.textSecondary,0.5)}
                    }
                }
                background:Rectangle{color:index===currentDevice?Qt.alpha("#0078D4",0.15):"transparent";radius:6}
                onClicked:{currentDevice=index;devicePopup.close()}
            }
        }
    }
}