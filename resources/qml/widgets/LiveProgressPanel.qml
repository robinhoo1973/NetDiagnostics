import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 0
    property int status: appState.runStatus

    RowLayout {
        spacing: 8
        Item {
            width: 14; height: 14
            Label {
                anchors.centerIn: parent
                text: status === 1 ? "⟳" : status === 2 ? "✓" : status === 3 ? "✗" : status === 4 ? "⚠" : "○"
                font.pixelSize: 14
                color: status === 1 ? Theme.cyan : status === 2 ? Theme.passGreen : status === 3 ? Theme.warnYellow : status === 4 ? Theme.failRed : Qt.alpha(Theme.textSecondary, 0.4)
            }
        }
        Label {
            text: status === 1 ? "Running" : status === 2 ? "Complete" : status === 3 ? "Cancelled" : status === 4 ? "Error" : "Ready"
            font.family: "JetBrains Mono"; font.pixelSize: 12; font.weight: Font.DemiBold
            color: status === 1 ? Theme.cyan : status === 2 ? Theme.passGreen : status === 3 ? Theme.warnYellow : status === 4 ? Theme.failRed : Theme.textSecondary
        }
        AppIcon { visible: appState.errorMessage !== ""; name: "warning"; size: 14; color: Theme.failRed }
        Item { Layout.fillWidth: true }
        Label {
            visible: status === 1
            text: appState.currentTestLabel || ""
            font.family: "JetBrains Mono"; font.pixelSize: 11; font.italic: true; color: Theme.cyan
            elide: Text.ElideRight; Layout.maximumWidth: 300
        }
        Label {
            visible: appState.totalTests > 0
            text: appState.totalCompleted + " / " + appState.totalTests
            font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary
        }
    }

    Label {
        visible: appState.errorMessage !== ""
        Layout.fillWidth: true; Layout.topMargin: 6
        text: "Error: " + (appState.errorMessage || "")
        font.family: "JetBrains Mono"; font.pixelSize: 10; color: Qt.alpha(Theme.failRed, 0.8)
        maximumLineCount: 2; elide: Text.ElideRight
    }
}
