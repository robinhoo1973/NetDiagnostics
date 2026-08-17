// DiagnosticToolbar.qml — 目标输入 + 运行/取消（P0 自包含版；scheme/host 与 AppState 同步）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets

Rectangle {
    id: root
    color: ThemeEngine.colors.surfaceContainer
    property bool wide: true
    signal runRequested()
    signal cancelRequested()

    implicitHeight: tbCol.implicitHeight + 8
    clip: true

    ColumnLayout {
        id: tbCol
        anchors { fill: parent; leftMargin: 4; rightMargin: 4; topMargin: 4; bottomMargin: 4 }
        spacing: ThemeEngine.spacing.xs

        RowLayout {
            Layout.fillWidth: true; spacing: ThemeEngine.spacing.sm

            // 协议下拉（分组：组图标 + 组名 + 协议项；UI 评审 7-2 恢复归档形态）
            SchemeSelector {
                id: schemeCombo
                Layout.preferredWidth: root.wide ? 104 : 88
                Layout.preferredHeight: 36
                enabled: AppState.runStatus !== 1
            }

            // 主机输入
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 36; radius: 6
                color: ThemeEngine.colors.surfaceContainerHighest
                border {
                    width: hostField.activeFocus ? 2 : 1
                    color: AppState.targetValidationErrorText !== "" ? ThemeEngine.colors.fail
                           : hostField.activeFocus ? ThemeEngine.colors.primary : ThemeEngine.colors.outlineVariant
                }
                TextField {
                    id: hostField
                    anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    font.family: ThemeEngine.monoFont; font.pixelSize: 12
                    color: ThemeEngine.colors.onSurface
                    placeholderText: "example.com"
                    placeholderTextColor: ThemeEngine.colors.textPlaceholder
                    text: { var h = AppState.targetHost; var p = AppState.targetPath; return (!h && !p) ? "" : h + p }
                    enabled: AppState.runStatus !== 1
                    verticalAlignment: TextInput.AlignVCenter
                    background: Item {}
                    onTextChanged: AppState.setTarget(text, schemeCombo.currentText)
                }
            }

            // 目标设置（协议凭据：用户名/密码/端口）
            Button {
                id: credsBtn
                Layout.preferredWidth: 44; Layout.preferredHeight: 36
                enabled: AppState.runStatus !== 1
                padding: 0
                contentItem: AppIcon {
                    name: "gear"; size: 18
                    color: credsBtn.enabled
                           ? (AppState.targetHasCredentials ? ThemeEngine.colors.primary : ThemeEngine.colors.onSurfaceVariant)
                           : ThemeEngine.colors.textMuted
                }
                background: Rectangle {
                    radius: 18
                    color: credsBtn.hovered && credsBtn.enabled
                           ? Qt.alpha(ThemeEngine.colors.primary, 0.10)
                           : "transparent"
                    border {
                        width: 1
                        color: AppState.targetHasCredentials && credsBtn.enabled
                               ? Qt.alpha(ThemeEngine.colors.primary, 0.6)
                               : ThemeEngine.colors.outlineVariant
                    }
                }
                onClicked: credsPopup.open()
                Accessible.role: Accessible.Button
                Accessible.name: T.tr("targetSettings")
            }

            // 运行/取消（pill 柔顺形）
            Button {
                id: runBtn
                Layout.preferredWidth: 48; Layout.preferredHeight: 36
                visible: AppState.runStatus !== 1
                text: "\u25B6"
                font.pixelSize: 12
                onClicked: root.runRequested()
                Accessible.role: Accessible.Button
                Accessible.name: T.tr("runDiag")
                background: Rectangle {
                    radius: 18
                    color: runBtn.hovered ? Qt.lighter(ThemeEngine.colors.primary, 1.15)
                                           : ThemeEngine.colors.primary
                    opacity: runBtn.enabled ? 1.0 : 0.5
                }
                contentItem: Text {
                    text: runBtn.text
                    font: runBtn.font
                    color: ThemeEngine.colors.onPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            Button {
                id: cancelBtn
                Layout.preferredWidth: 48; Layout.preferredHeight: 36
                visible: AppState.runStatus === 1
                text: "\u25A0"
                font.pixelSize: 12
                onClicked: root.cancelRequested()
                Accessible.role: Accessible.Button
                Accessible.name: T.tr("cellularCancel")   // "Cancel" 15 语言
                background: Rectangle {
                    radius: 18
                    color: ThemeEngine.colors.fail
                    opacity: cancelBtn.hovered ? 0.85 : 1.0
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // 校验提示（与输入框文本起始对齐）
        Label {
            visible: AppState.targetValidationErrorText !== ""
            text: T.trMsg(AppState.targetValidationErrorText)
            color: ThemeEngine.colors.fail
            font.family: ThemeEngine.fontUi; font.pixelSize: ThemeEngine.fontSize.caption
            leftPadding: ThemeEngine.spacing.sm
        }
    }

    // ── 目标设置弹窗（协议凭据：用户名/密码/端口）──
    Popup {
        id: credsPopup
        x: Math.max(4, root.width - width - 4)
        y: root.height + 4
        width: Math.min(320, root.width - 8)
        padding: 16
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: {
            userField.text = AppState.targetUser
            passField.text = AppState.targetPassword
            portField.text = AppState.targetPort
        }
        background: Rectangle {
            radius: ThemeEngine.radius.xl
            color: ThemeEngine.colors.surfaceContainerLow
            border { width: 1; color: ThemeEngine.colors.outlineVariant }
        }
        contentItem: ColumnLayout {
            spacing: ThemeEngine.spacing.md
            Label {
                text: T.tr("targetSettings")
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.subhead
                font.weight: Font.DemiBold
                color: ThemeEngine.colors.onSurface
            }
            Label {
                Layout.fillWidth: true
                text: T.tr("targetCredsHint")
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.caption
                color: ThemeEngine.colors.onSurfaceVariant
                wrapMode: Text.WordWrap
            }
            RowLayout {
                spacing: ThemeEngine.spacing.sm
                Label {
                    Layout.preferredWidth: 72
                    text: T.tr("targetUsername")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.body
                    color: ThemeEngine.colors.onSurface
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: 6
                    color: ThemeEngine.colors.surfaceContainerHighest
                    border { width: 1; color: ThemeEngine.colors.outlineVariant }
                    TextField {
                        id: userField
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        font.family: ThemeEngine.monoFont; font.pixelSize: 12
                        color: ThemeEngine.colors.onSurface
                        placeholderText: "user"
                        placeholderTextColor: ThemeEngine.colors.textPlaceholder
                        verticalAlignment: TextInput.AlignVCenter
                        background: Item {}
                    }
                }
            }
            RowLayout {
                spacing: ThemeEngine.spacing.sm
                Label {
                    Layout.preferredWidth: 72
                    text: T.tr("targetPassword")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.body
                    color: ThemeEngine.colors.onSurface
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: 6
                    color: ThemeEngine.colors.surfaceContainerHighest
                    border { width: 1; color: ThemeEngine.colors.outlineVariant }
                    TextField {
                        id: passField
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        echoMode: TextInput.Password
                        font.family: ThemeEngine.monoFont; font.pixelSize: 12
                        color: ThemeEngine.colors.onSurface
                        placeholderTextColor: ThemeEngine.colors.textPlaceholder
                        verticalAlignment: TextInput.AlignVCenter
                        background: Item {}
                    }
                }
            }
            RowLayout {
                spacing: ThemeEngine.spacing.sm
                Label {
                    Layout.preferredWidth: 72
                    text: T.tr("targetPort")
                    font.family: ThemeEngine.fontUi
                    font.pixelSize: ThemeEngine.fontSize.body
                    color: ThemeEngine.colors.onSurface
                }
                Rectangle {
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 36
                    radius: 6
                    color: ThemeEngine.colors.surfaceContainerHighest
                    border { width: 1; color: ThemeEngine.colors.outlineVariant }
                    TextField {
                        id: portField
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        font.family: ThemeEngine.monoFont; font.pixelSize: 12
                        color: ThemeEngine.colors.onSurface
                        placeholderText: "1-65535"
                        placeholderTextColor: ThemeEngine.colors.textPlaceholder
                        verticalAlignment: TextInput.AlignVCenter
                        background: Item {}
                        validator: IntValidator { bottom: 1; top: 65535 }
                    }
                }
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: ThemeEngine.spacing.sm
                Button {
                    id: popCancelBtn
                    text: T.tr("cancel")
                    onClicked: credsPopup.close()
                    background: Rectangle {
                        radius: 16
                        color: popCancelBtn.hovered ? Qt.alpha(ThemeEngine.colors.onSurfaceVariant, 0.12) : "transparent"
                        border { width: 1; color: ThemeEngine.colors.outlineVariant }
                    }
                    contentItem: Text {
                        text: popCancelBtn.text
                        font: popCancelBtn.font
                        color: ThemeEngine.colors.onSurface
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Button {
                    id: popApplyBtn
                    text: T.tr("apply")
                    onClicked: {
                        AppState.setTargetCredentials(userField.text, passField.text, portField.text)
                        credsPopup.close()
                    }
                    background: Rectangle {
                        radius: 16
                        color: popApplyBtn.hovered ? Qt.lighter(ThemeEngine.colors.primary, 1.15)
                                       : ThemeEngine.colors.primary
                    }
                    contentItem: Text {
                        text: popApplyBtn.text
                        font: popApplyBtn.font
                        color: ThemeEngine.colors.onPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
