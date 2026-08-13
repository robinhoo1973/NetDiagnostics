// DiagnosticToolbar.qml — 目标输入 + 运行/取消（P0 自包含版；scheme/host 与 AppState 同步）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

Rectangle {
    id: root
    color: ThemeEngine.colors.sidebar
    property bool wide: true
    signal runRequested()
    signal cancelRequested()

    implicitHeight: tbCol.implicitHeight + 8
    clip: true

    ColumnLayout {
        id: tbCol
        anchors { fill: parent; leftMargin: 4; rightMargin: 4; topMargin: 4; bottomMargin: 4 }
        spacing: 2

        RowLayout {
            Layout.fillWidth: true; spacing: 6

            // 协议下拉
            ComboBox {
                id: schemeCombo
                Layout.preferredWidth: root.wide ? 96 : 80
                Layout.preferredHeight: 36
                model: AppState.supportedSchemes()
                font.family: ThemeEngine.monoFont; font.pixelSize: 12
                enabled: AppState.runStatus !== 1
                Component.onCompleted: currentIndex = Math.max(0, model.indexOf("https"))
                onActivated: AppState.setTarget(AppState.targetHost + AppState.targetPath, currentText)
            }

            // 主机输入
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 36; radius: 6
                color: ThemeEngine.colors.input
                border {
                    width: hostField.activeFocus ? 2 : 1
                    color: AppState.targetValidationErrorText !== "" ? ThemeEngine.colors.failRed
                           : hostField.activeFocus ? ThemeEngine.colors.borderFocused : ThemeEngine.colors.borderCard
                }
                TextField {
                    id: hostField
                    anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    font.family: ThemeEngine.monoFont; font.pixelSize: 12
                    color: ThemeEngine.colors.textPrimary
                    placeholderText: "example.com"
                    placeholderTextColor: ThemeEngine.colors.textPlaceholder
                    text: { var h = AppState.targetHost; var p = AppState.targetPath; return (!h && !p) ? "" : h + p }
                    enabled: AppState.runStatus !== 1
                    verticalAlignment: TextInput.AlignVCenter
                    background: Item {}
                    onTextChanged: AppState.setTarget(text, schemeCombo.currentText)
                }
            }

            // 运行/取消
            Button {
                id: runBtn
                Layout.preferredWidth: 44; Layout.preferredHeight: 36
                visible: AppState.runStatus !== 1
                text: "\u25B6"
                font.pixelSize: 12
                onClicked: root.runRequested()
                Accessible.name: "run"
            }
            Button {
                Layout.preferredWidth: 44; Layout.preferredHeight: 36
                visible: AppState.runStatus === 1
                text: "\u25A0"
                font.pixelSize: 12
                onClicked: root.cancelRequested()
                Accessible.name: "cancel"
            }
        }

        // 校验提示
        Label {
            visible: AppState.targetValidationErrorText !== ""
            text: T.trMsg(AppState.targetValidationErrorText)
            color: ThemeEngine.colors.failRed
            font.family: ThemeEngine.fontUi; font.pixelSize: ThemeEngine.fontSize.caption
            leftPadding: 8
        }
    }
}
