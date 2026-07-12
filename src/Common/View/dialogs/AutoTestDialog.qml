// =============================================================================
// AutoTestDialog.qml — Easter egg: full auto-test with protocol selection
// Activated by tapping version label 5× (desktop) or long-press (mobile).
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

Dialog {
    id: root
    title: "Auto Test"
    modal: true
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape

    // Pre-set test targets (globally accessible neutral servers)
    readonly property var presetTargets: ({
        "https":    "httpbin.org",
        "http":     "httpbin.org",
        "ftp":      "test.rebex.net",
        "ssh":      "test.rebex.net:22",
        "telnet":   "telehack.com:23",
        "mqtt":     "test.mosquitto.org:1883",
        "smtp":     "smtp.mailtrap.io:587"
    })

    // ── Protocol descriptors (for tooltips) ──────────────────────────
    readonly property var protocolDescs: ({
        "https":    "HTTP/HTTPS headers, SSL cert, redirect, compression, timing",
        "http":     "HTTP plain — same tests as HTTPS without TLS",
        "ftp":      "FTP banner, anonymous login, PASV mode check",
        "ssh":      "SSH banner, key exchange, cipher negotiation",
        "telnet":   "Telnet banner grab, terminal negotiation",
        "mqtt":     "MQTT CONNECT, broker version, topic access",
        "smtp":     "SMTP EHLO, STARTTLS, mail server capabilities"
    })

    property var selectedSchemas: ({})
    property var targetFields: ({})

    function buildConfig() {
        var schemas = []
        var targets = {}
        for (var key in selectedSchemas) {
            if (selectedSchemas[key]) {
                schemas.push(key)
                targets[key] = targetFields[key] || presetTargets[key] || ""
            }
        }
        return { schemas: schemas, targets: targets }
    }

    function startTest() {
        var cfg = buildConfig()
        // Enable G1-G4 (always) + G5 only if schemas selected
        for (var g = 0; g < 4; g++) appState.setGroupActive(g, true)
        appState.setGroupActive(4, cfg.schemas.length > 0)
        // Run sequentially: first G1-G4 on localhost, then G5 per schema
        runNextSchema(cfg, 0)
    }

    // Sequential execution: G1-G4 first, then each G5 schema one by one
    property var _queue: []
    property int _qIdx: 0
    property string _currentLabel: ""
    property bool _autoTestActive: false

    function runNextSchema(cfg, idx) {
        _queue = []
        _qIdx = 0
        _autoTestActive = true
        // Phase 1: G1-G4 on localhost
        _queue.push({ target: "localhost", label: "G1-G4 (localhost)" })
        // Phase 2: each selected G5 schema
        for (var i = 0; i < cfg.schemas.length; i++) {
            _queue.push({
                target: cfg.targets[cfg.schemas[i]],
                label: "G5 " + cfg.schemas[i].toUpperCase() + " (" + (cfg.targets[cfg.schemas[i]] || "") + ")"
            })
        }
        if (_queue.length === 0) {
            _autoTestActive = false; root.close(); return
        }
        _currentLabel = _queue[0].label
        appState.setTarget(_queue[0].target)
        appState.runDiagnostics()
    }

    function onRunCompleted() {
        if (!_autoTestActive) return
        _qIdx++
        if (_qIdx >= _queue.length) {
            _currentLabel = "All tests complete"
            _autoTestActive = false
            root.close()
            return
        }
        _currentLabel = _queue[_qIdx].label
        appState.setTarget(_queue[_qIdx].target)
        appState.runDiagnostics()
    }

    // ── Select All / Deselect All ────────────────────────────────────
    property bool allSelected: false
    function toggleAll() {
        allSelected = !allSelected
        for (var i = 0; i < schemaList.count; i++) {
            var item = schemaList.itemAt(i)
            if (item) {
                for (var j = 0; j < item.children.length; j++) {
                    var child = item.children[j]
                    if (child && child.objectName && child.objectName.indexOf("schemaCheck_") === 0)
                        child.checked = allSelected
                }
            }
        }
    }

    // ── Monitor run completion via appState signal ────────────────────
    Connections {
        target: appState
        function onRunStatusChanged() {
            var rs = appState.runStatus()
            // RunStatus: Idle=0, Running=1, Completed=2, Cancelled=3, Error=4
            if (rs === 2 || rs === 3 || rs === 4) {
                if (_qIdx < _queue.length) root.onRunCompleted()
            }
        }
    }

    // ── UI ────────────────────────────────────────────────────────────
    ColumnLayout {
        spacing: 8
        Layout.preferredWidth: 420
        width: 420

        // Title
        Label {
            text: "Full Diagnostic Auto-Test"
            font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold
            color: ThemeEngine.textPrimary
        }
        Label {
            text: "G1-G4 tests run on localhost. G5 protocol tests use globally\naccessible public test servers. Select protocols to include."
            font.family: ThemeEngine.monoFont; font.pixelSize: 11
            color: ThemeEngine.textSecondary; wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Select All / Deselect All toggle
        RowLayout {
            Layout.fillWidth: true
            CheckBox {
                id: selectAllCheck
                text: root.allSelected ? "Deselect All" : "Select All"
                font.family: ThemeEngine.monoFont; font.pixelSize: 11
                onClicked: root.toggleAll()
            }
            Item { Layout.fillWidth: true }
            Label {
                text: root._currentLabel || ""
                font.family: ThemeEngine.monoFont; font.pixelSize: 10
                color: ThemeEngine.textSecondary
                visible: root._currentLabel.length > 0
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: ThemeEngine.borderSubtle }

        // Protocol list
        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(280, schemaList.count * 38)
            contentHeight: schemaColumn.implicitHeight
            clip: true
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: schemaColumn
                width: parent.width
                spacing: 2

                Repeater {
                    id: schemaList
                    model: ["https","http","ftp","ssh","telnet","mqtt","smtp"]

                    RowLayout {
                        spacing: 8; Layout.fillWidth: true
                        CheckBox {
                            objectName: "schemaCheck_" + modelData
                            checked: false
                            onCheckedChanged: {
                                root.selectedSchemas[modelData] = checked
                                // Update allSelected state
                                var anyUnchecked = false
                                for (var i = 0; i < schemaList.count; i++) {
                                    var it = schemaList.itemAt(i)
                                    if (it) {
                                        for (var j = 0; j < it.children.length; j++) {
                                            var child = it.children[j]
                                            if (child && child.objectName && child.objectName.indexOf("schemaCheck_") === 0 && !child.checked)
                                                anyUnchecked = true
                                        }
                                    }
                                }
                                root.allSelected = !anyUnchecked
                            }
                        }
                            text: modelData.toUpperCase()
                            font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.Bold
                            color: ThemeEngine.textPrimary
                            Layout.minimumWidth: 65
                        }
                        TextField {
                            id: targetField
                            text: root.presetTargets[modelData] || ""
                            font.family: ThemeEngine.monoFont; font.pixelSize: 12
                            color: ThemeEngine.textPrimary
                            placeholderText: "target[:port]"
                            Layout.fillWidth: true
                            enabled: schemaCheck.checked
                            onTextChanged: { root.targetFields[modelData] = text }
                        }
                        // Tooltip icon
                        Label {
                            text: "?"
                            font.family: ThemeEngine.monoFont; font.pixelSize: 12
                            color: ThemeEngine.textMuted
                            Layout.minimumWidth: 16
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                ToolTip.text: root.protocolDescs[modelData] || ""
                                ToolTip.visible: containsMouse
                            }
                        }
                    }
                }
            }
        }

        // Buttons
        Item { height: 4; width: 1 }
        RowLayout {
            spacing: 12; Layout.alignment: Qt.AlignRight
            Button { text: "Cancel"; flat: true; onClicked: root.close() }
            Button {
                text: " G1-G4 Only "
                onClicked: {
                    for (var g = 0; g < 4; g++) appState.setGroupActive(g, true)
                    appState.setGroupActive(4, false)
                    appState.setTarget("localhost")
                    appState.runDiagnostics()
                    root.close()
                }
            }
            Button {
                text: " Run Full Test "
                highlighted: true
                onClicked: root.startTest()
            }
        }
    }
