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

    ColumnLayout {
        id: tbCol
        anchors { fill: parent; leftMargin: 4; rightMargin: 4; topMargin: 4; bottomMargin: 4 }
        spacing: 2

        // ═══════════════ ROW 1: 3-zone layout ═══════════════════════
        RowLayout {
            Layout.fillWidth: true; spacing: 6

            // ── Zone 1: Advanced toggle + Scheme + Host ──────────────
            RowLayout {
                Layout.fillWidth: true; spacing: 4

                // Advanced toggle (gear)
                Rectangle {
                    Layout.preferredWidth: 30; Layout.preferredHeight: 30; radius: 6
                    color: root._advancedVisible ? Qt.alpha(ThemeEngine.accentBlue, 0.15) : "transparent"
                    Label { anchors.centerIn: parent; text: "⚙"; font.pixelSize: 16
                        color: root._advancedVisible ? ThemeEngine.accentBlue : Qt.alpha(ThemeEngine.colors.textSecondary, 0.5) }
                    MouseArea { anchors.fill: parent; onClicked: root._advancedVisible = !root._advancedVisible }
                }

                // Scheme combo
                ComboBox {
                    id: schemeCombo
                    Layout.preferredWidth: root.wide ? 88 : 72
                    Layout.preferredHeight: 30; flat: true
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11
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
                        y: schemeCombo.height; width: 210; padding: 4
                        background: Rectangle {
                            color: ThemeEngine.bgCard
                            border { width: 1; color: ThemeEngine.colors.borderCard }
                        radius: 8
                        }
                        contentItem: ListView {
                            clip: true; implicitHeight: contentHeight
                            model: schemeCombo.popup.visible ? schemeCombo.delegateModel : null
                            ScrollIndicator.vertical: ScrollIndicator {}
                        }
                    }
                    delegate: ItemDelegate {
                        width: 210; padding: 0; leftPadding: 0; rightPadding: 0
                        height: isFirst ? 52 : 32
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
                            color: highlighted ? Qt.alpha(ThemeEngine.colors.primary, 0.12) : "transparent"
                            radius: 4
                        }
                        contentItem: ColumnLayout {
                            spacing: 0
                            // ── Group header: separator + left-aligned icon label ──
                            Item {
                                Layout.fillWidth: true
                                implicitHeight: isFirst ? 20 : 0; visible: isFirst
                                // Separator line
                                Rectangle {
                                    anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 2 }
                                    height: 1; color: ThemeEngine.colors.borderCard
                                }
                                // Icon + label — left-aligned at 14px, 8px gap
                                Row {
                                    anchors { left: parent.left; leftMargin: 14; bottom: parent.bottom; bottomMargin: 2 }
                                    spacing: 8
                                    AppIcon {
                                        name: groupIcon; size: 10; color: ThemeEngine.colors.primary
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label {
                                        text: groupLabel
                                        font.family: ThemeEngine.monoFont; font.pixelSize: 8
                                        font.weight: Font.Bold; font.capitalization: Font.AllUppercase; color: ThemeEngine.textMuted
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                            // ── Scheme item: left-indented 24px ──────────
                            Label {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                text: scheme + "://"; font.family: ThemeEngine.monoFont; font.pixelSize: 12
                                color: highlighted ? ThemeEngine.colors.primary : ThemeEngine.colors.textPrimary
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 24
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

            // ── Zone 2: Group pills + Run button ────────────────────
            RowLayout {
                spacing: 4
                Repeater {
                    model: 5
                    delegate: Rectangle {
                        width: root.wide ? 36 : 28; height: 28; radius: 14
                        readonly property bool _chk: appState.isGroupAllEnabled(index)
                        color: _chk ? ThemeEngine.colors.primaryContainer : "transparent"
                        border { width: 1; color: _chk ? ThemeEngine.colors.primary : ThemeEngine.colors.borderCard }
                        Label { anchors.centerIn: parent; text: root.wide ? "G"+(index+1) : ""+(index+1)
                            font.family: ThemeEngine.monoFont; font.pixelSize: 10
                            font.weight: _chk ? Font.DemiBold : Font.Normal
                            color: _chk ? ThemeEngine.colors.primary : ThemeEngine.colors.textSecondary }
                        MouseArea { anchors.fill: parent; enabled: appState.runStatus !== 1
                            onClicked: appState.setGroupEnabled(index, !appState.isGroupAllEnabled(index)) }
                    }
                }

                // Run/Stop inline with pills
                Rectangle {
                    width: 36; height: 28; radius: 14
                    color: appState.runStatus === 1 ? ThemeEngine.failRed
                           : appState.canRun() ? ThemeEngine.accentBlue
                           : Qt.alpha(ThemeEngine.accentBlue, 0.3)
                    Label { anchors.centerIn: parent
                        text: appState.runStatus === 1 ? "■" : "▶"
                        font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: "white" }
                    MouseArea { anchors.fill: parent
                        enabled: appState.runStatus === 1 || appState.canRun()
                        onClicked: {
                            if (appState.runStatus === 1) { appState.cancel() }
                            else { if (appState.targetValidationError() !== "" || !appState.canRun()) return; appState.runDiagnostics() }
                        }
                    }
                }
            }  // end Zone 2

            // ── Zone 3: Clear button — standalone, separated from input + run ──
            AppIcon {
                name: "close"; size: 14
                color: hostField.text !== "" && appState.runStatus !== 1
                    ? Qt.alpha(ThemeEngine.failRed, 0.7) : "transparent"
                visible: hostField.text !== "" && appState.runStatus !== 1
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: { hostField.text=""; appState.targetHost=""; appState.targetPath="" } }
            }
        }  // end ROW 1

        // ═══════════════ ROW 2: Advanced fields (collapsible) ═══════════
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

        // ═══════════════ ROW 3: Port scan config ═══════════════════
        PortScanConfig {
            Layout.fillWidth: true
            visible: appState.isGroupAllEnabled(3) || appState.isGroupAnyEnabled(3)
        }
    }
}
