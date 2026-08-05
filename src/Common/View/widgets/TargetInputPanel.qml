import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

ColumnLayout {
    id: root
    spacing: 0
    // ── Advanced (Port / User / Pass) toggle ──────────────────────────────
    property bool advancedExpanded: false
    // 5WHY: This reusable widget read `page._snap…` from an implicit parent
    // ID. It could only work beneath one specific DiagnosticScreen instance
    // and failed when loaded independently. Keep validation state explicit
    // and self-contained; a future host can override validationError if it
    // needs a snapshot rather than this live default.
    property string validationError: appState.targetValidationErrorText
    readonly property string validationIconName: validationError !== "" ? "badge-warning" : "badge-info"
    readonly property color validationIconColor: validationError !== "" ? ThemeEngine.colors.warnYellow
                                                                          : ThemeEngine.colors.infoBlue

    RowLayout {
        AppIcon { name: "monitor"; size: 13; color: Qt.alpha(ThemeEngine.colors.textPrimary, 0.7) }
        Item { width: 5 }
        Label { text: Tr.target; font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.colors.textSecondary }
    }
    Item { Layout.preferredHeight: 6 }

    // ── Scheme ComboBox + Host Field ────────────────────────────────────
    Rectangle {
        Layout.fillWidth: true; implicitHeight: 40; radius: 8
        color: ThemeEngine.colors.input
        border { width: hostField.activeFocus || schemeCombo.activeFocus ? 2 : 1
               color: root.validationError !== "" ? ThemeEngine.colors.failRed
                        : (hostField.activeFocus || schemeCombo.activeFocus) ? ThemeEngine.colors.borderFocused
                        : ThemeEngine.colors.borderCard }

        RowLayout {
            anchors { fill: parent; leftMargin: 6; rightMargin: 4 }
            AppIcon {
                name: root.validationIconName; size: 12; color: root.validationIconColor
            }
            Item { width: 2 }

            // Shared scheme catalogue / popup keeps every target-entry
            // surface semantically and visually synchronized.
            SchemeSelector {
                id: schemeCombo
                Layout.preferredWidth: Math.min(105, parent.width * 0.28)
                Layout.fillHeight: true
                flat: true
                font.family: ThemeEngine.monoFont; font.pixelSize: 11
                enabled: appState.runStatus !== 1
            }

            // ── Host / Path field ────────────────────────────────────────
            TextField {
                id: hostField
                Layout.fillWidth: true; Layout.fillHeight: true
                font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.colors.textPrimary
                placeholderText: Tr.placeholderHost
                placeholderTextColor: ThemeEngine.colors.textPlaceholder
                text: {
                    // Combine host + path for display
                    var h = appState.targetHost
                    var p = appState.targetPath
                    if (!h && !p) return ""
                    return h + p
                }
                enabled: appState.runStatus !== 1
                verticalAlignment: TextInput.AlignVCenter
                background: Item {}

                onTextChanged: {
                    // 5WHY: Host/path splitting was duplicated here and in
                    // DiagnosticToolbar.qml.  Folded into parseUrlIntoFields
                    // (TargetModel.cpp) which now handles "example.com/path"
                    // without "://" by splitting at the first '/'.
                    appState.parseUrlIntoFields(text.trim())
                }
            }

            // ── Clear button ────────────────────────────────────────────
            Item { width: 4; visible: hostField.text !== "" && appState.runStatus !== 1 }
            // 5WHY: AppIcon size:10 gave a 10x10pt touch target — far below
            // the 44pt Apple HIG / 48dp MD3 minimum.  Wrap in a 44x44 Item
            // with the icon centered, matching the DiagnosticToolbar pattern.
            Item {
                width: 44; height: 44
                visible: hostField.text !== "" && appState.runStatus !== 1
                AppIcon {
                    anchors.centerIn: parent
                    name: "close"; size: 10; color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { hostField.text = ""; appState.targetHost = ""; appState.targetPath = "" }
                }
            }

            // ── Advanced toggle (gear) ──────────────────────────────────
            Item { width: 2; visible: appState.runStatus !== 1 }
            // 5WHY: Same touch-target issue as clear button — AppIcon size:12
            // gave a 12x12pt hit area.  Wrap in 44x44 Item matching the
            // DiagnosticToolbar pattern for accessibility compliance.
            Item {
                width: 44; height: 44
                visible: appState.runStatus !== 1
                AppIcon {
                    anchors.centerIn: parent
                    // 5WHY: tune.svg was a byte-identical duplicate of config.svg
                    // (same slider glyph) — consolidated onto "config" to remove
                    // the redundant master/generated-icon file set.
                    name: "config"; size: 12
                    color: root.advancedExpanded ? ThemeEngine.colors.secondary : Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.advancedExpanded = !root.advancedExpanded
                }
            }
        }
    }

    // ── Advanced: Port / User / Pass
    Item { Layout.preferredHeight: 6; visible: root.advancedExpanded }
    RowLayout {
        visible: root.advancedExpanded
        spacing: 6
        // Port — adaptive width, capped so it never dominates the row
        Rectangle {
            Layout.preferredWidth: Math.min(80, parent.width * 0.22)
            implicitHeight: 32; radius: 6
            color: Qt.alpha(ThemeEngine.colors.surface, 0.4)
            border { width: portField.activeFocus ? 2 : 1; color: portField.activeFocus ? ThemeEngine.colors.borderFocused : ThemeEngine.colors.borderCard }
            TextField {
                id: portField
                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
                placeholderText: appState.defaultPortForScheme > 0 ? "" + appState.defaultPortForScheme : Tr.placeholderPort
                placeholderTextColor: ThemeEngine.colors.textPlaceholder
                text: appState.targetPort > 0 ? "" + appState.targetPort : ""
                enabled: appState.runStatus !== 1
                verticalAlignment: TextInput.AlignVCenter
                background: Item {}
                onTextChanged: {
                    var v = parseInt(text)
                    appState.targetPort = isNaN(v) ? -1 : v
                }
            }
        }
        // Username
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 32; radius: 6
            color: Qt.alpha(ThemeEngine.colors.surface, 0.4)
            border { width: userField.activeFocus ? 2 : 1; color: userField.activeFocus ? ThemeEngine.colors.borderFocused : ThemeEngine.colors.borderCard }
            TextField {
                id: userField
                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
                placeholderText: Tr.usernameLabel
                placeholderTextColor: ThemeEngine.colors.textPlaceholder
                text: appState.targetUsername
                enabled: appState.runStatus !== 1
                verticalAlignment: TextInput.AlignVCenter
                background: Item {}
                onTextChanged: appState.targetUsername = text
            }
        }
        // Password
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 32; radius: 6
            color: Qt.alpha(ThemeEngine.colors.surface, 0.4)
            border {
                width: passField.activeFocus || passToggle.activeFocus ? 2 : 1
                color: passField.activeFocus || passToggle.activeFocus || passVisBtn.containsMouse
                       ? ThemeEngine.colors.borderFocused : ThemeEngine.colors.borderCard
            }
            RowLayout {
                anchors { fill: parent; leftMargin: 8; rightMargin: 2 }
                spacing: 0
                TextField {
                    id: passField
                    Layout.fillWidth: true; Layout.fillHeight: true
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
                    placeholderText: Tr.passwordLabel
                    placeholderTextColor: ThemeEngine.colors.textPlaceholder
                    text: appState.targetPassword
                    echoMode: passField._showPass ? TextInput.Normal : TextInput.Password
                    enabled: appState.runStatus !== 1
                    verticalAlignment: TextInput.AlignVCenter
                    background: Item {}
                    onTextChanged: appState.targetPassword = text
                    // 5WHY: Password visibility toggle — users enter credentials for
                    // MySQL, PostgreSQL, LDAP, etc. A typo in a hidden field produces
                    // silent auth failures with no feedback. Show/hide toggle reduces
                    // credential-entry errors per NIST SP 800-63B §5.1.1.2.
                    property bool _showPass: false
                }
                // Visibility toggle — TODO: replace check/close with eye/eye-off
                // SVG icons are in resources/icons/src/.
                // 5WHY: Touch target was 36×28 despite Apple HIG requiring 44pt
                // minimum in the comment.  Now matches the documented minimum.
                Item {
                    id: passToggle
                    implicitWidth: 44; implicitHeight: 44
                    visible: passField.text !== ""
                    Accessible.name: passField._showPass ? Tr.accHidePassword : Tr.accShowPassword
                    Accessible.role: Accessible.Button
                    activeFocusOnTab: true
                    function toggleVisibility() {
                        passField._showPass = !passField._showPass
                    }
                    AppIcon {
                        anchors.centerIn: parent
                        name: passField._showPass ? "check" : "close"
                        size: 14
                        color: passField._showPass ? ThemeEngine.colors.secondary
                                                    : Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
                    }
                    MouseArea {
                        id: passVisBtn
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: passToggle.toggleVisibility()
                        hoverEnabled: true
                    }
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                            || event.key === Qt.Key_Space) {
                            passToggle.toggleVisibility()
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }

    // ── Validation error ────────────────────────────────────────────────
    RowLayout {
        visible: root.validationError !== ""
        spacing: 4
        AppIcon { name: "warning"; size: 12; color: ThemeEngine.colors.failRed }
        Label {
            Layout.fillWidth: true
            // 5WHY: validationError is a C++ message — route through Tr.trMsg()
            // so it translates on language switch.
            text: Tr.trMsg(root.validationError)
            font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.colors.failRed
            wrapMode: Text.WordWrap
        }
    }
    Item { Layout.preferredHeight: 10 }

    // ── Run / Stop buttons ──────────────────────────────────────────────
    RowLayout {
        // 5WHY: Run button was 38pt — below Apple HIG (44pt) and M3 (48dp)
        // minimum touch targets. Increased to 44pt for accessible tapping.
        Rectangle {
            id: runBtn
            Layout.fillWidth: true; implicitHeight: 44; radius: 8
            color: appState.runStatus === 1 ? Qt.alpha(ThemeEngine.colors.secondary, 0.4) : (appState.canRun() ? ThemeEngine.colors.secondary : Qt.alpha(ThemeEngine.colors.secondary, 0.3))
            // 5WHY: "white" was hardcoded — doesn't adapt to light theme.
            // Use ThemeEngine.colors.surface for enabled, textPrimary for disabled.
            Label {
                anchors.centerIn: parent
                text: appState.runStatus === 1 ? Tr.running : Tr.runDiag
                font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.DemiBold
                color: (appState.canRun() || appState.runStatus === 1) ? ThemeEngine.colors.surface : Qt.alpha(ThemeEngine.colors.textPrimary, 0.4)
            }
            // 5WHY: MouseArea-only controls lack keyboard accessibility.
            // Adding Keys.onPressed + activeFocusOnTab so keyboard users
            // can activate via Enter/Space (WCAG 2.1 SC 2.1.1).
            MouseArea {
                id: runBtnArea
                anchors.fill: parent
                enabled: appState.runStatus !== 1 && appState.canRun()
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: {
                    if (appState.targetValidationError() !== "") {
                        validationFlash.start()
                        return
                    }
                    if (!appState.canRun()) return
                    if (ThemeEngine.isMobile) Qt.callLater(function() { appState.runDiagnostics() })
                    else appState.runDiagnostics()
                }
            }
            // Validation error feedback animation — brief red flash on click
            Rectangle {
                anchors.fill: parent; radius: 8; color: "transparent"
                border { width: 2; color: "transparent" }
                SequentialAnimation on border.color {
                    id: validationFlash
                    running: false
                    PropertyAction { value: ThemeEngine.colors.failRed }
                    PauseAnimation { duration: 300 }
                    PropertyAction { value: "transparent" }
                }
            }
            focus: true
            activeFocusOnTab: true
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                    if (appState.targetValidationError() !== "") {
                        validationFlash.start()
                        return
                    }
                    if (appState.canRun()) {
                        if (ThemeEngine.isMobile) Qt.callLater(function() { appState.runDiagnostics() })
                        else appState.runDiagnostics()
                    }
                }
            }
        }
        Item { width: 6; visible: appState.runStatus === 1 }
        Rectangle {
            id: stopBtn
            visible: appState.runStatus === 1
            Layout.preferredWidth: Math.min(90, parent.width * 0.25); implicitHeight: 44; radius: 8
            color: "transparent"; border { width: 1; color: Qt.alpha(ThemeEngine.colors.failRed, 0.5) }
            // 5WHY: Replaced ■ Unicode prefix with stop SVG icon + text label.
            RowLayout {
                anchors.centerIn: parent
                spacing: 6
                AppIcon { name: "stop"; size: 12; color: ThemeEngine.colors.failRed }
                Label { text: Tr.stop; font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.failRed }
            }
            MouseArea {
                id: stopBtnArea
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (ThemeEngine.isMobile) Qt.callLater(function() { appState.cancel() })
                    else appState.cancel()
                }
            }
            activeFocusOnTab: true
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                    if (ThemeEngine.isMobile) Qt.callLater(function() { appState.cancel() })
                    else appState.cancel()
                }
            }
        }
    }
}
