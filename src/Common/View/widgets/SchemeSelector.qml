import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

// ── Shared scheme selector for every target-entry surface ──────────────────
// 5WHY: TargetInputPanel and DiagnosticToolbar each maintained their own
// scheme list, group icons, group labels, popup implementation, and default
// selection.  Those copies already drifted in presentation and could select
// different schemes for the same AppState target.  One selector owns the
// catalogue and follows AppState.targetScheme in both locations.
ComboBox {
    id: root

    property ListModel schemeModel: ListModel {}
    property bool _synchronizing: false
    // 5WHY: the toolbar used to own a second, hand-maintained list of
    // authentication-capable schemes.  The policy now lives in C++
    // (G5WebsiteUrl::schemeSupportsUsername/Password, exposed via AppState)
    // beside defaultPorts(), so a newly added protocol automatically gets
    // consistent menu + credential-field behaviour on every platform.
    readonly property bool supportsUsername: appState.schemeSupportsUsername(currentText)
    readonly property bool supportsPassword: appState.schemeSupportsPassword(currentText)

    textRole: "scheme"
    model: schemeModel
    displayText: currentText ? currentText + "://" : ""

    contentItem: Label {
        text: root.displayText
        font: root.font
        color: ThemeEngine.colors.textPrimary
        verticalAlignment: Text.AlignVCenter
        leftPadding: 0
    }

    function groupIcon(group) {
        return ({
            0: "globe", 1: "file-transfer", 2: "mail", 3: "database",
            4: "wifi", 5: "circle", 6: "timer"
        }[group] || "circle")
    }

    function groupLabel(group) {
        return ({
            0: T.tr("schemeGroupWeb"), 1: T.tr("schemeGroupFile"),
            2: T.tr("schemeGroupEmail"), 3: T.tr("schemeGroupDb"),
            4: T.tr("schemeGroupRemote"), 5: T.tr("schemeGroupDir"),
            6: T.tr("schemeGroupMsg")
        }[group] || "")
    }

    function synchronizeCurrentScheme() {
        if (schemeModel.count === 0) return

        var previousSyncState = _synchronizing
        _synchronizing = true
        var targetScheme = appState.targetScheme
        for (var i = 0; i < schemeModel.count; ++i) {
            if (schemeModel.get(i).scheme === targetScheme) {
                if (currentIndex !== i) currentIndex = i
                break
            }
        }
        _synchronizing = previousSyncState
    }

    function populateSchemeModel() {
        _synchronizing = true
        schemeModel.clear()

        var supportedSchemes = appState.supportedSchemes
        var schemeGroups = [
            ["https", "http"],
            ["ftp", "ftps", "ssh", "sftp", "scp"],
            ["smtp", "smtps", "imap", "imaps", "pop3", "pop3s"],
            ["mysql", "postgresql", "redis", "mongodb", "mssql"],
            ["telnet", "rdp"],
            ["ldap", "ldaps"],
            ["mqtt", "mqtts"]
        ]
        for (var group = 0; group < schemeGroups.length; ++group) {
            for (var index = 0; index < schemeGroups[group].length; ++index) {
                var scheme = schemeGroups[group][index]
                if (supportedSchemes.indexOf(scheme) >= 0)
                    schemeModel.append({ scheme: scheme, schemeGroup: group })
            }
        }

        synchronizeCurrentScheme()
        _synchronizing = false
    }

    Component.onCompleted: populateSchemeModel()

    Connections {
        target: appState
        // 5WHY: 'function onTargetChanged()' uses the Qt 6.8 function-based
        // handler syntax, which collides with Connections' OWN target
        // property's targetChanged signal.  Qt 6.6 (Ubuntu 24.04 apt) rejects
        // it as 'Duplicate method name: invalid override of property change
        // signal' and the whole component chain fails to load.  The classic
        // binding form routes to the CONNECTED object's signal and works on
        // every Qt 6.x.
        onTargetChanged: root.synchronizeCurrentScheme()
    }

    onCurrentTextChanged: {
        if (!_synchronizing && currentText && appState.targetScheme !== currentText)
            appState.targetScheme = currentText
    }

    popup: Popup {
        y: root.height
        width: 222
        height: Math.min(implicitHeight, 320)
        padding: 6
        background: Rectangle {
            color: ThemeEngine.colors.card
            border { width: 1; color: ThemeEngine.colors.borderCard }
            radius: 10
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }

    delegate: ItemDelegate {
        width: ListView.view ? ListView.view.width : 222
        hoverEnabled: true
        padding: 0
        leftPadding: 0
        rightPadding: 0
        height: isFirst ? 64 : 36

        readonly property bool isFirst: {
            var previous = model.index > 0 ? root.schemeModel.get(model.index - 1) : null
            return !previous || previous.schemeGroup !== root.schemeModel.get(model.index).schemeGroup
        }
        readonly property string schemeGroupIcon: root.groupIcon(schemeGroup)
        readonly property string schemeGroupLabel: root.groupLabel(schemeGroup)

        highlighted: scheme === root.currentText
        background: Rectangle {
            color: highlighted
                ? Qt.alpha(ThemeEngine.colors.primary, 0.12)
                : (hovered ? Qt.alpha(ThemeEngine.colors.primary, 0.05) : "transparent")
            radius: 6
        }
        contentItem: ColumnLayout {
            spacing: 0
            Item {
                Layout.fillWidth: true
                implicitHeight: isFirst ? 26 : 0
                visible: isFirst
                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        leftMargin: 10
                        rightMargin: 10
                        topMargin: 4
                    }
                    height: 1
                    color: Qt.alpha(ThemeEngine.colors.borderCard, 0.6)
                }
                Row {
                    anchors {
                        left: parent.left
                        leftMargin: 16
                        bottom: parent.bottom
                        bottomMargin: 3
                    }
                    spacing: 10
                    AppIcon {
                        name: schemeGroupIcon
                        size: 12
                        color: ThemeEngine.colors.primary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        text: schemeGroupLabel.toUpperCase()
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.65)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: scheme + "://"
                font.family: ThemeEngine.monoFont
                font.pixelSize: 13
                font.weight: highlighted ? Font.DemiBold : Font.Normal
                color: highlighted ? ThemeEngine.colors.primary : ThemeEngine.colors.textPrimary
                verticalAlignment: Text.AlignVCenter
                leftPadding: 28
            }
        }
    }
}
