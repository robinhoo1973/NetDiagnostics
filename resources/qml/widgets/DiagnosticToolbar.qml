import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

Rectangle {
    id: root
    color: ThemeEngine.bgSidebar
    property bool wide: true
    property bool _portScanVisible: false
    property bool _advancedVisible: false

    // Does the current scheme support user/pass fields?
    readonly property bool _schemeHasUser: {
        var s = appState.targetScheme
        return s === "ftp" || s === "ftps" || s === "ssh" || s === "sftp" || s === "scp"
            || s === "mysql" || s === "postgresql" || s === "redis" || s === "mongodb" || s === "mssql"
    }
    readonly property bool _schemeHasPass: {
        var s = appState.targetScheme
        return s === "smtp" || s === "smtps" || s === "imap" || s === "imaps"
            || s === "pop3" || s === "pop3s"
            || s === "mysql" || s === "postgresql" || s === "redis" || s === "mongodb" || s === "mssql"
            || s === "ftp" || s === "ftps"
    }

    implicitHeight: tbCol.implicitHeight + 8
    clip: true

    ColumnLayout {
        id: tbCol
        anchors { fill: parent; leftMargin: 4; rightMargin: 4; topMargin: 4; bottomMargin: 4 }
        spacing: 2

        // ═══════════ ROW 1: Scheme + Host + Run/Stop ═══════════
        RowLayout {
            Layout.fillWidth: true; spacing: 4

            // Schema combo with themed popup
            ComboBox {
                id: schemeCombo
                Layout.preferredWidth: root.wide ? 88 : 72
                Layout.preferredHeight: 32; flat: true
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
                        for (var gi = 0; gi < groups.length; gi++) {
                            for (var si = 0; si < groups[gi].schemes.length; si++) {
                                var s = groups[gi].schemes[si]
                                if (schemes.indexOf(s) >= 0) _sm.append({scheme:s, schemeGroup:gi})
                            }
                        }
                        for (var i = 0; i < _sm.count; i++) {
                            if (_sm.get(i).scheme === "https") { schemeCombo.currentIndex = i; break }
                        }
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
                    width: 210
                    height: isFirst ? 48 : 32; padding: 6; leftPadding: 12
                    readonly property bool isFirst: {
                        var prev = model.index > 0 ? _sm.get(model.index - 1) : null
                        var cur = _sm.get(model.index)
                        return !prev || prev.schemeGroup !== cur.schemeGroup
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

                    background: Rectangle {
                        color: highlighted ? Qt.alpha(ThemeEngine.colors.primary, 0.12) : "transparent"
                        radius: 4
                    }
                    highlighted: scheme === schemeCombo.currentText

                    contentItem: ColumnLayout {
                        spacing: 0
                        // Group header row
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: isFirst ? 18 : 0
                            color: "transparent"; visible: isFirst
                            Rectangle {
                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                                height: 1; color: ThemeEngine.colors.borderCard
                            }
                            RowLayout {
                                anchors.centerIn: parent; spacing: 4
                                AppIcon { name: groupIcon; size: 12; color: ThemeEngine.colors.primary }
                                Label {
                                    text: groupLabel
                                    font.family: ThemeEngine.monoFont; font.pixelSize: 8
                                    font.weight: Font.Bold; color: ThemeEngine.textMuted
                                    background: Rectangle { color: ThemeEngine.bgCard; anchors.fill: parent }
                                }
                            }
                        }
                        // Scheme name row
                        Label {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            text: scheme + "://"
                            font.family: ThemeEngine.monoFont; font.pixelSize: 12
                            color: ThemeEngine.colors.textPrimary; verticalAlignment: Text.AlignVCenter
                            leftPadding: 2
                        }
                    }
                }

                onCurrentTextChanged: {
                    if (currentText && appState.targetScheme !== currentText)
                        appState.targetScheme = currentText
                }
            }

            // Host field with built-in clear button
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 32; radius: 6
                color: ThemeEngine.bgInput
                border {
                    width: hostField.activeFocus ? 1.5 : 1
                    color: appState.targetValidationError() !== "" ? ThemeEngine.failRed
                           : hostField.activeFocus ? ThemeEngine.accentBlue : ThemeEngine.colors.borderCard
                }
                TextField {
                    id: hostField
                    anchors { fill: parent; leftMargin: 8; rightMargin: 26 }
                    font.family: ThemeEngine.monoFont; font.pixelSize: 12
                    color: ThemeEngine.colors.textPrimary
                    placeholderText: "example.com/path"
                    placeholderTextColor: Qt.alpha(ThemeEngine.colors.textSecondary, 0.4)
                    text: { var h = appState.targetHost; var p = appState.targetPath; return (!h && !p) ? "" : h + p }
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
                // Clear X — anchored inside the host field
                AppIcon {
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 6 }
                    name: "close"; size: 10
                    color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
                    visible: hostField.text !== "" && appState.runStatus !== 1
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { hostField.text = ""; appState.targetHost = ""; appState.targetPath = "" }
                    }
                }
            }

            // Run/Stop toggle
            Rectangle {
                Layout.preferredWidth: 36; Layout.preferredHeight: 32; radius: 6
                color: appState.runStatus === 1 ? ThemeEngine.failRed
                       : appState.canRun() ? ThemeEngine.accentBlue
                       : Qt.alpha(ThemeEngine.accentBlue, 0.3)
                Label {
                    anchors.centerIn: parent
                    text: appState.runStatus === 1 ? "■" : "▶"
                    font.family: ThemeEngine.monoFont; font.pixelSize: 14; color: "white"
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: appState.runStatus === 1 || appState.canRun()
                    onClicked: {
                        if (appState.runStatus === 1) { appState.cancel() }
                        else { if (appState.targetValidationError() !== "" || !appState.canRun()) return; appState.runDiagnostics() }
                    }
                }
            }
        }

        // ═══════════ ROW 2: Pills ═══════════════════════════
        Flow {
            Layout.fillWidth: true; spacing: 4
            Layout.preferredHeight: childrenRect.height > 0 ? 30 : 0

            Repeater {
                model: 5
                delegate: Rectangle {
                    width: root.wide ? 44 : 32; height: 28; radius: 14
                    readonly property bool _chk: appState.isGroupAllEnabled(index)
                    color: _chk ? ThemeEngine.colors.primaryContainer : "transparent"
                    border { width: 1; color: _chk ? ThemeEngine.colors.primary : ThemeEngine.colors.borderCard }
                    Label {
                        anchors.centerIn: parent; text: "G" + (index + 1)
                        font.family: ThemeEngine.monoFont; font.pixelSize: 10
                        font.weight: _chk ? Font.DemiBold : Font.Normal
                        color: _chk ? ThemeEngine.colors.primary : ThemeEngine.colors.textSecondary
                    }
                    MouseArea {
                        anchors.fill: parent; enabled: appState.runStatus !== 1
                        onClicked: appState.setGroupEnabled(index, !appState.isGroupAllEnabled(index))
                    }
                }
            }

            // Port scan toggle pill — always visible
            Rectangle {
                width: 72; height: 28; radius: 14
                color: root._portScanVisible ? ThemeEngine.colors.primaryContainer : "transparent"
                border { width: 1; color: root._portScanVisible ? ThemeEngine.colors.primary : ThemeEngine.colors.borderCard }
                RowLayout { anchors.centerIn: parent; spacing: 4
                    AppIcon { name: "portscan"; size: 12
                        color: root._portScanVisible ? ThemeEngine.colors.primary : ThemeEngine.colors.textSecondary }
                    Label { text: "Port"; font.family: ThemeEngine.monoFont; font.pixelSize: 10
                        color: root._portScanVisible ? ThemeEngine.colors.primary : ThemeEngine.colors.textSecondary }
                }
                MouseArea { anchors.fill: parent; onClicked: root._portScanVisible = !root._portScanVisible }
            }

            // Advanced fields toggle pill
            Rectangle {
                width: 28; height: 28; radius: 14
                color: root._advancedVisible ? ThemeEngine.colors.primaryContainer : "transparent"
                border { width: 1; color: root._advancedVisible ? ThemeEngine.colors.primary : ThemeEngine.colors.borderCard }
                Label { anchors.centerIn: parent; text: "⚙"; font.pixelSize: 14
                    color: root._advancedVisible ? ThemeEngine.colors.primary : ThemeEngine.colors.textSecondary }
                MouseArea { anchors.fill: parent; onClicked: root._advancedVisible = !root._advancedVisible }
            }
        }

        // ═══════════ ROW 3: Port scan config ═══════════════════
        PortScanConfig {
            Layout.fillWidth: true
            visible: root._portScanVisible
        }

        // ═══════════ ROW 4: Advanced fields ═══════════════
        GridLayout {
            visible: root._advancedVisible
            columns: root.wide ? 3 : 2; Layout.fillWidth: true
            columnSpacing: 6; rowSpacing: 4

            // Port field — always show when advanced is open
            Rectangle {
                Layout.preferredWidth: root.wide ? 80 : 70; implicitHeight: 32; radius: 6
                color: ThemeEngine.bgInput
                border { width: 1; color: portField.activeFocus ? ThemeEngine.accentBlue : ThemeEngine.colors.borderCard }
                TextField {
                    id: portField
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
                    placeholderText: appState.defaultPortForScheme > 0 ? "" + appState.defaultPortForScheme : "Port"
                    placeholderTextColor: Qt.alpha(ThemeEngine.colors.textSecondary, 0.4)
                    text: appState.targetPort > 0 ? "" + appState.targetPort : ""
                    enabled: appState.runStatus !== 1; verticalAlignment: TextInput.AlignVCenter; background: Item {}
                    onTextChanged: { var v = parseInt(text); appState.targetPort = isNaN(v) ? -1 : v }
                }
            }

            // Username — schema-aware visibility
            Rectangle {
                visible: root._schemeHasUser
                Layout.fillWidth: true; implicitHeight: 32; radius: 6
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

            // Password — schema-aware visibility
            Rectangle {
                visible: root._schemeHasPass
                Layout.fillWidth: true; implicitHeight: 32; radius: 6
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
