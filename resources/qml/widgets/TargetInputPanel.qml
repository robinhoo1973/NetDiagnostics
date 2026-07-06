import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

ColumnLayout {
    id: root
    spacing: 0
    RowLayout {
        AppIcon { name: "target"; size: 13; color: Qt.alpha(ThemeEngine.textPrimary, 0.7) }
        Item { width: 5 }
        Label { text: Tr.target; font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.textSecondary }
    }
    Item { Layout.preferredHeight: 6 }

    Rectangle {
        Layout.fillWidth: true; implicitHeight: 40; radius: 8
        color: Qt.alpha(ThemeEngine.bgDark, 0.6)
        border { width: targetField.activeFocus ? 1.5 : 1; color: page._snapTargetError !== "" ? ThemeEngine.failRed : (targetField.activeFocus ? ThemeEngine.accentBlue : ThemeEngine.colors.borderCard) }
        RowLayout {
            anchors { fill: parent; leftMargin: 10; rightMargin: 4 }
            AppIcon {
                name: page._snapIconName
                size: 12
                color: page._snapIconColor
            }
            Item { width: 8 }
            TextField {
                id: targetField
                Layout.fillWidth: true; Layout.fillHeight: true
                font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textPrimary
                placeholderText: Tr.enterTarget
                placeholderTextColor: Qt.alpha(ThemeEngine.textSecondary, 0.4)
                text: appState.target
                enabled: appState.runStatus !== 1
                verticalAlignment: TextInput.AlignVCenter
                background: Item {}
                onTextChanged: appState.target = text.trim()
            }
            AppIcon {
                name: "close"; size: 10; color: Qt.alpha(ThemeEngine.textSecondary, 0.5)
                visible: targetField.text !== "" && appState.runStatus !== 1
                MouseArea {
                    anchors.fill: parent
                    onClicked: { targetField.text = "" }
                }
            }
        }
    }
    // RFC validation error (shown when target is invalid — snapshot via parent)
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
                    // Block Run: validation error, or no tests enabled
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