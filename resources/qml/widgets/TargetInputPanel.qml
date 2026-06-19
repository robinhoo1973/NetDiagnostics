import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 0
    property string _iconName: "circle"
    property color _iconColor: Qt.alpha(Theme.textSecondary, 0.4)
    Timer {
        interval: 200; running: true; repeat: true
        onTriggered: {
            // Call debug method (outputs to stderr for diagnosis)
            var isUrl = appState.isTargetUrl()
            appState.debugTargetUrl()
            if (appState.targetValidationError() !== "") { _iconName="error"; _iconColor=Theme.failRed }
            else if (appState.isTargetEmpty()) { _iconName="circle"; _iconColor=Qt.alpha(Theme.textSecondary,0.4) }
            else if (isUrl) { _iconName="globe"; _iconColor=Theme.accentBlue }
            else { _iconName="target"; _iconColor=Theme.passGreen }
        }
    }
    RowLayout {
        AppIcon { name: "target"; size: 13; color: Qt.alpha(Theme.textSecondary, 0.7) }
        Item { width: 5 }
        Label { text: "Target"; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
    }
    Item { Layout.preferredHeight: 6 }

    Rectangle {
        Layout.fillWidth: true; implicitHeight: 40; radius: 8
        color: Qt.alpha(Theme.bgDark, 0.6)
        border { width: targetField.activeFocus ? 1.5 : 1; color: appState.targetValidationError !== "" ? Theme.failRed : (targetField.activeFocus ? Theme.accentBlue : "#3A3A5A") }
        RowLayout {
            anchors { fill: parent; leftMargin: 10; rightMargin: 4 }
            AppIcon {
                name: root._iconName
                size: 18
                color: root._iconColor
            }
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
    // RFC validation error (shown when target is invalid)
    property string _error: { let _dummy = appState.target; return appState.targetValidationError() }
    Label {
        visible: _error !== ""
        Layout.fillWidth: true
        text: "⚠ " + (_error || "")
        font.family: "JetBrains Mono"; font.pixelSize: 10; color: Theme.failRed
        wrapMode: Text.WordWrap
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
                    // Block Run if target has validation errors (invalid scheme, etc.)
                    if (appState.targetValidationError() !== "") return
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
