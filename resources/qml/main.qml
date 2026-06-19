import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "screens"
import "widgets"

ApplicationWindow {
    id: root
    title: "NetAnalysis"
    visible: true
    minimumWidth: 360; minimumHeight: 400
    color: Theme.bgDark

    Component.onCompleted: {
        // Start at 80% desktop size, centered — user can resize freely
        var dw = Screen.desktopAvailableWidth
        var dh = Screen.desktopAvailableHeight
        width = Math.max(800, dw * 0.8)
        height = Math.max(500, dh * 0.8)
        x = Math.max(0, (dw - width) / 2)
        y = Math.max(0, (dh - height) / 2)
    }

    FontLoader { id: jbMono; source: "qrc:/fonts/JetBrainsMono-Regular.ttf" }
    FontLoader { id: jbMonoBold; source: "qrc:/fonts/JetBrainsMono-Bold.ttf" }

    // ── App Bar (matches Flutter: bg #1A1A2E, elevation 0) ─────
    // Flutter uses per-screen AppBars; here we provide a global nav bar
    // that is always visible. Screens can include their own title bars
    // below this global nav bar (matching Flutter's Scaffold.appBar pattern).
    header: ToolBar {
        background: Rectangle { color: "#1A1A2E" }
        leftPadding: 16; rightPadding: 16; topPadding: 4; bottomPadding: 4

        RowLayout {
            anchors.fill: parent; spacing: 0
            // Brand
            Row { spacing: 10
                AppIcon { name: "wifi"; size: 20; color: Theme.cyan }
                Label { text: "NetAnalysis"; font.family: "JetBrains Mono"; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.textPrimary }
            }
            Item { Layout.fillWidth: true }
            // Navigation tabs — match Flutter layout
            Row { spacing: 4
                Repeater {
                    model: [
                        { label: "Dashboard",   screen: "dashboard",  icon: "dashboard" },
                        { label: "Diagnostics", screen: "diagnostic", icon: "diagnostics" },
                        { label: "Config",      screen: "config",     icon: "config" },
                        { label: "Report",      screen: "report",     icon: "report" },
                        { label: "Settings",    screen: "settings",   icon: "settings" }
                    ]
                    delegate: ItemDelegate {
                        id: navBtn
                        property bool active: stackView.currentItem && stackView.currentItem.objectName === modelData.screen
                        implicitWidth: 90; implicitHeight: 40
                        background: Rectangle {
                            color: navBtn.active ? Qt.alpha(Theme.cyan, 0.12) : "transparent"
                            radius: 8
                        }
                        contentItem: RowLayout {
                            anchors.centerIn: parent; spacing: 4
                            AppIcon {
                                name: modelData.icon
                                size: 14
                                color: navBtn.active ? Theme.cyan : Qt.alpha(Theme.textSecondary, 0.5)
                            }
                            Label {
                                text: modelData.label
                                font.family: "JetBrains Mono"; font.pixelSize: 10
                                font.weight: navBtn.active ? Font.DemiBold : Font.Normal
                                color: navBtn.active ? Theme.cyan : Qt.alpha(Theme.textSecondary, 0.7)
                            }
                        }
                        onClicked: {
                            for (var i = 0; i < stackView.depth; i++) {
                                var item = stackView.get(i);
                                if (item && item.objectName === modelData.screen) {
                                    stackView.pop(item); return;
                                }
                            }
                            var comp = modelData.screen === "dashboard"  ? dashboardComp :
                                       modelData.screen === "diagnostic" ? diagnosticComp :
                                       modelData.screen === "config"     ? configComp :
                                       modelData.screen === "report"     ? reportComp : settingsComp;
                            if (comp) stackView.push(comp.createObject(stackView))
                        }
                    }
                }
            }
            // Close button
            Item { Layout.preferredWidth: 12 }
            Rectangle {
                implicitWidth: 32; implicitHeight: 32; radius: 6
                color: "transparent"
                border { width: 1; color: "#5A5A7A" }
                Label {
                    anchors.centerIn: parent
                    text: "✕"; font.pixelSize: 16; color: Theme.textSecondary
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }
        }
    }

    // ── Screen stack ───────────────────────────────────────────────────
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: diagnosticComp
    }

    // ── Screen components ──────────────────────────────────────────────
    Component { id: diagnosticComp; DiagnosticScreen { objectName: "diagnostic" } }
    Component { id: dashboardComp;  DashboardScreen  { objectName: "dashboard"  } }
    Component { id: configComp;     ConfigScreen     { objectName: "config"     } }
    Component { id: reportComp;     ReportScreen     { objectName: "report"     } }
    Component { id: settingsComp;   SettingsScreen   { objectName: "settings"   } }
}
