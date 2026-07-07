import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

// ── Diagnostic Toolbar — scheme combo + host + Run/Stop + group pills + port scan toggle
Rectangle {
    id: root
    color: ThemeEngine.bgSidebar

    // ── Expanded port scan config ──────────────────────────────────────
    property alias portScanExpanded: portScanPopup.visible

    implicitHeight: portScanPopup.visible ? toolbarRow.implicitHeight + portScanPopup.implicitHeight + 8 : 44
    clip: true

    ColumnLayout {
        anchors { fill: parent; leftMargin: 8; rightMargin: 8; topMargin: 4; bottomMargin: 4 }
        spacing: 4

        // ── Main toolbar row ───────────────────────────────────────────
        RowLayout {
            id: toolbarRow
            Layout.fillWidth: true; spacing: page.wide ? 4 : 2

            // Scheme combo (narrower than before)
            ComboBox {
                id: schemeCombo
                Layout.preferredWidth: page.wide ? 84 : 72
                Layout.preferredHeight: 32
                flat: true
                font.family: ThemeEngine.monoFont; font.pixelSize: 11
                enabled: appState.runStatus !== 1
                displayText: currentText + "://"

                contentItem: Label {
                    text: schemeCombo.displayText
                    font: schemeCombo.font
                    color: ThemeEngine.colors.textPrimary
                    verticalAlignment: Text.AlignVCenter
                }

                textRole: "scheme"
                model: ListModel {
                    id: _tbModel
                    Component.onCompleted: {
                        var schemes = appState.supportedSchemes
                        for (var i = 0; i < schemes.length; i++)
                            _tbModel.append({scheme: schemes[i]})
                        for (var i = 0; i < _tbModel.count; i++) {
                            if (_tbModel.get(i).scheme === "https") {
                                schemeCombo.currentIndex = i; break
                            }
                        }
                    }
                }

                popup: Popup {
                    y: schemeCombo.height
                    width: 200; padding: 4
                    background: Rectangle {
                        color: ThemeEngine.bgCard
                        border { width: 1; color: ThemeEngine.colors.borderCard }
                        radius: 8
                    }
                    contentItem: ListView {
                        clip: true; implicitHeight: contentHeight
                        model: schemeCombo.popup.visible ? schemeCombo.delegateModel : null
                    }
                }

                delegate: ItemDelegate {
                    width: 192; implicitHeight: 30; padding: 4; leftPadding: 10
                    contentItem: Label {
                        text: scheme + "://"
                        font.family: ThemeEngine.monoFont; font.pixelSize: 12
                        color: ThemeEngine.colors.textPrimary
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: scheme === schemeCombo.currentText
                    background: Rectangle {
                        color: highlighted ? Qt.alpha(ThemeEngine.colors.primary, 0.12) : "transparent"
                        radius: 4
                    }
                }

                onCurrentTextChanged: {
                    if (currentText && appState.targetScheme !== currentText)
                        appState.targetScheme = currentText
                }
            }

            // Host field
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 32; radius: 6
                color: ThemeEngine.bgInput
                border {
                    width: hostField.activeFocus ? 1.5 : 1
                    color: page._snapTargetError !== "" ? ThemeEngine.failRed
                           : hostField.activeFocus ? ThemeEngine.accentBlue
                           : ThemeEngine.colors.borderCard
                }
                TextField {
                    id: hostField
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4
                    font.family: ThemeEngine.monoFont; font.pixelSize: 12
                    color: ThemeEngine.colors.textPrimary
                    placeholderText: "example.com/path"
                    placeholderTextColor: Qt.alpha(ThemeEngine.colors.textSecondary, 0.4)
                    text: {
                        var h = appState.targetHost; var p = appState.targetPath
                        return (!h && !p) ? "" : h + p
                    }
                    enabled: appState.runStatus !== 1
                    verticalAlignment: TextInput.AlignVCenter
                    background: Item {}
                    onTextChanged: {
                        var t = text.trim()
                        if (t.indexOf("://") >= 0) { appState.parseUrlIntoFields(t); return }
                        var slash = t.indexOf("/")
                        if (slash >= 0) {
                            appState.targetHost = t.substring(0, slash)
                            appState.targetPath = t.substring(slash)
                        } else {
                            appState.targetHost = t; appState.targetPath = ""
                        }
                    }
                }
            }

            // Clear button
            AppIcon {
                name: "close"; size: 10
                color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
                visible: hostField.text !== "" && appState.runStatus !== 1
                MouseArea {
                    anchors.fill: parent
                    onClicked: { hostField.text = ""; appState.targetHost = ""; appState.targetPath = "" }
                }
            }

            // Run button
            Rectangle {
                Layout.preferredWidth: page.wide ? 70 : 40; Layout.preferredHeight: 32; radius: 6
                color: appState.runStatus === 1 ? Qt.alpha(ThemeEngine.accentBlue, 0.4)
                       : appState.canRun() ? ThemeEngine.accentBlue
                       : Qt.alpha(ThemeEngine.accentBlue, 0.3)
                Label {
                    anchors.centerIn: parent
                    text: appState.runStatus === 1 ? (page.wide ? Tr.runningDots : "▶") : Tr.runDiag
                    font.family: ThemeEngine.monoFont; font.pixelSize: page.wide ? 11 : 10
                    font.weight: Font.DemiBold
                    color: appState.canRun() || appState.runStatus === 1 ? ThemeEngine.colors.textPrimary
                                                                          : Qt.alpha(ThemeEngine.colors.textSecondary, 0.4)
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

            // Stop button — visible only during run
            Rectangle {
                visible: appState.runStatus === 1
                Layout.preferredWidth: page.wide ? 60 : 36; Layout.preferredHeight: 32; radius: 6
                color: "transparent"
                border { width: 1; color: Qt.alpha(ThemeEngine.failRed, 0.5) }
                Label {
                    anchors.centerIn: parent
                    text: page.wide ? Tr.stop : "■"
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11
                    color: ThemeEngine.failRed
                }
                MouseArea { anchors.fill: parent; onClicked: appState.cancel() }
            }

            // ── Group filter pills (G1-G5) ─────────────────────────────
            Repeater {
                model: 5
                delegate: Rectangle {
                    Layout.preferredWidth: page.wide ? 44 : 32
                    Layout.preferredHeight: 28; radius: 14
                    property bool _chk: [page._snapG0chk, page._snapG1chk, page._snapG2chk, page._snapG3chk, page._snapG4chk][index]
                    color: _chk ? ThemeEngine.colors.primaryContainer : "transparent"
                    border {
                        width: 1
                        color: _chk ? ThemeEngine.colors.primary : ThemeEngine.colors.borderCard
                    }
                    Label {
                        anchors.centerIn: parent
                        text: "G" + (index + 1)
                        font.family: ThemeEngine.monoFont; font.pixelSize: 10
                        font.weight: _chk ? Font.DemiBold : Font.Normal
                        color: _chk ? ThemeEngine.colors.primary : ThemeEngine.colors.textSecondary
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !page._runActive
                        onClicked: appState.setGroupEnabled(index, !appState.isGroupAllEnabled(index))
                    }
                }
            }

            // Port scan toggle (collapsible gear)
            AppIcon {
                name: "portscan"; size: 14
                color: portScanPopup.visible ? ThemeEngine.accentBlue
                                              : Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
                visible: page._snapG3chk
                MouseArea {
                    anchors.fill: parent
                    onClicked: portScanPopup.visible = !portScanPopup.visible
                }
            }
        }

        // ── Expanded port scan config ───────────────────────────────────
        Popup {
            id: portScanPopup
            visible: false; closePolicy: Popup.NoAutoClose
            y: toolbarRow.height + 4; x: 0
            width: parent.width; padding: 8
            background: Rectangle {
                color: ThemeEngine.bgCard
                border { width: 1; color: ThemeEngine.colors.borderCard }
                radius: 8
            }
            contentItem: PortScanConfig {
                anchors.fill: parent
            }
        }
    }
}
