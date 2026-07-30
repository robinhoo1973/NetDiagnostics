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
            id: liveSpinner
            name: status === 1 ? "spinner" : status === 2 ? "badge-check" : status === 3 ? "badge-close" : status === 4 ? "badge-error" : "badge-circle"
            size: 14
            color: status === 1 ? ThemeEngine.colors.primary : (status === 2 ? ThemeEngine.colors.passGreen : (status === 3 ? ThemeEngine.colors.failRed : (status === 4 ? ThemeEngine.colors.errorRed : ThemeEngine.colors.textPrimary)))
            RotationAnimation on rotation {
                running: status === 1; from:0; to:360; duration:1000; loops:Animation.Infinite
                // 5WHY: Reset rotation so post-spinner icon (badge-check etc.) isn't skewed.
                onStopped: liveSpinner.rotation = 0
            }
        }
        Label {
            text: status === 1 ? Tr.runningStatus : status === 2 ? Tr.completeStatus : status === 3 ? Tr.cancelledStatus : status === 4 ? Tr.errorStatus : Tr.readyStatus
            font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.DemiBold
            color: status === 1 ? ThemeEngine.colors.cyan : status === 2 ? ThemeEngine.colors.passGreen : status === 3 ? ThemeEngine.colors.warnYellow : status === 4 ? ThemeEngine.colors.errorRed : ThemeEngine.colors.textSecondary
        }
        AppIcon { visible: appState.errorMessage !== ""; name: "warning"; size: 14; color: ThemeEngine.colors.errorRed }
        Item { Layout.fillWidth: true }
        Label {
            visible: status === 1
            text: appState.currentDiagLabel || ""
            font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.italic: true; color: ThemeEngine.colors.cyan
            elide: Text.ElideRight; Layout.maximumWidth: 300
        }
        Label {
            visible: appState.totalDiags > 0
            text: appState.totalCompleted + " / " + appState.totalDiags
            font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.colors.textSecondary
        }
    }

    Label {
        visible: appState.errorMessage !== ""
        Layout.fillWidth: true; Layout.topMargin: 6
        text: Tr.errorPrefix + (appState.errorMessage || "")
        font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: Qt.alpha(ThemeEngine.colors.errorRed, 0.8)
        maximumLineCount: 2; elide: Text.ElideRight
    }
}
