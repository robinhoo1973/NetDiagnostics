import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"
import "../theme"
import "../utils"
import "../"

ApplicationWindow {
    id: page
    objectName: "simulator"
    title: "NetDiagnostics Simulator"
    visible: true
    color: ThemeEngine.bgDark

    // Maximize to fill the current screen's available area.
    // onWidthChanged/onHeightChanged handlers will trigger recalcScale()
    // after the window manager assigns the final geometry.
    visibility: Window.Maximized

    AppFonts {}

    property var devices: [
        // ── Desktop ──────────────────────────────────────────────────
        { id:"win-x64",   name:"Windows 11 (x64)",     os:"windows", w:1024,h:640, bezel:0, island:false, radius:8 },
        { id:"win-arm64", name:"Windows 11 (ARM64)",   os:"windows", w:1024,h:640, bezel:0, island:false, radius:8 },
        { id:"lx-x64",    name:"Ubuntu 24.04 (x64)",   os:"linux",   w:1024,h:640, bezel:0, island:false, radius:8 },
        { id:"lx-arm64",  name:"Ubuntu 24.04 (ARM64)", os:"linux",   w:1024,h:640, bezel:0, island:false, radius:8 },
        { id:"mac-x64",   name:"macOS 15 (x64)",       os:"macos",   w:1024,h:640, bezel:0, island:false, radius:8 },
        { id:"mac-arm64", name:"macOS 15 (ARM64)",     os:"macos",   w:1024,h:640, bezel:0, island:false, radius:8 },
        // ── iOS ─────────────────────────────────────────────────────
        { id:"ios-iphone-se",    name:"iPhone SE (3rd gen)",   os:"ios", w:375, h:667,  bezel:8,  island:false, radius:20 },
        { id:"ios-iphone15",     name:"iPhone 15 Pro",         os:"ios", w:393, h:852,  bezel:12, island:true,  radius:55 },
        { id:"ios-iphone15pm",   name:"iPhone 15 Pro Max",     os:"ios", w:430, h:932,  bezel:12, island:true,  radius:55 },
        { id:"ios-iphone16",     name:"iPhone 16",             os:"ios", w:402, h:874,  bezel:11, island:true,  radius:55 },
        { id:"ios-iphone16pm",   name:"iPhone 16 Pro Max",     os:"ios", w:440, h:956,  bezel:11, island:true,  radius:55 },
        { id:"ios-ipad-mini",    name:"iPad mini (6th gen)",   os:"ios", w:744, h:1133, bezel:14, island:false, radius:16 },
        { id:"ios-ipadpro11",    name:"iPad Pro 11″ (M4)",     os:"ios", w:834, h:1210, bezel:14, island:false, radius:18 },
        { id:"ios-ipadpro13",    name:"iPad Pro 12.9″ (M4)",   os:"ios", w:1024,h:1366, bezel:14, island:false, radius:18 },
        // ── Android ─────────────────────────────────────────────────
        { id:"android-pixel8",   name:"Pixel 8 (Android 14)",  os:"android", w:412, h:915,  bezel:8,  island:false, radius:20 },
        { id:"android-pixel9",   name:"Pixel 9 Pro (Android 15)", os:"android", w:448, h:1008, bezel:7,  island:false, radius:20 },
        { id:"android-s24",      name:"Galaxy S24 (Android)",  os:"android", w:360, h:780,  bezel:6,  island:false, radius:16 },
        { id:"android-s24u",     name:"Galaxy S24 Ultra (Android)", os:"android", w:384, h:854, bezel:6, island:false, radius:16 },
        { id:"android-oneplus",  name:"OnePlus 12 (Android)",  os:"android", w:412, h:917,  bezel:7,  island:false, radius:18 }
    ]
    property int currentDevice: 0
    property bool portrait: true
    onPortraitChanged: recalcScale()
    onCurrentDeviceChanged: recalcScale()
    onWidthChanged: recalcScale()
    onHeightChanged: recalcScale()

    // Polling fallback — ARM64 Qt signals may be dropped; gate to Linux only
    readonly property bool needsScalePolling: Qt.platform.os === "linux"
    Timer { interval: 200; running: needsScalePolling; repeat: true; onTriggered: recalcScale() }

    function recalcScale() {
        var w = page.width; var h = page.height - 48
        var p = portrait; var d = cur()
        if (w <= 0 || h <= 0 || !d) { return }
        var desktop = isDesktop()
        var dev_sw = p ? d.w : d.h
        var dev_sh = p ? d.h : d.w
        var dev_bh = desktop ? 0 : (d.bezel || 0)
        var fw = dev_sw + dev_bh*2
        var fh = dev_sh + dev_bh*2 + 36
	var s = Math.max(0.1, Math.min((w-16)/fw, (h-16)/fh))
        // All dimensions at natural size — Scale transform handles visual scaling
        deviceFrame.width = fw; deviceFrame.height = fh
        deviceFrame.radius = desktop ? d.radius+2 : d.radius+dev_bh
        deviceFrame.color = desktop ? ThemeEngine.bgDark : "#0A0A0A"
        deviceFrame.border.width = desktop ? 1 : 0
        screenRect.x = dev_bh; screenRect.y = dev_bh
        screenRect.width = dev_sw; screenRect.height = dev_sh + 36
        screenRect.radius = d.radius
        // Natural size — Scale transform handles visual
        scaled.width = fw; scaled.height = fh
        scaled.scale = s
        scaled.x = (page.width - fw * s) / 2
        scaled.y = 0
    }

    function cur() { return devices[currentDevice] }
    function isMobile() { var d = cur(); return d ? (d.os === "ios" || d.os === "android") : false }
    function isDesktop(){ return !isMobile() }

    // OS metadata lookup — replaces osIcon/osColor/osLabel functions
    readonly property var osMeta: ({
        linux:   { icon:"linux",   color:"#FCC624", label:"Linux"   },
        windows: { icon:"windows", color:"#00A4EF", label:"Windows" },
        macos:   { icon:"apple",   color:"#007AFF", label:"macOS"   },
        ios:     { icon:"apple",   color:"#007AFF", label:"iOS"     },
        android: { icon:"android", color:"#3DDC84", label:"Android" }
    })
    function osIcon(os)  { var m=osMeta[os]; return m ? m.icon  : "circle" }
    function osColor(os) { var m=osMeta[os]; return m ? m.color : "#888"   }
    function osLabel(os) { var m=osMeta[os]; return m ? m.label : os       }

    // ── Outer layout ────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // Top bar — device selector
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 48; color: ThemeEngine.colors.navBar; z: 10
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 12 }
                Label { text:"NetDiagnostics Simulator"; font.family:ThemeEngine.monoFont; font.pixelSize:16; font.weight:Font.DemiBold; color:"white" }
                Item { Layout.fillWidth:true }
                Rectangle { implicitWidth:180; implicitHeight:34; radius:4; color:Qt.alpha("white",0.08); border{width:1;color:Qt.alpha("white",0.15)}
                    RowLayout { anchors.fill:parent; anchors.margins:8
                        AppIcon { name:osIcon(cur().os); size:14; color:osColor(cur().os) }
                        Label { Layout.fillWidth:true; text:cur().name; font.family:ThemeEngine.monoFont; font.pixelSize:12; color:"white" }
                        Label { text:"▾"; font.pixelSize:14; color:Qt.alpha("white",0.6) }
                    }
                    MouseArea { anchors.fill:parent; onClicked:devicePopup.open() }
                }
                Item { width:8 }
                Rectangle { implicitWidth:34; implicitHeight:34; radius:4; color:"transparent"; border{width:1;color:Qt.alpha("white",0.2)}
                    AppIcon { anchors.centerIn:parent; name:"refresh"; size:16; color:Qt.alpha("white",0.7) }
                    MouseArea { anchors.fill:parent; onClicked:portrait=!portrait }
                }
            }
        }

        // Body — Rectangle (not Item) ensures Layout assigns proper width/height
        Rectangle {
            id: body; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"
            Item {
                id: scaled
                property real scale: 0.5
                transform: Scale { xScale: scaled.scale; yScale: scaled.scale }

                Rectangle {
                    id: deviceFrame
                    color: "#0A0A0A"
                    border { width: 0; color: "#444" }

                    // ── Screen ──────────────────────────────────────────
                    Rectangle {
                        id: screenRect
                        color: ThemeEngine.bgDark; clip: true

                        // ── Production GUI (shared with main.qml) ────────────────
                        AppContent {
                            id: appContent
                            anchors.fill: parent
                            onCloseRequested: close()
                            compact: isMobile()
                        }
                    }
                }
            }
        }
    }

    // ── Device popup ───────────────────────────────────────────────────
    Popup {
        id: devicePopup
        y: 90; x: Math.max(page.width-310,0)
        closePolicy: Popup.CloseOnEscape|Popup.CloseOnPressOutside
        width: 290; height: Math.min(450, devices.length*54+16); padding: 8
        background: Rectangle { radius:10; color:ThemeEngine.bgDark; border{width:1;color:ThemeEngine.colors.borderCard} }
        ListView {
            anchors.fill:parent; clip:true; model:devices
            delegate: ItemDelegate {
                width:ListView.view.width; implicitHeight:50
                contentItem: RowLayout { spacing:10
                    Rectangle { implicitWidth:32; implicitHeight:32; radius:8; color:Qt.alpha(osColor(modelData.os),0.15)
                        AppIcon { anchors.centerIn:parent; name:osIcon(modelData.os); size:18; color:osColor(modelData.os) } }
                    ColumnLayout { spacing:1
                        Label { text:modelData.name; font.family:ThemeEngine.monoFont; font.pixelSize:13; color:"white" }
                        RowLayout { spacing:6
                            Rectangle { implicitWidth:44; implicitHeight:16; radius:3; color:Qt.alpha(osColor(modelData.os),0.2)
                                Label { anchors.centerIn:parent; text:osLabel(modelData.os); font.family:ThemeEngine.monoFont; font.pixelSize:8; font.weight:Font.DemiBold; color:osColor(modelData.os) } }
                            Label { text:(modelData.w)+"×"+(modelData.h); font.family:ThemeEngine.monoFont; font.pixelSize:9; color:Qt.alpha("white",0.4) }
                        }
                    }
                }
                background: Rectangle { color:index===currentDevice?Qt.alpha("#0078D4",0.2):"transparent"; radius:6 }
                onClicked: { currentDevice=index; devicePopup.close() }
            }
        }
    }
}