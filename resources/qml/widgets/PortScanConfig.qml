import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts

// ── Flutter PortScanConfig — card with header/checkbox/range ───────────
Rectangle {
    radius: 8; color: Qt.alpha(ThemeEngine.bgCard, 0.5); border { width: 1; color: Qt.alpha(ThemeEngine.accentBlue, 0.3) }
    implicitHeight: portCol.implicitHeight + 20
    ColumnLayout {
        id: portCol
        anchors { fill: parent; margins: 10 }
        spacing: 0
        // Header
        RowLayout {
            AppIcon { name: "portscan"; size: 14; color: ThemeEngine.accentBlue }
            Item { width: 6 }
            Label { text: Tr.portScan; font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.accentBlue }
        }
        Item { Layout.preferredHeight: 6 }
        // Common ports checkbox
        RowLayout {
            CheckBox {
                id: commonCb; Layout.preferredWidth: 20; Layout.preferredHeight: 20
                enabled: appState.runStatus !== 1
                checked: appState.portScanCommon
                onCheckedChanged: appState.portScanCommon = checked
            }
            Item { width: 6 }
            Label { text: Tr.scanCommon; font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textPrimary }
        }
        Item { Layout.preferredHeight: 8 }
        // Range label
        RowLayout {
            AppIcon { name: "tune"; size: 12; color: Qt.alpha(ThemeEngine.textPrimary, 0.7) }
            Item { width: 4 }
            Label { text: Tr.range; font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textSecondary }
        }
        Item { Layout.preferredHeight: 4 }
        // From / To fields
        RowLayout {
            TextField {
                id: fromField; Layout.fillWidth: true; implicitHeight: 28
                font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textPrimary
                text: appState.portScanFrom > 0 ? appState.portScanFrom : ""
                placeholderText: "From"
                placeholderTextColor: ThemeEngine.textSecondary
                enabled: appState.runStatus !== 1
                background: Rectangle { radius: 4; color: "transparent"; border { width: 1; color: fromField.focus ? ThemeEngine.cyan : "#3A3A5A" } }
                onTextChanged: { var v = parseInt(text)||0; v = Math.max(0, Math.min(65535, v)); appState.portScanFrom = v }
            }
            Item { width: 8 }
            Label { text: "–"; font.pixelSize: 13; color: ThemeEngine.textSecondary }
            Item { width: 8 }
            TextField {
                id: toField; Layout.fillWidth: true; implicitHeight: 28
                font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textPrimary
                text: appState.portScanTo > 0 ? appState.portScanTo : ""
                placeholderText: "To"
                placeholderTextColor: ThemeEngine.textSecondary
                enabled: appState.runStatus !== 1
                background: Rectangle { radius: 4; color: "transparent"; border { width: 1; color: toField.focus ? ThemeEngine.cyan : "#3A3A5A" } }
                onTextChanged: { var v = parseInt(text)||0; v = Math.max(0, Math.min(65535, v)); appState.portScanTo = v }
            }
        }
        // Validation hint
        Label {
            visible: appState.portScanFrom > 0 && appState.portScanTo > 0 && appState.portScanFrom > appState.portScanTo
            text: Tr.fromMust; font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.failRed
        }
    }
}