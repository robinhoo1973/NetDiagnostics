import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

Rectangle {
    id: root
    color: ThemeEngine.colors.sidebar
    property bool wide: true
    property bool _advancedVisible: false

    implicitHeight: tbCol.implicitHeight + 8
    clip: true

    Component.onCompleted: {
        // Debug logging disabled in production builds
        // console.warn("[DiagnosticToolbar] loaded — schemes:", appState.supportedSchemes.length)
    }

    ColumnLayout {
        id: tbCol
        anchors { fill: parent; leftMargin: 4; rightMargin: 4; topMargin: 4; bottomMargin: 4 }
        spacing: 2

        // ═══════════════ ROW 1: Target input + Actions ══════════════
        RowLayout {
            Layout.fillWidth: true; spacing: 6

            // ── Zone 1: Advanced toggle + Scheme + Host (fills) ─────
            RowLayout {
                Layout.fillWidth: true; spacing: 4

                // Advanced toggle (gear) — SVG icon, not emoji, for cross-platform consistency
                // 5WHY: 30×30 touch target below 44px minimum; no keyboard access or label.
                Rectangle {
                    id: gearBtn
                    Layout.preferredWidth: 44; Layout.preferredHeight: 44; radius: 8
                    color: root._advancedVisible ? Qt.alpha(ThemeEngine.colors.secondary, 0.15) : "transparent"
                    AppIcon {
                        anchors.centerIn: parent
                        // 5WHY: tune.svg was a byte-identical duplicate of
                        // config.svg — consolidated onto "config".
                        name: "config"; size: 18
                        color: root._advancedVisible ? ThemeEngine.colors.secondary : ThemeEngine.colors.textMuted
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root._advancedVisible = !root._advancedVisible }
                    activeFocusOnTab: true
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                            root._advancedVisible = !root._advancedVisible
                            event.accepted = true
                        }
                    }
                    Accessible.name: root._advancedVisible ? Tr.accHideAdvanced : Tr.accShowAdvanced
                    Accessible.role: Accessible.Button
                }

                // Uses the same catalogue, popup and AppState synchronization
                // as TargetInputPanel so the two entry points cannot drift.
                SchemeSelector {
                    id: schemeCombo
                    Layout.preferredWidth: root.wide ? 88 : 72
                    Layout.preferredHeight: 30; flat: true
                    font.family: ThemeEngine.monoFont; font.pixelSize: 12
                    enabled: appState.runStatus !== 1
                }

                // Host field
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 30; radius: 6
                    color: ThemeEngine.colors.input
                    border {
                        width: hostField.activeFocus ? 2 : 1
                        color: appState.targetValidationErrorText !== "" ? ThemeEngine.colors.failRed
                               : hostField.activeFocus ? ThemeEngine.colors.borderFocused : ThemeEngine.colors.borderCard
                    }
                    TextField {
                        id: hostField
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        font.family: ThemeEngine.monoFont; font.pixelSize: 12
                        color: ThemeEngine.colors.textPrimary
                        placeholderText: Tr.placeholderHost
                        placeholderTextColor: ThemeEngine.colors.textPlaceholder
                        text: { var h=appState.targetHost; var p=appState.targetPath; return (!h&&!p)?"": h+p }
                        enabled: appState.runStatus !== 1
                        verticalAlignment: TextInput.AlignVCenter; background: Item {}
                        onTextChanged: {
                            // 5WHY: Host/path splitting was duplicated here and in
                            // TargetInputPanel.qml.  Folded into parseUrlIntoFields
                            // (TargetModel.cpp) which now handles "example.com/path"
                            // without "://" by splitting at the first '/'.
                            appState.parseUrlIntoFields(text.trim())
                        }
                    }
                }
            }  // end Zone 1

            // ── Zone 2: Run button ──────────────────────────────────
            // Group enable/disable is managed from the Config page only.
            // The Diagnostic toolbar shows just the Run/Stop control.
            RowLayout {
                spacing: 4

                // 5WHY: Run/Stop button had no keyboard access (WCAG 2.1 SC 2.1.1
                // failure).  Touch target was 36×28 — well below 44px minimum.
                // Now: 44×44px touch target, tab-focusable, Enter/Space activate,
                // Accessible properties for screen readers.
                // Code review: extracted shared runOrCancel() to deduplicate
                // onClicked and Keys.onPressed logic.
                Rectangle {
                    id: runBtn
                    width: 44; height: 44; radius: 22
                    color: appState.runStatus === 1 ? ThemeEngine.colors.failRed
                           : appState.canRun() ? ThemeEngine.colors.secondary
                           : Qt.alpha(ThemeEngine.colors.secondary, 0.3)
                    // 5WHY: Replaced ▶/■ Unicode with play/stop SVG icons
                    // for consistent iconography across the app.
                    AppIcon { anchors.centerIn: parent
                        name: appState.runStatus === 1 ? "stop" : "play"
                        size: 16; color: "white" }
                    function runOrCancel() {
                        if (appState.runStatus === 1) {
                            if (ThemeEngine.isMobile) Qt.callLater(function() { appState.cancel() })
                            else appState.cancel()
                        } else {
                            var err = appState.targetValidationError()
                            if (err !== "" || !appState.canRun()) {
                                validationFlash.start()
                                return
                            }
                            if (ThemeEngine.isMobile) Qt.callLater(function() { appState.runDiagnostics() })
                            else appState.runDiagnostics()
                        }
                    }
                    Rectangle {
                        id: flashOverlay
                        anchors.fill: parent; radius: parent.radius
                        color: "transparent"
                        border { width: 3; color: "transparent" }
                        opacity: 0
                    }
                    SequentialAnimation {
                        id: validationFlash
                        PropertyAction { target: flashOverlay; property: "border.color"; value: ThemeEngine.colors.failRed }
                        PropertyAction { target: flashOverlay; property: "opacity"; value: 0.8 }
                        PauseAnimation { duration: 80 }
                        PropertyAction { target: flashOverlay; property: "opacity"; value: 0 }
                        PauseAnimation { duration: 80 }
                        PropertyAction { target: flashOverlay; property: "border.color"; value: ThemeEngine.colors.failRed }
                        PropertyAction { target: flashOverlay; property: "opacity"; value: 0.8 }
                        PauseAnimation { duration: 80 }
                        PropertyAction { target: flashOverlay; property: "opacity"; value: 0 }
                        PropertyAction { target: flashOverlay; property: "border.color"; value: "transparent" }
                    }
                    MouseArea { anchors.fill: parent
                        enabled: appState.runStatus === 1 || appState.canRun()
                        cursorShape: Qt.PointingHandCursor
                        onClicked: runBtn.runOrCancel()
                    }
                    activeFocusOnTab: true
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                            runBtn.runOrCancel()
                            event.accepted = true
                        }
                    }
                    Accessible.name: appState.runStatus === 1 ? Tr.accStopDiag : Tr.accRunDiag
                    Accessible.role: Accessible.Button
                }
            }  // end Zone 2

            // Visual separator + Zone 3: Clear button (desktop/wide only)
            // On narrow screens (mobile) this entire section is removed —
            // the separator, spacer, and clear button waste precious
            // horizontal space and users can clear via field backspace.
            // Visibility is gated by root.wide (passed from DiagnosticScreen)
            // instead of Qt.platform.os so it stays consistent with the rest
            // of the mobile layout (DiagGroupPanel uses the same pattern).
            Rectangle {
                Layout.preferredWidth: 1; Layout.preferredHeight: 22
                color: ThemeEngine.colors.borderCard
                visible: root.wide && hostField.text !== "" && appState.runStatus !== 1
            }
            Item { Layout.preferredWidth: root.wide ? 6 : 0; Layout.preferredHeight: 22
                visible: root.wide && (hostField.text !== "" || appState.runStatus !== 1)
            }

            // ── Zone 3: Clear button — 44px touch target, keyboard accessible ──
            // 5WHY: 30×30px touch target below minimum; no keyboard or a11y label.
            // Code review: extracted shared doClear() function for DRY.
            Item {
                id: clearBtn
                visible: root.wide
                Layout.preferredWidth: root.wide ? 44 : 0; Layout.preferredHeight: 44
                AppIcon {
                    anchors.centerIn: parent
                    name: "close"; size: 14
                    color: hostField.text !== "" && appState.runStatus !== 1
                        ? ThemeEngine.colors.textSecondary : "transparent"
                    visible: hostField.text !== "" && appState.runStatus !== 1
                }
                function doClear() {
                    hostField.text = ""
                    appState.targetHost = ""
                    appState.targetPath = ""
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    enabled: hostField.text !== "" && appState.runStatus !== 1
                    onClicked: clearBtn.doClear()
                }
                activeFocusOnTab: true
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                        if (hostField.text !== "" && appState.runStatus !== 1) {
                            clearBtn.doClear()
                        }
                        event.accepted = true  // always consume to prevent propagation
                    }
                }
                Accessible.name: Tr.accClearTarget
                Accessible.role: Accessible.Button
            }
        }  // end ROW 1

        // ═══════════════ ROW 3: Advanced fields (collapsible) ═══════════
        RowLayout {
            visible: root._advancedVisible; spacing: 6
            Layout.fillWidth: true

            // Port
            Rectangle {
                Layout.preferredWidth: 80; implicitHeight: 30; radius: 6
                color: ThemeEngine.colors.input
                border { width: portField.activeFocus ? 2 : 1; color: portField.activeFocus ? ThemeEngine.colors.borderFocused : ThemeEngine.colors.borderCard }
                TextField {
                    id: portField
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
                    placeholderText: appState.defaultPortForScheme > 0 ? ""+appState.defaultPortForScheme : Tr.placeholderPort
                    placeholderTextColor: ThemeEngine.colors.textPlaceholder
                    text: appState.targetPort > 0 ? ""+appState.targetPort : ""
                    enabled: appState.runStatus !== 1; verticalAlignment: TextInput.AlignVCenter; background: Item {}
                    onTextChanged: { var v = parseInt(text); appState.targetPort = isNaN(v) ? -1 : v }
                }
            }

            // Username
            Rectangle {
                visible: schemeCombo.supportsUsername; Layout.fillWidth: true; implicitHeight: 30; radius: 6
                color: ThemeEngine.colors.input
                border { width: userField.activeFocus ? 2 : 1; color: userField.activeFocus ? ThemeEngine.colors.borderFocused : ThemeEngine.colors.borderCard }
                TextField {
                    id: userField
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
                    placeholderText: Tr.usernameLabel
                    placeholderTextColor: ThemeEngine.colors.textPlaceholder
                    text: appState.targetUsername; enabled: appState.runStatus !== 1
                    verticalAlignment: TextInput.AlignVCenter; background: Item {}
                    onTextChanged: appState.targetUsername = text
                }
            }

            // Password
            Rectangle {
                visible: schemeCombo.supportsPassword; Layout.fillWidth: true; implicitHeight: 30; radius: 6
                color: ThemeEngine.colors.input
                border { width: passField.activeFocus ? 2 : 1; color: passField.activeFocus ? ThemeEngine.colors.borderFocused : ThemeEngine.colors.borderCard }
                TextField {
                    id: passField
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
                    placeholderText: Tr.passwordLabel
                    placeholderTextColor: ThemeEngine.colors.textPlaceholder
                    text: appState.targetPassword; echoMode: TextInput.Password; enabled: appState.runStatus !== 1
                    verticalAlignment: TextInput.AlignVCenter; background: Item {}
                    onTextChanged: appState.targetPassword = text
                }
            }
        }

    }
}
