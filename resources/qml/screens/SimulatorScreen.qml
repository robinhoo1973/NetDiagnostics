import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"

ApplicationWindow {
    id: page
    objectName: "simulator"
    title: "NetAnalysis Simulator"
    visible: true
    color: "#2D2D2D"
    property int currentTab: 1

    Component.onCompleted: {
        showFullScreen()
        if (visibility !== Window.FullScreen) {
            width = Screen.desktopAvailableWidth * 0.9
            height = Screen.desktopAvailableHeight * 0.9
            x = (Screen.desktopAvailableWidth - width) / 2
            y = (Screen.desktopAvailableHeight - height) / 2
        }
    }

    FontLoader { source: "qrc:/fonts/JetBrainsMono-Regular.ttf" }
    FontLoader { source: "qrc:/fonts/JetBrainsMono-Bold.ttf" }

    property var devices: [
        { id:"ip15", name:"iPhone 15 Pro",  os:"ios",     w:393, h:852, bezel:4, island:true,  radius:47 },
        { id:"se",   name:"iPhone SE",      os:"ios",     w:375, h:667, bezel:8, island:false, radius:20 },
        { id:"px8",  name:"Pixel 8 Pro",    os:"android", w:412, h:915, bezel:4, island:false, radius:28 },
        { id:"dt",   name:"Generic Desktop", os:"linux",  w:800, h:500, bezel:0, island:false, radius:8  }
    ]
    property int currentDevice: 0
    property bool portrait: true
    onPortraitChanged: scaled.scale = calcScale()
    onCurrentDeviceChanged: scaled.scale = calcScale()

    function cur() { return devices[currentDevice] }
    function osColor(os) {
        if (os==="ios") return "#007AFF"
        if (os==="android") return "#3DDC84"
        if (os==="linux") return "#FCC624"
        return "#888"
    }

    // ── Layout ────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // Top area: AppBar + NavBar
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 88; color: "#1A1A2E"; z: 10
            ColumnLayout {
                anchors.fill: parent; spacing: 0
                // AppBar
                RowLayout {
                    Layout.fillWidth: true; Layout.preferredHeight: 48
                    anchors { leftMargin: 16; rightMargin: 12 }
                    Label { text:"NetAnalysis Simulator"; font.family:"JetBrains Mono"; font.pixelSize:16; font.weight:Font.DemiBold; color:"white" }
                    Item { Layout.fillWidth:true }
                    // Device selector
                    Rectangle { implicitWidth:160; implicitHeight:34; radius:4; color:Qt.alpha("white",0.08); border{width:1;color:Qt.alpha("white",0.15)}
                        RowLayout { anchors.fill:parent; anchors.margins:8
                            Label { Layout.fillWidth:true; text:cur().name; font.family:"JetBrains Mono"; font.pixelSize:12; color:"white" }
                            Label { text:"▾"; font.pixelSize:14; color:Qt.alpha("white",0.6) }
                        }
                        MouseArea { anchors.fill:parent; onClicked:devicePopup.open() }
                    }
                    Item { width:8 }
                    Rectangle { implicitWidth:34; implicitHeight:34; radius:4; color:"transparent"; border{width:1;color:Qt.alpha("white",0.2)}
                        Label { anchors.centerIn:parent; text:portrait?"↻":"↕"; font.pixelSize:18; color:Qt.alpha("white",0.7) }
                        MouseArea { anchors.fill:parent; onClicked:portrait=!portrait }
                    }
                }
                // NavBar
                RowLayout {
                    Layout.fillWidth:true; Layout.preferredHeight:40
                    anchors { leftMargin:16; rightMargin:16 }
                    Repeater {
                        model: ["Dashboard","Diagnostics","Config","Report","Settings"]
                        delegate: ItemDelegate {
                            implicitWidth:90; implicitHeight:36
                            background: Rectangle { color:index===page.currentTab?Qt.alpha(Theme.cyan,0.12):"transparent"; radius:8 }
                            contentItem: Label { anchors.centerIn:parent; text:modelData; font.family:"JetBrains Mono"; font.pixelSize:10; color:index===page.currentTab?Theme.cyan:Theme.textSecondary }
                            onClicked: page.currentTab=index
                        }
                    }
                }
            }
        }

        // Body: scaled device frame
        Item {
            id: body
            Layout.fillWidth: true; Layout.fillHeight: true

            function calcScale() {
                var d = cur()
                var sw = portrait ? d.w : d.h
                var sh = portrait ? d.h : d.w
                var tw = sw + d.bezel*2 + 16
                var th = sh + d.bezel*2 + 16 + (d.island?30:0)
                return Math.max(0.1, Math.min(3.0, Math.min((width-8)/tw, (height-8)/th)))
            }

            Item {
                id: scaled
                anchors.centerIn: parent
                property real scale: body.calcScale()
                width: deviceFrame.width * scale
                height: deviceFrame.height * scale

                Rectangle {
                    id: deviceFrame
                    property var d: cur()
                    property real sw: portrait ? d.w : d.h
                    property real sh: portrait ? d.h : d.w
                    property real bh: d.bezel
                    width: sw + bh*2
                    height: sh + bh*2 + (d.island?30:0)
                    radius: d.radius + bh
                    color: d.bezel>0 ? "#0A0A0A" : "transparent"
                    border { width:d.bezel>0?0:1; color:"#333" }

                    // Screen area
                    Rectangle {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: deviceFrame.d.island ? -15 : 0
                        width: deviceFrame.sw; height: deviceFrame.sh
                        radius: deviceFrame.d.radius
                        color: "#1E1E2E"
                        clip: true

                        // Status bar
                        Rectangle {
                            anchors { top:parent.top; left:parent.left; right:parent.right }
                            height: 24; color: "#1A1A2E"
                            RowLayout {
                                anchors { fill:parent; leftMargin:16; rightMargin:12 }
                                Label { text:"9:41"; font.family:"JetBrains Mono"; font.pixelSize:10; color:"#A0A0B8" }
                                Item { Layout.fillWidth:true }
                                Label { text:"●●●●○"; font.pixelSize:8; color:"#4ADE80" }
                            }
                        }

                        // Page stack
                        StackView {
                            id: simStack
                            anchors.fill: parent
                            anchors.topMargin: 24
                            Component.onCompleted: push("../screens/DiagnosticScreen.qml")

                            Connections {
                                target: page
                                function onCurrentTabChanged() {
                                    var screens = ["../screens/DashboardScreen.qml","../screens/DiagnosticScreen.qml","../screens/ConfigScreen.qml","../screens/ReportScreen.qml","../screens/SettingsScreen.qml"]
                                    if (page.currentTab >=0 && page.currentTab < screens.length) {
                                        simStack.clear()
                                        simStack.push(screens[page.currentTab])
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Device popup ───────────────────────────────────────────────────
    Popup {
        id: devicePopup
        y: 90; x: Math.max(page.width - 300, 0)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: 260; height: Math.min(200, devices.length * 46 + 8); padding: 6
        background: Rectangle { radius:10; color:"#1E1E2E"; border{width:1;color:"#3A3A5A"} }
        ListView {
            anchors.fill:parent; clip:true; model:devices
            delegate: ItemDelegate {
                width:ListView.view.width; implicitHeight:44
                contentItem: RowLayout { spacing:8
                    Label { text:modelData.name; font.family:"JetBrains Mono"; font.pixelSize:13; color:"white"; Layout.fillWidth:true }
                    Rectangle { implicitWidth:32; implicitHeight:20; radius:4; color:Qt.alpha(osColor(modelData.os),0.15)
                        Label { anchors.centerIn:parent; text:modelData.os.substring(0,1).toUpperCase(); font.family:"JetBrains Mono"; font.pixelSize:10; color:"white" }
                    }
                }
                background: Rectangle { color:index===currentDevice?Qt.alpha("#0078D4",0.2):"transparent"; radius:6 }
                onClicked: { currentDevice=index; devicePopup.close() }
            }
        }
    }
}
