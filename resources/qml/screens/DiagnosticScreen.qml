import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"
import "../theme"

// ── Flutter DiagnosticMainScreen 1:1 ───────────────────────────────────
Item {
    id: page
    objectName: "diagnostic"
    readonly property bool wide: width >= 600

    // ── Version-based state sync (single Timer replaces 4 polling timers) ──
    property int _cachedGen: -1
    property int _runStatus: 0
    property int _totalCompleted: 0
    property int _totalDiags: 0
    property bool _runActive: false       // true while runStatus === Running

    // ── Config snapshot — frozen at Run click, released on Complete/Cancel/Error ──
    property string _snapIconName: "circle"
    property color _snapIconColor: Qt.alpha(Theme.textSecondary, 0.4)
    property string _snapTargetError: ""
    property bool _snapG0chk: true
    property bool _snapG1chk: true
    property bool _snapG2chk: true
    property bool _snapG3en: false
    property bool _snapG3chk: false
    property bool _snapG4en: false
    property bool _snapG4chk: false
    // Incremented by takeSnapshot so SidebarContent can detect changes
    property int _snapVersion: 0

    function takeSnapshot() {
        _snapG0chk = appState.isGroupAllEnabled(0) || appState.isGroupAnyEnabled(0)
        _snapG1chk = appState.isGroupAllEnabled(1) || appState.isGroupAnyEnabled(1)
        _snapG2chk = appState.isGroupAllEnabled(2) || appState.isGroupAnyEnabled(2)
        _snapG3chk = appState.isGroupAllEnabled(3) || appState.isGroupAnyEnabled(3)
        _snapG4chk = appState.isGroupAllEnabled(4) || appState.isGroupAnyEnabled(4)
        _snapG3en  = !appState.isTargetEmpty()
        _snapG4en  = appState.isTargetUrl()

        var err = appState.targetValidationError()
        if (err !== "") {
            _snapIconName = "error"
            _snapIconColor = Theme.failRed
        } else if (appState.isTargetEmpty()) {
            _snapIconName = "circle"
            _snapIconColor = Qt.alpha(Theme.textSecondary, 0.4)
        } else if (appState.isTargetUrl()) {
            _snapIconName = "globe"
            _snapIconColor = Theme.accentBlue
        } else {
            _snapIconName = "target"
            _snapIconColor = Theme.passGreen
        }
        _snapTargetError = err
        _snapVersion++
    }

    function syncState() {
        var v = appState.stateVersion
        if (v === _cachedGen) return
        _cachedGen = v

        var newStatus = appState.runStatus

        // ── Detect run lifecycle transitions ──
        if (newStatus === 1 && !_runActive) {
            // Run started: take snapshot, freeze config
            takeSnapshot()
            _runActive = true
        } else if (newStatus !== 1 && _runActive) {
            // Run ended (Completed/Cancelled/Error): release snapshot, full sync
            _runActive = false
        }

        // Always sync progress (changes during run)
        _runStatus = newStatus
        _totalCompleted = appState.totalCompleted
        _totalDiags = appState.totalDiags

        // Full config sync only when not in active run
        if (!_runActive) {
            takeSnapshot()
        }
    }

    // Init immediately — triggers first takeSnapshot() before 200 ms Timer fires
    Component.onCompleted: takeSnapshot()

    // Single polling Timer — replaces 4 independent timers
    Timer {
        interval: 200; running: true; repeat: true
        onTriggered: syncState()
    }

    // Filter groups: only show those with enabled tests or results
    property var currentDetail: ({})
    property var visibleGroups: {
        var _force = _totalCompleted + _runStatus + (_runActive ? 1 : 0)
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
            Flickable {
                anchors.fill: parent
                contentWidth: width; contentHeight: sidebarColWide.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ColumnLayout { id: sidebarColWide; width: parent.width
                    SidebarContent { width: parent.width; compact: false }
                }
            }
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
        property int _cachedSnapVer: -1
        spacing: 0

        // ── CheckBox → page snapshot sync (inside component scope where cb0–cb4 exist) ──
        function syncCheckboxes() {
            if (!cb0 || !cb1 || !cb2 || !cb3 || !cb4) return
            cb0.checked = page._snapG0chk
            cb1.checked = page._snapG1chk
            cb2.checked = page._snapG2chk
            cb3.checked = page._snapG3chk
            cb4.checked = page._snapG4chk
            cb3.enabled = page._snapG3en && !page._runActive
            cb4.enabled = page._snapG4en && !page._runActive
        }

        // Poll page._snapVersion — page.takeSnapshot() bumps it on every config change.
        // Timer lives inside SidebarContent so it can reach cb0…cb4 directly.
        Timer {
            interval: 200; running: true; repeat: true
            onTriggered: {
                if (page._snapVersion !== _cachedSnapVer) {
                    _cachedSnapVer = page._snapVersion
                    syncCheckboxes()
                }
            }
        }

        // Immediate init on component creation
        Component.onCompleted: syncCheckboxes()

        // Header — matches Flutter Container(padding h16 v14, border bottom #3A3A5A)
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 48
            color: "transparent"
            border { width: 1; color: "#3A3A5A" }
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                AppIcon { name: "wifi"; size: 20; color: Theme.cyan }
                Item { width: 10 }
                Label { text: "NetAnalysis"; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.textPrimary }
            }
        }

        // Target input + Run
        Item { Layout.preferredHeight: 12 }
        TargetInputPanel { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12 }

         // Layer checkboxes — bound to page-level snapshot (frozen during run)
        Item { Layout.preferredHeight: 8; visible: !compact }
        ColumnLayout {
            visible: !compact; spacing: 2
            Layout.leftMargin: 12; Layout.rightMargin: 12
            Label { text: Tr.diagGroup; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
            Item { Layout.preferredHeight: 6 }

            // G1
            Rectangle { Layout.fillWidth: true; implicitHeight: 32; radius: 6
                color: page._snapG0chk ? Qt.alpha(Theme.accentBlue, 0.12) : "transparent"
                RowLayout { anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    CheckBox { id: cb0; Layout.preferredWidth: 18; Layout.preferredHeight: 18
                        enabled: !page._runActive
                        onClicked: appState.setGroupEnabled(0, cb0.checked) }
                    Item { width: 8 }
                    Label { Layout.fillWidth: true; text: Tr.groupName(0); font.family:"JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize:12 }
                }
            }
            // G2
            Rectangle { Layout.fillWidth: true; implicitHeight: 32; radius: 6
                color: page._snapG1chk ? Qt.alpha(Theme.accentBlue, 0.12) : "transparent"
                RowLayout { anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    CheckBox { id: cb1; Layout.preferredWidth: 18; Layout.preferredHeight: 18
                        enabled: !page._runActive
                        onClicked: appState.setGroupEnabled(1, cb1.checked) }
                    Item { width: 8 }
                    Label { Layout.fillWidth: true; text: Tr.groupName(1); font.family:"JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize:12 }
                }
            }
            // G3
            Rectangle { Layout.fillWidth: true; implicitHeight: 32; radius: 6
                color: page._snapG2chk ? Qt.alpha(Theme.accentBlue, 0.12) : "transparent"
                RowLayout { anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    CheckBox { id: cb2; Layout.preferredWidth: 18; Layout.preferredHeight: 18
                        enabled: !page._runActive
                        onClicked: appState.setGroupEnabled(2, cb2.checked) }
                    Item { width: 8 }
                    Label { Layout.fillWidth: true; text: Tr.groupName(2); font.family:"JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize:12 }
                }
            }
            // G4
            Rectangle { Layout.fillWidth: true; implicitHeight: 32; radius: 6
                color: page._snapG3chk ? Qt.alpha(Theme.accentBlue, 0.12) : "transparent"
                RowLayout { anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    CheckBox { id: cb3; Layout.preferredWidth: 18; Layout.preferredHeight: 18
                        enabled: !page._runActive && page._snapG3en
                        onClicked: appState.setGroupEnabled(3, cb3.checked) }
                    Item { width: 8 }
                    Label { Layout.fillWidth: true; text: Tr.groupName(3); font.family:"JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize:12 }
                }
            }
            // G5
            Rectangle { Layout.fillWidth: true; implicitHeight: 32; radius: 6
                color: page._snapG4chk ? Qt.alpha(Theme.accentBlue, 0.12) : "transparent"
                RowLayout { anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    CheckBox { id: cb4; Layout.preferredWidth: 18; Layout.preferredHeight: 18
                        enabled: !page._runActive && page._snapG4en
                        onClicked: appState.setGroupEnabled(4, cb4.checked) }
                    Item { width: 8 }
                    Label { Layout.fillWidth: true; text: Tr.groupName(4); font.family:"JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize:12 }
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

        // Header bar — status label, progress counter
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 41
            color: "#1A1A2E"
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                AppIcon { name: "diagnostics"; size: 18; color: Theme.cyan }
                Item { width: 8 }
                Label {
                    text: _runStatus === 1 ? Tr.runningDots :
                          _runStatus === 2 ? Tr.complete :
                          _runStatus === 3 ? Tr.cancelled :
                          _runStatus === 4 ? Tr.errorCheck : Tr.results
                    font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary
                }
                // Progress counter — visible during run
                Label {
                    visible: _runStatus === 1 && _totalDiags > 0
                    text: _totalCompleted + " / " + _totalDiags
                    font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 12
                    font.weight: Font.DemiBold; color: Theme.cyan
                }
                Item { Layout.fillWidth: true }
            }
        }

        // Results body
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            Column {
                anchors.centerIn: parent; spacing: 16
                visible: _runStatus === 0 && _totalCompleted === 0
                AppIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "wifi"; size: 80; color: Qt.alpha(Theme.textSecondary, 0.2) }
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: Tr.runDiag; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 15; font.weight: Font.Medium; color: Qt.alpha(Theme.textSecondary, 0.6) }
            }
            // Flickable results list — scrolls when content exceeds viewport
            Flickable {
                id: resultsFlick
                anchors { fill: parent; margins: 4 }
                visible: _totalCompleted > 0 || _runStatus === 1
                clip: true
                contentWidth: width
                contentHeight: treeColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                Column {
                    id: treeColumn
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: visibleGroups
                        delegate: DiagGroupPanel {
                            anchors { left: parent.left; right: parent.right }
                            groupIndex: modelData
                        onDetailClicked: function(data) {
                            var tid = data.diagId
                            var d = appState.getDetailResult(tid)
                            dtTitle.text = (d && d.displayName) ? d.displayName : (data.displayName || "Test #" + tid)
                            var statStr = "Unknown"
                            if (d && d.status !== undefined) statStr = ["Pass","Warning","Fail","Skipped","Error","Info"][d.status] || "Unknown"
                            var durStr = (d && d.durationMs) ? d.durationMs : (data.durationMs || 0)
                            dtStatus.text = "Status: " + statStr + "    Duration: " + durStr + "ms"
                            dtSummary.text = (d && d.summary) ? d.summary : (data.summary || "")
                            dtOutput.text = (d && d.details) ? d.details : ""
                            page.currentDetail = d || {}
                            detailOverlay.visible = true
                            // DEBUG: verify monospace font actually applied
                            Qt.callLater(function() {
                                console.log("[FONT-DEBUG] requested family:", dtOutput.font.family)
                                console.log("[FONT-DEBUG] resolved family:", dtOutput.fontInfo.family)
                                console.log("[FONT-DEBUG] pixelSize:", dtOutput.fontInfo.pixelSize)
                                console.log("[FONT-DEBUG] text length:", dtOutput.text.length)
                                console.log("[FONT-DEBUG] text first 200 chars:", dtOutput.text.substring(0, 200))
                            })
                        }
                    }
                }
            }
            }  // Flickable
        }

    }

    // ── Detail popup — anchors to root window for full coverage ──────
    Rectangle {
        id: detailOverlay
        parent: page.parent ? page.parent : page  // mount on StackView if available
        anchors.fill: parent
        color: "#88000000"
        visible: false; z: 1000

        onVisibleChanged: {
            if (!visible) {
                // Clear stale state to prevent flash on next open
                dtTitle.text = ""
                dtStatus.text = ""
                dtSummary.text = ""
                dtOutput.text = ""
                page.currentDetail = {}
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: detailOverlay.visible = false
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(700, parent.width - 20)
            height: Math.min(parent.height - 40, 620)
            radius: 14
            color: "#252538"
            border { width: 1.5; color: "#4A4A6A" }

            // Close button — larger, top-right corner
            Rectangle {
                anchors { top: parent.top; right: parent.right; topMargin: 10; rightMargin: 10 }
                width: 28; height: 28; radius: 14; color: "#E94560"
                AppIcon { anchors.centerIn: parent; name: "close"; size: 14; color: "white" }
                MouseArea { anchors.fill: parent; onClicked: detailOverlay.visible = false }
            }

            Flickable {
                anchors { fill: parent; margins: 16; topMargin: 44 }
                clip: true
                contentWidth: Math.max(width, detailCol.implicitWidth)
                contentHeight: detailCol.implicitHeight
                // Horizontal scrollbar for wide diagnostic table output
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                Column {
                    id: detailCol; spacing: 8
                    // Allow Column to grow wider than viewport for horizontal scrolling
                    width: Math.max(parent.width, implicitWidth)
                    Label { id: dtTitle; text: ""; textFormat:Text.PlainText; font.family:"JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize:16; font.weight:Font.DemiBold; color:"#FFFFFF"; elide:Text.ElideRight }
                    Label { id: dtStatus; text: ""; textFormat:Text.PlainText; font.family:"JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize:12; color:"#A0A0B8" }
                    Label { id: dtSummary; text: ""; textFormat:Text.PlainText; font.family:"JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize:12; color:"#E0E0E0"; wrapMode:Text.WordWrap }
                    Rectangle { width: parent.width; height: 1; color: "#3A3A5A" }
                    Repeater {
                        model: currentDetail.properties || []
                        delegate: Row {
                            spacing: 4
                            Label { text: (modelData["label"]||"?")+":"; textFormat:Text.PlainText; font.family:"JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize:11; font.weight:Font.DemiBold; color:"#A0A0B8"; width:120 }
                            Label { text: modelData["value"]||""; textFormat:Text.PlainText; font.family:"JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize:11; color:"#E0E0E0"; wrapMode:Text.WordWrap }
                        }
                    }
                    Label { id: dtOutput; text: ""; textFormat:Text.PlainText; font.family: dejavuMono.name; font.pixelSize:10; color:"#A0A0B8"; wrapMode:Text.NoWrap; visible:text!=="" }
                }
            }
        }
    }
}
