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

    property var selectedSchemas: ({})

    function buildConfig() {
        var schemas = []
        var targets = {}
        for (var key in selectedSchemas) {
            if (selectedSchemas[key]) {
                schemas.push(key)
                targets[key] = schemaFields[key].targetField.text || presetTargets[key] || ""
            }
        }
        return { schemas: schemas, targets: targets }
    }

    function startTest() {
        var cfg = buildConfig()
        if (cfg.schemas.length === 0) {
            // No schemas selected — just run G1-G4
            for (var g = 0; g < 4; g++) appState.setGroupActive(g, true)
            appState.setGroupActive(4, false)
            appState.setTarget("localhost")
            appState.runDiagnostics()
        } else {
            // G1-G4 + selected G5 schemas
            for (var g = 0; g < 5; g++) appState.setGroupActive(g, true)
            appState.runAllTests(cfg)
        }
        root.close()
    }

    // ── UI ────────────────────────────────────────────────────────────────
    ColumnLayout {
        spacing: 8
        width: 360

        Label {
            text: "Full Diagnostic Auto-Test"
            font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold
            color: ThemeEngine.textPrimary
        }
        Label {
            text: "Select protocols to test. G1-G4 run on localhost. G5 tests run\non globally accessible public test servers."
            font.family: ThemeEngine.monoFont; font.pixelSize: 11
            color: ThemeEngine.textSecondary; wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ── G5 Protocol checkboxes ────────────────────────────────────────
        Item { height: 4; width: 1 }

        Repeater {
            id: schemaList
            model: ["https","http","ftp","ssh","telnet","mqtt","smtp"]

            RowLayout {
                spacing: 8; Layout.fillWidth: true

                CheckBox {
                    id: schemaCheck
                    checked: false
                    onCheckedChanged: { root.selectedSchemas[modelData] = checked }
                }
                Label {
                    text: modelData.toUpperCase()
                    font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.Bold
                    color: ThemeEngine.textPrimary
                    Layout.minimumWidth: 70
                }
                TextField {
                    id: targetField
                    text: root.presetTargets[modelData] || ""
                    font.family: ThemeEngine.monoFont; font.pixelSize: 12
                    color: ThemeEngine.textPrimary
                    placeholderText: "target host[:port]"
                    Layout.fillWidth: true
                    enabled: schemaCheck.checked
                }
            }
        }

        // Store field references so buildConfig() can read user-edited targets
        property var schemaFields: {
            var m = {}
            for (var i = 0; i < schemaList.count; i++) {
                var item = schemaList.itemAt(i)
                m[schemaList.model[i]] = { targetField: item.children[2] }
            }
            return m
        }

        // ── Buttons ───────────────────────────────────────────────────────
        Item { height: 8; width: 1 }

        RowLayout {
            spacing: 12; Layout.alignment: Qt.AlignRight

            Button {
                text: "Cancel"
                flat: true
                onClicked: root.close()
            }
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
}
