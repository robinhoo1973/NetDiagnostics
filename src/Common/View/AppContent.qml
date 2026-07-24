import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "screens"
import "widgets"
import "theme"

// ── Shared production GUI: nav bar + screen stack ─────────────────────
// Used by main.qml (production)

Item {
    id: content
    readonly property alias stackView: stackView
    property bool compact: false // mobile: icons only, right-aligned, no close
    property bool navBlocked: (stackView.currentItem && stackView.currentItem.overlayVisible === true) || false
    signal closeRequested()

    // 5WHY: Nav buttons were disabled when overlays were open, preventing
    // navigation.  Users expect nav taps to dismiss overlays like tapping
    // the backdrop does.  Close any open overlay on the active screen.
    function closeCurrentOverlay() {
        var item = stackView.currentItem
        if (!item) return
        // Close detail overlay (DiagnosticScreen)
        if (item.detailOverlay && item.detailOverlay.visible) item.detailOverlay.visible = false
        // Close preview overlay (ReportScreen / DashboardScreen)
        if (typeof item.previewVisible !== 'undefined' && item.previewVisible) item.previewVisible = false
        // Close share dialog
        if (typeof item.shareStage !== 'undefined' && item.shareStage !== 0) item.shareStage = 0
        // Close cellular warning
        if (appState.cellularWarnVisible) appState.cellularWarnVisible = false
    }

    // ── Single source of truth for tab definitions ───────────────────
    readonly property var tabScreens: ["dashboard","diagnostic","config","settings"]
    readonly property var tabComponents: [dashboardComp, diagnosticComp, configComp, settingsComp]
    readonly property var tabLabels: [Tr.dashboard, Tr.diagnostics, Tr.config, Tr.settings]

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

        // ── Bottom dock navigation bar (Material Design 3 compliant) ──
        Rectangle {
            Layout.fillWidth: true
            // M3: 80dp full, 56dp compact desktop.  Apple HIG: 44-48pt mobile.
            implicitHeight: compact ? 48 : 56
            color: ThemeEngine.colors.navBar
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
                anchors { fill: parent; leftMargin: compact ? 0 : 16; rightMargin: compact ? 4 : 16 }
                // Nav items centered via balanced left+right Layout.fillWidth spacers
                Item { Layout.fillWidth: true }
                // M3 spec: 8dp minimum gap between touch targets. 4dp for same-group icons.
                // 5WHY: compact spacing was 0 — adjacent 48dp touch targets with zero
                // separation cause mis-taps on narrow mobile screens.
                Row { spacing: compact ? 4 : 4
                    Repeater {
                        model: [
                            { screen: "dashboard",  icon: "dashboard" },
                            { screen: "diagnostic", icon: "diagnostics" },
                            { screen: "config",     icon: "config" },
                            { screen: "settings",   icon: "gear" }
                        ]
                        delegate: ItemDelegate {
                            id: navBtn
                            property bool active: stackView.currentItem && stackView.currentItem.objectName === modelData.screen
                            property string labelText: {
                                Tr.lang // force re-evaluation on language change
                                return content.tabLabels[index] || modelData.screen
                            }
                            // M3: icon 24dp + gap 8dp + text + padding 12dp each side
                            implicitWidth: compact ? 48
                                : Math.max(80, labelMetrics.width + 24 + 8 + 24)
                            // M3 touch target: 48dp minimum
                            implicitHeight: compact ? 48 : 44
                            TextMetrics {
                                id: labelMetrics
                                font.family: ThemeEngine.monoFont; font.pixelSize: 12
                                text: navBtn.labelText
                            }
                            background: Rectangle {
                                color: navBtn.active ? Qt.alpha(ThemeEngine.colors.primary, 0.12) : "transparent"
                                radius: ThemeEngine.radius.md
                            }
                            contentItem: Item {
                                // Compact (mobile): M3 24dp icon, 48dp touch target
                                AppIcon {
                                    visible: content.compact
                                    anchors.centerIn: parent
                                    name: modelData.icon; size: 24
                                    color: navBtn.active ? ThemeEngine.colors.primary
                                                          : ThemeEngine.colors.textSecondary
                                }
                                // Desktop: M3 icon 24dp + label 12sp, 8dp gap
                                RowLayout {
                                    visible: !content.compact
                                    anchors.centerIn: parent; spacing: 8
                                    AppIcon {
                                        name: modelData.icon; size: 24
                                        color: navBtn.active ? ThemeEngine.colors.primary
                                                              : ThemeEngine.colors.textSecondary
                                    }
                                    Label {
                                        text: navBtn.labelText
                                        font.family: ThemeEngine.monoFont; font.pixelSize: 12
                                        font.weight: navBtn.active ? Font.DemiBold : Font.Normal
                                        color: navBtn.active ? ThemeEngine.colors.primary
                                                              : ThemeEngine.colors.textSecondary
                                    }
                                }
                            }
                            onClicked: {
                                if (navBlocked) { closeCurrentOverlay(); return }
                                switchToTab(index)
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Item { width: compact ? 0 : 4; visible: !compact }
            }
        }
    }
}
