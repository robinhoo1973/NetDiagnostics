import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 0
    property int status: appState.runStatus

    RowLayout {
        spacing: 8
        AppIcon {
            name: status === 1 ? "spinner" : status === 2 ? "badge-check" : status === 3 ? "badge-close" : status === 4 ? "badge-error" : "badge-circle"
            size: 14
            color: status === 1 ? ThemeEngine.colors.primary : (status === 2 ? ThemeEngine.passGreen : (status === 3 || status === 4 ? ThemeEngine.failRed : ThemeEngine.colors.textPrimary))
            RotationAnimation on rotation { running: status === 1; from:0; to:360; duration:1000; loops:Animation.Infinite }
        }
        Label {
            text: status === 1 ? Tr.runningStatus : status === 2 ? Tr.completeStatus : status === 3 ? Tr.cancelledStatus : status === 4 ? Tr.errorStatus : Tr.readyStatus
            font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.DemiBold
            color: status === 1 ? ThemeEngine.cyan : status === 2 ? ThemeEngine.passGreen : status === 3 ? ThemeEngine.warnYellow : status === 4 ? ThemeEngine.failRed : ThemeEngine.textSecondary
        }
        AppIcon { visible: appState.errorMessage !== ""; name: "warning"; size: 14; color: ThemeEngine.failRed }
        Item { Layout.fillWidth: true }
        Label {
            visible: status === 1
            text: appState.currentDiagLabel || ""
            font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.italic: true; color: ThemeEngine.cyan
            elide: Text.ElideRight; Layout.maximumWidth: 300
        }
        Label {
            visible: appState.totalDiags > 0
            text: appState.totalCompleted + " / " + appState.totalDiags
            font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.textSecondary
        }
    }

    Label {
        visible: appState.errorMessage !== ""
        Layout.fillWidth: true; Layout.topMargin: 6
        text: Tr.errorPrefix + (appState.errorMessage || "")
        font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: Qt.alpha(ThemeEngine.failRed, 0.8)
        maximumLineCount: 2; elide: Text.ElideRight
    }
}
