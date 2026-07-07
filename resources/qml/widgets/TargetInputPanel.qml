import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

ColumnLayout {
    id: root
    spacing: 0
    // ── Advanced (Port / User / Pass) toggle ──────────────────────────────
    property bool advancedExpanded: false
    // ── Scheme model — populated once in Component.onCompleted ─────────────
    property ListModel schemeModel: ListModel { id: _schemeModel }
    // ── Guard flag — suppress onTextChanged during programmatic updates ───
    property bool _syncing: false

    Component.onCompleted: {
        // ── Populate scheme model BEFORE ComboBox renders ──────────────
        var schemes = appState.supportedSchemes
        var groups = [
            {schemes: ["https","http"]},
            {schemes: ["ftp","ftps","ssh","sftp","scp"]},
            {schemes: ["smtp","smtps","imap","imaps","pop3","pop3s"]},
            {schemes: ["mysql","postgresql","redis","mongodb","mssql"]},
            {schemes: ["telnet","rdp"]},
            {schemes: ["ldap","ldaps"]},
            {schemes: ["mqtt","mqtts"]}
        ]
        for (var gi = 0; gi < groups.length; gi++) {
            var g = groups[gi]
            for (var si = 0; si < g.schemes.length; si++) {
                var s = g.schemes[si]
                if (schemes.indexOf(s) >= 0) {
                    _schemeModel.append({scheme: s, schemeGroup: gi})
                }
            }
        }
        // Select https by default in ComboBox
        for (var i = 0; i < _schemeModel.count; i++) {
            if (_schemeModel.get(i).scheme === "https") {
                schemeCombo.currentIndex = i
                break
            }
        }
    }

    RowLayout {
        AppIcon { name: "target"; size: 13; color: Qt.alpha(ThemeEngine.textPrimary, 0.7) }
        Item { width: 5 }
        Label { text: Tr.target; font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.textSecondary }
    }
    Item { Layout.preferredHeight: 6 }

    // ═══════════════════ Scheme ComboBox + Host Field ═══════════════════
    Rectangle {
        Layout.fillWidth: true; implicitHeight: 40; radius: 8
        color: ThemeEngine.bgInput
        border { width: hostField.activeFocus || schemeCombo.activeFocus ? 1.5 : 1
                 color: page._snapTargetError !== "" ? ThemeEngine.failRed
                        : (hostField.activeFocus || schemeCombo.activeFocus) ? ThemeEngine.accentBlue
                        : ThemeEngine.colors.borderCard }

        RowLayout {
            anchors { fill: parent; leftMargin: 6; rightMargin: 4 }
            AppIcon {
                name: page._snapIconName; size: 12; color: page._snapIconColor
            }
            Item { width: 2 }

            // ── Scheme ComboBox (grouped, with icons) ─────────────────────
            ComboBox {
                id: schemeCombo
                Layout.preferredWidth: 94
                Layout.fillHeight: true
                flat: true
                font.family: ThemeEngine.monoFont; font.pixelSize: 11
                enabled: appState.runStatus !== 1

                displayText: currentText + "://"

                // Theme-colored display text
                contentItem: Label {
                    text: schemeCombo.displayText
                    font: schemeCombo.font
                    color: ThemeEngine.colors.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 0
                }

                textRole: "scheme"
                model: root.schemeModel

                // ── Themed popup ──────────────────────────────────────────
                popup: Popup {
                    y: schemeCombo.height
                    width: 210
                    implicitHeight: contentItem.implicitHeight
                    padding: 4
                    background: Rectangle {
                        color: ThemeEngine.bgCard
                        border { width: 1; color: ThemeEngine.colors.borderCard }
                        radius: 8
                    }

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: schemeCombo.popup.visible ? schemeCombo.delegateModel : null
                        ScrollIndicator.vertical: ScrollIndicator {}
                    }
                }

                delegate: ItemDelegate {
                    width: 210
                    height: {
                        var prev = model.index > 0 ? root.schemeModel.get(model.index - 1) : null
                        var cur  = root.schemeModel.get(model.index)
                        if (!prev || prev.schemeGroup !== cur.schemeGroup) return 48
                        return 32
                    }
                    padding: 6; leftPadding: 12

                    readonly property bool isFirst: {
                        var prev = model.index > 0 ? root.schemeModel.get(model.index - 1) : null
                        var cur  = root.schemeModel.get(model.index)
                        return !prev || prev.schemeGroup !== cur.schemeGroup
                    }

                    // Group icon — only for headers (not per-scheme)
                    readonly property string groupIcon: ({
                        0:"globe",1:"portscan",2:"mail",3:"config",
                        4:"wifi",5:"target",6:"timer"
                    }[schemeGroup] || "circle")

                    // Group label — i18n via schemeGroup name lookup
                    readonly property string groupLabel: ({
                        0:Tr.schemeGroupWeb, 1:Tr.schemeGroupFile,
                        2:Tr.schemeGroupEmail, 3:Tr.schemeGroupDb,
                        4:Tr.schemeGroupRemote, 5:Tr.schemeGroupDir,
                        6:Tr.schemeGroupMsg
                    }[schemeGroup] || "")

                    contentItem: ColumnLayout {
                        spacing: 0
                        // ── Group header (icon + label + separator) ─────
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: isFirst ? 20 : 0
                            color: "transparent"
                            visible: isFirst
                            // Separator line
                            Rectangle {
                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                                height: 1; color: ThemeEngine.colors.borderCard
                            }
                            // Label pill (left-aligned with icon)
                            RowLayout {
                                anchors { left: parent.left; leftMargin: 0; verticalCenter: parent.verticalCenter }
                                spacing: 6
                                AppIcon {
                                    name: groupIcon; size: 12
                                    color: ThemeEngine.colors.primary
                                }
                                Label {
                                    text: groupLabel
                                    font.family: ThemeEngine.monoFont; font.pixelSize: 8
                                    font.weight: Font.Bold; color: ThemeEngine.textMuted
                                    background: Rectangle {
                                        color: ThemeEngine.bgCard
                                        anchors.fill: parent
                                        anchors.leftMargin: -2
                                        anchors.rightMargin: -4
                                    }
                                }
                            }
                        }
                        // ── Scheme row (left-indented, no icon) ─────────
                        Label {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            text: scheme + "://"
                            font.family: ThemeEngine.monoFont; font.pixelSize: 12
                            color: ThemeEngine.colors.textPrimary
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 2
                        }
                    }

                    highlighted: model.scheme === schemeCombo.currentText
                    background: Rectangle {
                        color: highlighted ? Qt.alpha(ThemeEngine.colors.primary, 0.12) : "transparent"
                        radius: 4
                    }
                }

                // ── Scheme change handler ─────────────────────────────────
                onCurrentTextChanged: {
                    if (root._syncing) return
                    if (currentText && appState.targetScheme !== currentText) {
                        appState.targetScheme = currentText
                    }
                }
            }

            // ── Host / Path field ────────────────────────────────────────
            TextField {
                id: hostField
                Layout.fillWidth: true; Layout.fillHeight: true
                font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textPrimary
                placeholderText: "example.com/path"
                placeholderTextColor: Qt.alpha(ThemeEngine.textSecondary, 0.4)
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
                    var t = text.trim()
                    // Detect pasted URL (contains ://) → parse into fields
                    if (t.indexOf("://") >= 0) {
                        appState.parseUrlIntoFields(t)
                        return
                    }
                    // Split host from path at first /
                    var slash = t.indexOf("/")
                    if (slash >= 0) {
                        appState.targetHost = t.substring(0, slash)
                        appState.targetPath = t.substring(slash)
                    } else {
                        appState.targetHost = t
                        appState.targetPath = ""
                    }
                }
            }

            // ── Clear button ────────────────────────────────────────────
            AppIcon {
                name: "close"; size: 10; color: Qt.alpha(ThemeEngine.textSecondary, 0.5)
                visible: hostField.text !== "" && appState.runStatus !== 1
                MouseArea {
                    anchors.fill: parent
                    onClicked: { hostField.text = ""; appState.targetHost = ""; appState.targetPath = "" }
                }
            }

            // ── Advanced toggle (gear) ──────────────────────────────────
            Item { width: 2; visible: appState.runStatus !== 1 }
            AppIcon {
                name: "tune"; size: 12
                color: root.advancedExpanded ? ThemeEngine.accentBlue : Qt.alpha(ThemeEngine.textSecondary, 0.5)
                visible: appState.runStatus !== 1
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.advancedExpanded = !root.advancedExpanded
                }
            }
        }
    }

    // ═══════════════════ Advanced: Port / User / Pass ═══════════════════
    Item { Layout.preferredHeight: 6; visible: root.advancedExpanded }
    RowLayout {
        visible: root.advancedExpanded
        spacing: 6
        // Port
        Rectangle {
            Layout.preferredWidth: 70; implicitHeight: 32; radius: 6
            color: Qt.alpha(ThemeEngine.bgDark, 0.4)
            border { width: 1; color: portField.activeFocus ? ThemeEngine.accentBlue : ThemeEngine.colors.borderCard }
            TextField {
                id: portField
                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textPrimary
                placeholderText: appState.defaultPortForScheme > 0 ? "" + appState.defaultPortForScheme : "Port"
                placeholderTextColor: Qt.alpha(ThemeEngine.textSecondary, 0.4)
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
            color: Qt.alpha(ThemeEngine.bgDark, 0.4)
            border { width: 1; color: userField.activeFocus ? ThemeEngine.accentBlue : ThemeEngine.colors.borderCard }
            TextField {
                id: userField
                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textPrimary
                placeholderText: Tr.usernameLabel
                placeholderTextColor: Qt.alpha(ThemeEngine.textSecondary, 0.4)
                text: appState.targetUsername
                enabled: appState.runStatus !== 1
                verticalAlignment: TextInput.AlignVCenter
                background: Item {}
                onTextChanged: appState.targetUsername = text.trim()
            }
        }
        // Password
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 32; radius: 6
            color: Qt.alpha(ThemeEngine.bgDark, 0.4)
            border { width: 1; color: passField.activeFocus ? ThemeEngine.accentBlue : ThemeEngine.colors.borderCard }
            TextField {
                id: passField
                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textPrimary
                placeholderText: Tr.passwordLabel
                placeholderTextColor: Qt.alpha(ThemeEngine.textSecondary, 0.4)
                text: appState.targetPassword
                echoMode: TextInput.Password
                enabled: appState.runStatus !== 1
                verticalAlignment: TextInput.AlignVCenter
                background: Item {}
                onTextChanged: appState.targetPassword = text.trim()
            }
        }
    }

    // ═══════════════════ Validation error ═══════════════════
    RowLayout {
        visible: page._snapTargetError !== ""
        spacing: 4
        AppIcon { name: "warning"; size: 12; color: ThemeEngine.failRed }
        Label {
            Layout.fillWidth: true
            text: page._snapTargetError || ""
            font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.failRed
            wrapMode: Text.WordWrap
        }
    }
    Item { Layout.preferredHeight: 8 }

    // ═══════════════════ Run / Stop buttons ═══════════════════
    RowLayout {
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 38; radius: 8
            color: appState.runStatus === 1 ? Qt.alpha(ThemeEngine.accentBlue, 0.4) : (appState.canRun() ? ThemeEngine.accentBlue : Qt.alpha(ThemeEngine.accentBlue, 0.3))
            Label {
                anchors.centerIn: parent
                text: appState.runStatus === 1 ? Tr.running : Tr.runDiag
                font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.DemiBold
                color: appState.canRun() || appState.runStatus === 1 ? "white" : Qt.alpha("white", 0.4)
            }
            MouseArea {
                anchors.fill: parent
                enabled: appState.runStatus !== 1 && appState.canRun()
                onClicked: {
                    if (appState.targetValidationError() !== "") return
                    if (!appState.canRun()) return
                    appState.runDiagnostics()
                }
            }
        }
        Item { width: 6; visible: appState.runStatus === 1 }
        Rectangle {
            visible: appState.runStatus === 1
            Layout.preferredWidth: 80; implicitHeight: 38; radius: 8
            color: "transparent"; border { width: 1; color: Qt.alpha(ThemeEngine.failRed, 0.5) }
            Label { anchors.centerIn: parent; text: Tr.stop; font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.failRed }
            MouseArea { anchors.fill: parent; onClicked: appState.cancel() }
        }
    }
}
