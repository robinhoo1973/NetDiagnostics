import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

Rectangle {
    id: root
    color: ThemeEngine.bgSidebar
    property bool wide: true
    property bool _advancedVisible: false

    // ── Schema-aware field visibility ────────────────────────────────────
    // Port: always shown in advanced (every protocol has a port)
    // User: protocols that commonly use authentication
    readonly property bool _showUser: {
        var s = appState.targetScheme
        return s === "ftp" || s === "ftps" || s === "ssh" || s === "sftp" || s === "scp"
            || s === "telnet" || s === "rdp"
            || s === "mysql" || s === "postgresql" || s === "redis" || s === "mongodb" || s === "mssql"
            || s === "smtp" || s === "smtps" || s === "imap" || s === "imaps"
            || s === "pop3" || s === "pop3s"
            || s === "ldap" || s === "ldaps"
            || s === "mqtt" || s === "mqtts"
    }
    // Pass: subset that routinely uses password auth (not SSH key-based)
    readonly property bool _showPass: {
        var s = appState.targetScheme
        return s === "ftp" || s === "ftps"
            || s === "telnet" || s === "rdp"
            || s === "mysql" || s === "postgresql" || s === "redis" || s === "mongodb" || s === "mssql"
            || s === "smtp" || s === "smtps" || s === "imap" || s === "imaps"
            || s === "pop3" || s === "pop3s"
            || s === "ldap" || s === "ldaps"
            || s === "mqtt" || s === "mqtts"
    }
    // Port shown for all protocols — always visible in advanced

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
                    color: root._advancedVisible ? Qt.alpha(ThemeEngine.accentBlue, 0.15) : "transparent"
                    AppIcon {
                        anchors.centerIn: parent
                        name: "tune"; size: 14
                        color: root._advancedVisible ? ThemeEngine.accentBlue : ThemeEngine.textMuted
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root._advancedVisible = !root._advancedVisible }
                    activeFocusOnTab: true
                    Keys.onPressed: function(event) { if (event.key===Qt.Key_Return||event.key===Qt.Key_Space) { root._advancedVisible=!root._advancedVisible; event.accepted=true } }
                    Accessible.name: root._advancedVisible ? "Hide advanced options" : "Show advanced options"
                    Accessible.role: Accessible.Button
                }

                // Scheme combo
                ComboBox {
                    id: schemeCombo
                    Layout.preferredWidth: root.wide ? 88 : 72
                    Layout.preferredHeight: 30; flat: true
                    font.family: ThemeEngine.monoFont; font.pixelSize: 12
                    enabled: appState.runStatus !== 1
                    displayText: currentText + "://"

                    contentItem: Label {
                        text: schemeCombo.displayText; font: schemeCombo.font
                        color: ThemeEngine.colors.textPrimary; verticalAlignment: Text.AlignVCenter
                    }
                    textRole: "scheme"
                    model: ListModel {
                        id: _sm
                        Component.onCompleted: {
                            var schemes = appState.supportedSchemes
                            var groups = [
                                {schemes:["https","http"]},
                                {schemes:["ftp","ftps","ssh","sftp","scp"]},
                                {schemes:["smtp","smtps","imap","imaps","pop3","pop3s"]},
                                {schemes:["mysql","postgresql","redis","mongodb","mssql"]},
                                {schemes:["telnet","rdp"]},
                                {schemes:["ldap","ldaps"]},
                                {schemes:["mqtt","mqtts"]}
                            ]
                            for (var gi = 0; gi < groups.length; gi++)
                                for (var si = 0; si < groups[gi].schemes.length; si++) {
                                    var s = groups[gi].schemes[si]
                                    if (schemes.indexOf(s) >= 0) _sm.append({scheme:s, schemeGroup:gi})
                                }
                            for (var i = 0; i < _sm.count; i++)
                                if (_sm.get(i).scheme === "https") { schemeCombo.currentIndex = i; break }
                        }
                    }

                    popup: Popup {
                        y: schemeCombo.height; width: 222; padding: 6
                        // Clamp popup height at 320px so it never overflows the screen on
                        // short / phone displays; taller content scrolls via the ListView.
                        // Uses a constant max-height instead of Window.height arithmetic to
                        // avoid depending on the Window attached property (which can be null
                        // during component initialization in some Qt configurations).
                        height: Math.min(implicitHeight, 320)
                        background: Rectangle {
                            color: ThemeEngine.bgCard
                            border { width: 1; color: ThemeEngine.colors.borderCard }
                            radius: 10
                        }
                        contentItem: ListView {
                            clip: true; implicitHeight: contentHeight
                            model: schemeCombo.popup.visible ? schemeCombo.delegateModel : null
                            ScrollIndicator.vertical: ScrollIndicator {}
                        }
                    }
                    delegate: ItemDelegate {
                        width: ListView.view ? ListView.view.width : 222
                        hoverEnabled: true
                        padding: 0; leftPadding: 0; rightPadding: 0
                        height: isFirst ? 64 : 36
                        // ── Computed delegate properties ─────────────────
                        readonly property bool isFirst: {
                            var prev = model.index > 0 ? _sm.get(model.index - 1) : null
                            return !prev || prev.schemeGroup !== _sm.get(model.index).schemeGroup
                        }
                        readonly property string groupIcon: ({
                            0:"globe",1:"portscan",2:"mail",3:"config",
                            4:"wifi",5:"target",6:"timer"
                        }[schemeGroup] || "circle")
                        readonly property string groupLabel: ({
                            0:Tr.schemeGroupWeb,1:Tr.schemeGroupFile,
                            2:Tr.schemeGroupEmail,3:Tr.schemeGroupDb,
                            4:Tr.schemeGroupRemote,5:Tr.schemeGroupDir,
                            6:Tr.schemeGroupMsg
                        }[schemeGroup] || "")
                        highlighted: scheme === schemeCombo.currentText
                        background: Rectangle {
                            color: highlighted
                                ? Qt.alpha(ThemeEngine.colors.primary, 0.12)
                                : (hovered ? Qt.alpha(ThemeEngine.colors.primary, 0.05) : "transparent")
                            radius: 6
                        }
                        contentItem: ColumnLayout {
                            spacing: 0
                            // ── Group header: separator + icon + label ───
                            Item {
                                Layout.fillWidth: true
                                implicitHeight: isFirst ? 26 : 0; visible: isFirst
                                // Separator — thin line with generous margins
                                Rectangle {
                                    anchors { left: parent.left; right: parent.right; top: parent.top
                                              leftMargin: 10; rightMargin: 10; topMargin: 4 }
                                    height: 1; color: Qt.alpha(ThemeEngine.colors.borderCard, 0.6)
                                }
                                // Icon (12px) + label (10px) — 10px gap
                                Row {
                                    anchors { left: parent.left; leftMargin: 16; bottom: parent.bottom; bottomMargin: 3 }
                                    spacing: 10
                                    AppIcon {
                                        name: groupIcon; size: 12; color: ThemeEngine.colors.primary
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label {
                                        text: groupLabel.toUpperCase()
                                        font.family: ThemeEngine.monoFont; font.pixelSize: 10
                                        font.weight: Font.Bold
                                        color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.65)
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                            // ── Scheme item row: padded, with :// suffix ──
                            Label {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                text: scheme + "://"
                                font.family: ThemeEngine.monoFont; font.pixelSize: 13
                                font.weight: highlighted ? Font.DemiBold : Font.Normal
                                color: highlighted ? ThemeEngine.colors.primary : ThemeEngine.colors.textPrimary
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 28
                            }
                        }
                    }
                    onCurrentTextChanged: {
                        if (currentText && appState.targetScheme !== currentText)
                            appState.targetScheme = currentText
                    }
                }

                // Host field
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 30; radius: 6
                    color: ThemeEngine.bgInput
                    border {
                        width: hostField.activeFocus ? 1.5 : 1
                        color: appState.targetValidationError() !== "" ? ThemeEngine.failRed
                               : hostField.activeFocus ? ThemeEngine.accentBlue : ThemeEngine.colors.borderCard
                    }
                    TextField {
                        id: hostField
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        font.family: ThemeEngine.monoFont; font.pixelSize: 12
                        color: ThemeEngine.colors.textPrimary
                        placeholderText: "example.com/path"
                        placeholderTextColor: Qt.alpha(ThemeEngine.colors.textSecondary, 0.4)
                        text: { var h=appState.targetHost; var p=appState.targetPath; return (!h&&!p)?"": h+p }
                        enabled: appState.runStatus !== 1
                        verticalAlignment: TextInput.AlignVCenter; background: Item {}
                        onTextChanged: {
                            var t = text.trim()
                            if (t.indexOf("://") >= 0) { appState.parseUrlIntoFields(t); return }
                            var slash = t.indexOf("/")
                            if (slash >= 0) { appState.targetHost = t.substring(0,slash); appState.targetPath = t.substring(slash) }
                            else { appState.targetHost = t; appState.targetPath = "" }
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
                Rectangle {
                    id: runBtn
                    width: 44; height: 44; radius: 22
                    color: appState.runStatus === 1 ? ThemeEngine.failRed
                           : appState.canRun() ? ThemeEngine.accentBlue
                           : Qt.alpha(ThemeEngine.accentBlue, 0.3)
                    Label { anchors.centerIn: parent
                        text: appState.runStatus === 1 ? "■" : "▶"
                        font.family: ThemeEngine.monoFont; font.pixelSize: 14; color: "white" }
                    MouseArea { anchors.fill: parent
                        enabled: appState.runStatus === 1 || appState.canRun()
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (appState.runStatus === 1) { appState.cancel() }
                            else { if (appState.targetValidationError() !== "" || !appState.canRun()) return; appState.runDiagnostics() }
                        }
                    }
                    activeFocusOnTab: true
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                            if (appState.runStatus === 1) { appState.cancel() }
                            else { if (appState.targetValidationError() !== "" || !appState.canRun()) return; appState.runDiagnostics() }
                            event.accepted = true
                        }
                    }
                    Accessible.name: appState.runStatus === 1 ? "Stop diagnostics" : "Run diagnostics"
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
            Item {
                id: clearBtn
                visible: root.wide
                Layout.preferredWidth: root.wide ? 44 : 0; Layout.preferredHeight: 44
                AppIcon {
                    anchors.centerIn: parent
                    name: "close"; size: 14
                    color: hostField.text !== "" && appState.runStatus !== 1
                        ? ThemeEngine.textSecondary : "transparent"
                    visible: hostField.text !== "" && appState.runStatus !== 1
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    enabled: hostField.text !== "" && appState.runStatus !== 1
                    onClicked: { hostField.text=""; appState.targetHost=""; appState.targetPath="" }
                }
                activeFocusOnTab: true
                Keys.onPressed: function(event) {
                    if ((event.key===Qt.Key_Return||event.key===Qt.Key_Space) && hostField.text!=="" && appState.runStatus!==1)
                        { hostField.text=""; appState.targetHost=""; appState.targetPath=""; event.accepted=true }
                }
                Accessible.name: "Clear target input"
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
                color: ThemeEngine.bgInput
                border { width: 1; color: portField.activeFocus ? ThemeEngine.accentBlue : ThemeEngine.colors.borderCard }
                TextField {
                    id: portField
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
                    placeholderText: appState.defaultPortForScheme > 0 ? ""+appState.defaultPortForScheme : "Port"
                    placeholderTextColor: Qt.alpha(ThemeEngine.colors.textSecondary, 0.4)
                    text: appState.targetPort > 0 ? ""+appState.targetPort : ""
                    enabled: appState.runStatus !== 1; verticalAlignment: TextInput.AlignVCenter; background: Item {}
                    onTextChanged: { var v = parseInt(text); appState.targetPort = isNaN(v) ? -1 : v }
                }
            }

            // Username
            Rectangle {
                visible: root._showUser; Layout.fillWidth: true; implicitHeight: 30; radius: 6
                color: ThemeEngine.bgInput
                border { width: 1; color: userField.activeFocus ? ThemeEngine.accentBlue : ThemeEngine.colors.borderCard }
                TextField {
                    id: userField
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
                    placeholderText: Tr.usernameLabel
                    placeholderTextColor: Qt.alpha(ThemeEngine.colors.textSecondary, 0.4)
                    text: appState.targetUsername; enabled: appState.runStatus !== 1
                    verticalAlignment: TextInput.AlignVCenter; background: Item {}
                    onTextChanged: appState.targetUsername = text.trim()
                }
            }

            // Password
            Rectangle {
                visible: root._showPass; Layout.fillWidth: true; implicitHeight: 30; radius: 6
                color: ThemeEngine.bgInput
                border { width: 1; color: passField.activeFocus ? ThemeEngine.accentBlue : ThemeEngine.colors.borderCard }
                TextField {
                    id: passField
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
                    placeholderText: Tr.passwordLabel
                    placeholderTextColor: Qt.alpha(ThemeEngine.colors.textSecondary, 0.4)
                    text: appState.targetPassword; echoMode: TextInput.Password; enabled: appState.runStatus !== 1
                    verticalAlignment: TextInput.AlignVCenter; background: Item {}
                    onTextChanged: appState.targetPassword = text.trim()
                }
            }
        }

    }
}
