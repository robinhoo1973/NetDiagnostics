import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

// ── Diagnostic Toolbar — scheme combo + host + Run/Stop + group pills + port scan toggle
Rectangle {
    id: root
    color: ThemeEngine.bgSidebar
    property bool wide: true

    implicitHeight: tbCol.implicitHeight + 8
    clip: true

    ColumnLayout {
        id: tbCol
        anchors { fill: parent; leftMargin: 8; rightMargin: 8; topMargin: 4; bottomMargin: 4 }
        spacing: 4

        // ── Main toolbar row ───────────────────────────────────────────
        RowLayout {
            id: toolbarRow
            Layout.fillWidth: true; spacing: root.wide ? 4 : 2

            // Scheme combo (narrower than before)
            ComboBox {
                id: schemeCombo
                Layout.preferredWidth: root.wide ? 84 : 72
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
                    color: appState.targetValidationError() !== "" ? ThemeEngine.failRed
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
                Layout.preferredWidth: root.wide ? 70 : 40; Layout.preferredHeight: 32; radius: 6
                color: appState.runStatus === 1 ? Qt.alpha(ThemeEngine.accentBlue, 0.4)
                       : appState.canRun() ? ThemeEngine.accentBlue
                       : Qt.alpha(ThemeEngine.accentBlue, 0.3)
                Label {
                    anchors.centerIn: parent
                    text: appState.runStatus === 1 ? (root.wide ? Tr.runningDots : "▶") : Tr.runDiag
                    font.family: ThemeEngine.monoFont; font.pixelSize: root.wide ? 11 : 10
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
                Layout.preferredWidth: root.wide ? 60 : 36; Layout.preferredHeight: 32; radius: 6
                color: "transparent"
                border { width: 1; color: Qt.alpha(ThemeEngine.failRed, 0.5) }
                Label {
                    anchors.centerIn: parent
                    text: root.wide ? Tr.stop : "■"
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11
                    color: ThemeEngine.failRed
                }
                MouseArea { anchors.fill: parent; onClicked: appState.cancel() }
            }

            // ── Group filter pills (G1-G5) ─────────────────────────────
            Repeater {
                model: 5
                delegate: Rectangle {
                    Layout.preferredWidth: root.wide ? 44 : 32
                    Layout.preferredHeight: 28; radius: 14
                    property bool _chk: appState.isGroupAllEnabled(index)
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
                        enabled: appState.runStatus !== 1
                        onClicked: appState.setGroupEnabled(index, !appState.isGroupAllEnabled(index))
                    }
                }
            }

            // Port scan toggle
            AppIcon {
                name: "portscan"; size: 14
                color: root._portScanVisible ? ThemeEngine.accentBlue
                                              : Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
                visible: appState.isGroupAllEnabled(3) || appState.isGroupAnyEnabled(3)
                MouseArea {
                    anchors.fill: parent
                    onClicked: root._portScanVisible = !root._portScanVisible
                }
            }
        }
    }

        // ── Expandable port scan config (toggled by icon) ────────────────
        PortScanConfig {
            Layout.fillWidth: true
            visible: root._portScanVisible
        }
    }
    property bool _portScanVisible: false
    }
}
