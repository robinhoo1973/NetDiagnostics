import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"

// ── Flutter DiagnosticMainScreen 1:1 ───────────────────────────────────
Item {
    id: page
    objectName: "diagnostic"
    readonly property bool wide: width >= 600

    // ── Polled state (C++ signals are unreliable on ARM64) ────────────
    property int _runStatus: 0
    property int _totalCompleted: 0
    property int _totalTests: 0
    Timer {
        interval: 200; running: true; repeat: true
        onTriggered: {
            _runStatus = appState.runStatus
            _totalCompleted = appState.totalCompleted
            _totalTests = appState.totalTests
        }
    }

    // Filter groups: only show those with enabled tests or results
    property var currentDetail: ({})
    property var visibleGroups: {
        var _force = _totalCompleted + _runStatus
        var g = []
        for (var i = 0; i < appState.groupLabels.length; i++) {
            var s = appState.groupStats(i)
            if (s.enabled > 0 || s.total > 0) g.push(i)
        }
        return g
    }

    // ── Wide: Row[ Sidebar(260) | Divider(1) | Content(flex) ] ─────
    RowLayout {
        anchors.fill: parent; spacing: 0
        visible: page.wide

        Rectangle {
            Layout.preferredWidth: 260; Layout.fillHeight: true
            color: Theme.bgSidebar; clip: true
            SidebarContent { anchors.fill: parent; compact: false }
        }
        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#3A3A5A" }
        ContentArea { Layout.fillWidth: true; Layout.fillHeight: true }
    }

    // ── Narrow: Column[ Sidebar(flex) | Divider | Content(flex) ] ──
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        visible: !page.wide

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: _totalCompleted > 0 ? 0.29 * page.height : 0.5 * page.height
            color: Theme.bgSidebar; clip: true
            Flickable {
                anchors.fill: parent; contentHeight: sidebarCol.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ColumnLayout { id: sidebarCol; width: parent.width
                    SidebarContent { width: parent.width; compact: _totalCompleted > 0 }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#3A3A5A" }
        ContentArea { Layout.fillWidth: true; Layout.fillHeight: true }
    }

    // ═══════════════════ SIDEBAR ═══════════════════
    component SidebarContent: ColumnLayout {
        property bool compact: false
        spacing: 0

        // Header — matches Flutter Container(padding h16 v14, border bottom #3A3A5A)
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 48
            color: "transparent"
            border { width: 1; color: "#3A3A5A" }
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                AppIcon { name: "wifi"; size: 20; color: Theme.cyan }
                Item { width: 10 }
                Label { text: "NetAnalysis"; font.family: "JetBrains Mono"; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.textPrimary }
            }
        }

        // Target input + Run
        Item { Layout.preferredHeight: 12 }
        TargetInputPanel { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12 }

        // Layer checkboxes
        Item { Layout.preferredHeight: 8; visible: !compact }
        ColumnLayout {
            visible: !compact; spacing: 2
            Layout.leftMargin: 12; Layout.rightMargin: 12
            Label { text: "Diagnosis Group"; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
            Item { Layout.preferredHeight: 6 }
            Repeater {
                model: appState.groupLabels.length
                delegate: Rectangle {
                    property int gIdx: index
                    readonly property bool canEnable: _runStatus === 1 ? false :
                        (gIdx === 3) ? (appState.target !== "") :
                        (gIdx === 4) ? (appState.target.indexOf("://") >= 0) : true
                    Layout.fillWidth: true; implicitHeight: 32; radius: 6
                    color: appState.isGroupAllEnabled(gIdx) ? Qt.alpha(Theme.accentBlue, 0.12) : "transparent"
                    RowLayout {
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        CheckBox {
                            Layout.preferredWidth: 18; Layout.preferredHeight: 18
                            checkState: appState.isGroupAllEnabled(gIdx) ? Qt.Checked :
                                        appState.isGroupAnyEnabled(gIdx) ? Qt.PartiallyChecked : Qt.Unchecked
                            enabled: canEnable
                            onToggled: if (canEnable) appState.setGroupEnabled(gIdx, checkState === Qt.Checked)
                        }
                        Item { width: 8 }
                        Label {
                            Layout.fillWidth: true
                            text: appState.groupLabels[gIdx] || ""
                            font.family: "JetBrains Mono"; font.pixelSize: 12
                            font.weight: appState.isGroupAllEnabled(gIdx) ? Font.DemiBold : Font.Normal
                            color: canEnable ? (appState.isGroupAllEnabled(gIdx) ? Theme.textPrimary : Theme.textSecondary)
                                            : Qt.alpha(Theme.textSecondary, 0.35)
                        }
                    }
                }
            }
        }

        // Port scan
        Item { Layout.preferredHeight: 8; visible: !compact }
        PortScanConfig { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; visible: !compact }

        // Divider
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 20; color: "transparent"
            visible: !compact
            Rectangle { anchors.centerIn: parent; width: parent.width - 24; height: 1; color: "#3A3A5A" }
        }

        // Target Analysis
        TargetAnalysisPanel {
            Layout.fillWidth: true
            Layout.leftMargin: 12; Layout.rightMargin: 12
            visible: !compact && appState.target !== ""
            target: appState.target
        }

        // Spacer
        Item { Layout.fillHeight: true; visible: !compact }

        // Summary cards
        Rectangle {
            Layout.fillWidth: true; implicitHeight: summaryCards.implicitHeight + 24
            color: "transparent"
            border { width: 1; color: "#3A3A5A" }
            SummaryCards { id: summaryCards; anchors { fill: parent; margins: 12; topMargin: 8; bottomMargin: 16 } }
        }
    }

    // ═══════════════════ CONTENT ═══════════════════
    component ContentArea: ColumnLayout {
        spacing: 0

        // Header bar
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 41
            color: "#1A1A2E"
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                AppIcon { name: "diagnostics"; size: 18; color: Theme.cyan }
                Item { width: 8 }
                Label {
                    text: _runStatus === 1 ? "Running Diagnostics..." :
                          _runStatus === 2 ? "Diagnostic Complete" :
                          _runStatus === 3 ? "Cancelled" :
                          _runStatus === 4 ? "Error — Check Target" : "Results"
                    font.family: "JetBrains Mono"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                Button {
                    visible: _runStatus >= 2 && _totalCompleted > 0
                    implicitHeight: 32
                    text: "Reset"
                    font.family: "JetBrains Mono"; font.pixelSize: 12
                    flat: true
                    background: Rectangle { radius: 6; color: "transparent"; border { width: 1; color: "#5A5A7A" } }
                    contentItem: Label { text: "↻ Reset"; font: parent.font; color: Theme.textSecondary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: appState.reset()
                }
            }
        }

        // Results body
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            Column {
                anchors.centerIn: parent; spacing: 16
                visible: _runStatus === 0 && _totalCompleted === 0
                AppIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "wifi"; size: 80; color: Qt.alpha(Theme.textSecondary, 0.2) }
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Enter a target and press Run"; font.family: "JetBrains Mono"; font.pixelSize: 15; font.weight: Font.Medium; color: Qt.alpha(Theme.textSecondary, 0.6) }
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Analyze your network with a single click"; font.family: "JetBrains Mono"; font.pixelSize: 12; color: Qt.alpha(Theme.textSecondary, 0.4) }
            }
            Flickable {
                anchors { fill: parent; margins: 6 }
                visible: _totalCompleted > 0 || _runStatus === 1
                clip: true
                contentWidth: width
                contentHeight: resultCol.implicitHeight + 8
                Column {
                    id: resultCol; width: parent.width; spacing: 4
                    Repeater {
                        model: visibleGroups
                        delegate: TestGroupPanel {
                            groupIndex: modelData
                            width: resultCol.width
                            onDetailClicked: function(data) {
                                // Use native C++ QDialog — bypasses QML rendering issues on ARM64
                                appState.showDetailDialog(data.testId)
                            }
                        }
                    }
                }
            }
        }

        // Bottom bar
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 40
            color: "#1A1A2E"
            border { width: 1; color: "#3A3A5A" }
            // Polled LiveProgress
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                visible: _runStatus >= 1 && _runStatus <= 4
                spacing: 8
                Label {
                    text: _runStatus === 1 ? "⟳" : _runStatus === 2 ? "✓" : _runStatus === 3 ? "✗" : _runStatus === 4 ? "⚠" : "○"
                    font.pixelSize: 14
                    color: _runStatus === 1 ? Theme.cyan : _runStatus === 2 ? Theme.passGreen : _runStatus === 3 ? Theme.warnYellow : _runStatus === 4 ? Theme.failRed : Qt.alpha(Theme.textSecondary, 0.4)
                }
                Label {
                    text: _runStatus === 1 ? "Running" : _runStatus === 2 ? "Complete" : _runStatus === 3 ? "Cancelled" : _runStatus === 4 ? "Error" : "Ready"
                    font.family: "JetBrains Mono"; font.pixelSize: 12; font.weight: Font.DemiBold
                    color: _runStatus === 1 ? Theme.cyan : _runStatus === 2 ? Theme.passGreen : _runStatus === 3 ? Theme.warnYellow : _runStatus === 4 ? Theme.failRed : Theme.textSecondary
                }
                AppIcon { visible: appState.errorMessage !== ""; name: "warning"; size: 14; color: Theme.failRed }
                Item { Layout.fillWidth: true }
                Label { visible: _runStatus === 1; text: appState.currentTestLabel || ""; font.family: "JetBrains Mono"; font.pixelSize: 11; font.italic: true; color: Theme.cyan; elide: Text.ElideRight; Layout.maximumWidth: 300 }
                Label { visible: _totalTests > 0; text: _totalCompleted + " / " + _totalTests; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
            }
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                visible: _runStatus === 0
                AppIcon { name: "circle"; size: 14; color: Theme.textSecondary }
                Item { width: 8 }
                Label { text: "Ready"; font.family: "JetBrains Mono"; font.pixelSize: 12; font.weight: Font.DemiBold; color: Theme.textSecondary }
            }
        }
    }

    // ── Detail popup ──────────────────────────────────────────────────
    Rectangle {
        id: detailOverlay
        anchors.fill: parent
        color: "#88000000"
        visible: false; z: 100

        MouseArea {
            anchors.fill: parent
            onClicked: detailOverlay.visible = false
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(600, parent.width - 20)
            height: Math.min(parent.height - 40, 600)
            radius: 14
            color: "#252538"
            border { width: 1.5; color: "#4A4A6A" }

            Rectangle {
                anchors { top: parent.top; right: parent.right; topMargin: 8; rightMargin: 8 }
                width: 24; height: 24; radius: 12; color: "#FF4444"
                Label { anchors.centerIn: parent; text: "✕"; font.pixelSize: 12; font.weight: Font.Bold; color: "white" }
                MouseArea { anchors.fill: parent; onClicked: detailOverlay.visible = false }
            }

            Column {
                anchors { fill: parent; margins: 16; topMargin: 36 }
                spacing: 8
                Label { id: dtTitle; text: ""; font.family: "JetBrains Mono"; font.pixelSize: 16; font.weight: Font.DemiBold; color: "#FFFFFF"; elide: Text.ElideRight }
                Label { id: dtStatus; text: ""; font.family: "JetBrains Mono"; font.pixelSize: 12; color: "#A0A0B8" }
                Label { id: dtSummary; text: ""; font.family: "JetBrains Mono"; font.pixelSize: 12; color: "#E0E0E0"; wrapMode: Text.WordWrap }
                Rectangle { implicitWidth: 500; height: 1; color: "#3A3A5A" }
                Repeater {
                    model: currentDetail.properties || []
                    delegate: Row {
                        spacing: 4
                        Label { text: (modelData.label||"?")+":"; font.family:"JetBrains Mono"; font.pixelSize:11; font.weight:Font.DemiBold; color:"#A0A0B8"; width:120 }
                        Label { text: modelData.value||""; font.family:"JetBrains Mono"; font.pixelSize:11; color:"#E0E0E0"; wrapMode:Text.WordWrap }
                    }
                }
                Label { id: dtOutput; text: ""; font.family:"JetBrains Mono"; font.pixelSize:10; color:"#A0A0B8"; wrapMode:Text.WordWrap; visible:text!=="" }
            }
        }
    }
}
