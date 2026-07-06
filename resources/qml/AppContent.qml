import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "screens"
import "widgets"
import "theme"

// ── Shared production GUI: nav bar + screen stack ─────────────────────
// Used by both main.qml (production) and SimulatorScreen.qml (simulator)

Item {
    id: content
    readonly property alias stackView: stackView
    property bool compact: false // mobile: icons only, right-aligned, no close
    signal closeRequested()

    // ── Single source of truth for tab definitions ───────────────────
    readonly property var tabScreens: ["dashboard","diagnostic","config","report","settings"]
    readonly property var tabComponents: [dashboardComp, diagnosticComp, configComp, reportComp, settingsComp]
    readonly property var tabLabels: [Tr.dashboard, Tr.diagnostics, Tr.config, Tr.report, Tr.settings]

    function switchToTab(idx) {
        if (idx < 0 || idx >= tabScreens.length) return
        for (var i = 0; i < stackView.depth; i++) {
            var item = stackView.get(i)
            if (item && item.objectName === tabScreens[idx]) {
                stackView.pop(item)
                return
            }
        }
        stackView.push(tabComponents[idx].createObject(stackView))
    }

    Component { id: diagnosticComp; DiagnosticScreen { objectName: "diagnostic" } }
    Component { id: dashboardComp;  DashboardScreen  { objectName: "dashboard"  } }
    Component { id: configComp;     ConfigScreen     { objectName: "config"     } }
    Component { id: reportComp;     ReportScreen     { objectName: "report"     } }
    Component { id: settingsComp;   SettingsScreen   { objectName: "settings"   } }

    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // ── Screen stack (fills remaining space above the dock) ──────
        StackView {
            id: stackView
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            initialItem: diagnosticComp
        }

        // ── Bottom dock navigation bar ───────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: compact ? 32 : 36
            color: "#1A1A2E"
            // Drag handle for frameless window (Qt.FramelessWindowHint)
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPositionChanged: function(mouse) {
                    if (mouse.buttons & Qt.LeftButton) {
                        var win = content.Window.window
                        if (win && typeof win.startSystemMove === "function")
                            win.startSystemMove()
                    }
                }
            }
            RowLayout {
                anchors { fill: parent; leftMargin: compact ? 0 : 8; rightMargin: compact ? 4 : 8 }
                Item { Layout.fillWidth: true }
                Row { spacing: compact ? 0 : 2
                    Repeater {
                        model: [
                            { screen: "dashboard",  icon: "dashboard" },
                            { screen: "diagnostic", icon: "diagnostics" },
                            { screen: "config",     icon: "config" },
                            { screen: "report",     icon: "report" },
                            { screen: "settings",   icon: "settings" }
                        ]
                        delegate: ItemDelegate {
                            id: navBtn
                            property bool active: stackView.currentItem && stackView.currentItem.objectName === modelData.screen
                            property string labelText: {
                                Tr.lang // force re-evaluation on language change
                                return content.tabLabels[index] || modelData.screen
                            }
                            implicitWidth: compact ? 44 : 100; implicitHeight: 32
                            background: Rectangle {
                                color: navBtn.active ? Qt.alpha(Theme.cyan, 0.12) : "transparent"
                                radius: 6
                            }
                            contentItem: Item {
                                // Compact (mobile): icon only, brighter color
                                AppIcon {
                                    visible: content.compact
                                    anchors.centerIn: parent
                                    name: modelData.icon; size: 14
                                    color: navBtn.active ? Theme.cyan : Qt.alpha(Theme.textPrimary, 0.55)
                                }
                                // Desktop: icon + text, brighter color
                                RowLayout {
                                    visible: !content.compact
                                    anchors.centerIn: parent; spacing: 4
                                    AppIcon {
                                        name: modelData.icon; size: 12
                                        color: navBtn.active ? Theme.cyan : Qt.alpha(Theme.textPrimary, 0.55)
                                    }
                                    Label {
                                        text: navBtn.labelText
                                        font.family: Theme.monoFont; font.pixelSize: 10
                                        font.weight: navBtn.active ? Font.DemiBold : Font.Normal
                                        color: navBtn.active ? Theme.cyan : Qt.alpha(Theme.textPrimary, 0.7)
                                    }
                                }
                            }
                            onClicked: switchToTab(index)
                        }
                    }
                }
                Item { width: compact ? 0 : 12; visible: !compact }
                Rectangle {
                    visible: !compact
                    implicitWidth: 28; implicitHeight: 28; radius: 6
                    color: "transparent"; border { width: 1; color: "#5A5A7A" }
                    AppIcon { anchors.centerIn: parent; name: "close"; size: 14; color: Qt.alpha(Theme.textPrimary, 0.7) }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: content.closeRequested() }
                }
            }
        }
    }
}
