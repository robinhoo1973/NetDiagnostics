import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    spacing: 0

    RowLayout {
        AppIcon { name: "target"; size: 13; color: Qt.alpha(Theme.textSecondary, 0.7) }
        Item { width: 5 }
        Label { text: "Target"; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
    }
    Item { Layout.preferredHeight: 6 }

    Rectangle {
        Layout.fillWidth: true; implicitHeight: 40; radius: 8
        color: Qt.alpha(Theme.bgDark, 0.6)
        border { width: targetField.activeFocus ? 1.5 : 1; color: targetField.activeFocus ? Theme.accentBlue : "#3A3A5A" }
        RowLayout {
            anchors { fill: parent; leftMargin: 10; rightMargin: 4 }
            AppIcon { name: "globe"; size: 18; color: targetField.activeFocus ? Theme.accentBlue : Qt.alpha(Theme.textSecondary, 0.6) }
            Item { width: 8 }
            TextField {
                id: targetField
                Layout.fillWidth: true; Layout.fillHeight: true
                font.family: "JetBrains Mono"; font.pixelSize: 12; color: Theme.textPrimary
                placeholderText: "Enter URL, IP address, or hostname..."
                placeholderTextColor: Qt.alpha(Theme.textSecondary, 0.4)
                text: appState.target
                enabled: appState.runStatus !== 1
                verticalAlignment: TextInput.AlignVCenter
                background: Item {}
                onTextChanged: appState.target = text.trim()
            }
            Label {
                text: "✕"; font.pixelSize: 16; color: Qt.alpha(Theme.textSecondary, 0.5)
                visible: targetField.text !== "" && appState.runStatus !== 1
                MouseArea {
                    anchors.fill: parent
                    onClicked: { targetField.text = ""; appState.target = "" }
                }
            }
        }
    }
    Item { Layout.preferredHeight: 8 }

    RowLayout {
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 38; radius: 8
            color: appState.runStatus === 1 ? Qt.alpha(Theme.accentBlue, 0.4) : Theme.accentBlue
            Label {
                anchors.centerIn: parent
                text: appState.runStatus === 1 ? "⏳ Running..." : "▶ Run Diagnostics"
                font.family: "JetBrains Mono"; font.pixelSize: 12; font.weight: Font.DemiBold
                color: "white"
            }
            MouseArea {
                anchors.fill: parent
                enabled: appState.runStatus !== 1
                onClicked: {
                    var t = targetField.text.trim()
                    appState.target = t
                    if (t === "") {
                        // AppState will set errorMessage and show Error status
                    }
                    appState.runDiagnostics()
                }
            }
        }
        Item { width: 6; visible: appState.runStatus === 1 }
        Rectangle {
            visible: appState.runStatus === 1
            Layout.preferredWidth: 80; implicitHeight: 38; radius: 8
            color: "transparent"; border { width: 1; color: Qt.alpha(Theme.failRed, 0.5) }
            Label { anchors.centerIn: parent; text: "■ Stop"; font.family: "JetBrains Mono"; font.pixelSize: 11; color: Theme.failRed }
            MouseArea { anchors.fill: parent; onClicked: appState.cancel() }
        }
    }
}
