import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
import "../widgets"

// ── Flutter SettingsScreen 1:1 — with AppBar ───────────────────────────
Item {
    id: page
    objectName: "settings"

    // Listen for restore-purchases result
    Connections {
        target: appState
        function onRestoreCompleted(restoredAny, isError) {
            if (isError) {
                restoreToast.text = Tr.restoreError
            } else if (restoredAny) {
                restoreToast.text = Tr.restoreOk
            } else {
                restoreToast.text = Tr.restoreFail
            }
            restoreToastTimer.restart()
        }
    }

    // AppBar (Flutter: Scaffold.appBar with "Settings" title)
    Rectangle {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        implicitHeight: 52; color: "#1A1A2E"
        border { width: 1; color: "#3A3A5A" }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            AppIcon { name: "settings"; size: 20; color: Theme.cyan }
            Item { width: 10 }
            Label { text: Tr.settings; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary }
        }
    }

    Flickable {
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        clip: true
        contentHeight: setCol.implicitHeight

        ColumnLayout {
            id: setCol; width: parent.width - 48; x: 24; spacing: 0

            Item { Layout.preferredHeight: 24 }

            // ── Language Section ───────────────────────────────────────
            SectionHeader { iconName: "translate"; title: Tr.languageSection }
            Item { Layout.preferredHeight: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: langCol.implicitHeight + 32; radius: 12
                color: Theme.bgCard; border { width: 1; color: "#2A2A4A" }
                ColumnLayout {
                    id: langCol
                    anchors { fill: parent; margins: 16 } spacing: 0
                    // Language dropdown
                    ComboBox {
                        id: langCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        // Displayed in UTF-8 (code-point) order; idx = internal language
                        // index (0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW,7=ES,8=PT).
                        readonly property var langItems: [
                            { name: "Deutsch",   idx: 2 },
                            { name: "English",   idx: 0 },
                            { name: "Español",   idx: 7 },
                            { name: "Français",  idx: 1 },
                            { name: "Italiano",  idx: 4 },
                            { name: "Português", idx: 8 },
                            { name: "Русский",   idx: 3 },
                            { name: "简体中文",   idx: 5 },
                            { name: "繁體中文",   idx: 6 }
                        ]
                        model: langItems.map(function(e) { return e.name })
                        currentIndex: {
                            if (!appState) return 0
                            for (var i = 0; i < langItems.length; i++)
                                if (langItems[i].idx === appState.languageIndex) return i
                            return 0
                        }
                        onActivated: function(index) { if (appState) appState.setLanguage(langItems[index].idx) }
                        font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 13
                        background: Rectangle {
                            radius: 6; color: Theme.bgInput; border { width: 1; color: "#3A3A5A" }
                        }
                        contentItem: Label {
                            text: langCombo.displayText
                            font: langCombo.font; color: Theme.textPrimary
                            verticalAlignment: Text.AlignVCenter; leftPadding: 12
                        }
                        indicator: Rectangle {
                            width: 24; height: 24; radius: 4; color: "transparent"
                            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                            Label {
                                anchors.centerIn: parent
                                text: "▾"; font.pixelSize: 12; color: Theme.textSecondary
                            }
                        }
                        delegate: ItemDelegate {
                            width: langCombo.width
                            contentItem: Label {
                                text: modelData; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 13
                                color: highlighted ? Theme.cyan : Theme.textPrimary
                                verticalAlignment: Text.AlignVCenter; leftPadding: 12
                            }
                            background: Rectangle { color: highlighted ? Qt.alpha(Theme.cyan, 0.1) : "transparent" }
                        }
                        popup: Popup {
                            y: langCombo.height + 4
                            width: langCombo.width
                            implicitHeight: contentItem.implicitHeight + 8
                            padding: 4
                            background: Rectangle { radius: 8; color: Theme.bgCard; border { width: 1; color: "#3A3A5A" } }
                            contentItem: ListView {
                                clip: true; implicitHeight: contentHeight
                                model: langCombo.popup.visible ? langCombo.delegateModel : null
                                currentIndex: langCombo.highlightedIndex
                            }
                        }
                    }
                }
            }
            Item { Layout.preferredHeight: 20 }

            // (Email/SMTP section removed — report sharing is handled from the
            //  Report screen's preview window via Share/Email.)

            // ── Premium Section (mobile only) ────────────
            ColumnLayout {
                id: restoreSection
                visible: Qt.platform.os === "ios" || Qt.platform.os === "android"
                Layout.fillWidth: true; spacing: 0
                SectionHeader { iconName: "badge-check"; title: Tr.subscribeTitle }
                Item { Layout.preferredHeight: 12 }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: restoreBtnCol.implicitHeight + 32; radius: 12
                    color: Theme.bgCard; border { width: 1; color: "#2A2A4A" }
                    ColumnLayout {
                        id: restoreBtnCol
                        anchors { fill: parent; margins: 16 } spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: appState.isPremium ? Tr.premiumUnlocked : Tr.premiumRequiredMsg
                            font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"
                            font.pixelSize: 12; color: Theme.textSecondary; wrapMode: Text.WordWrap; lineHeight: 1.4
                        }
                        Item { Layout.preferredHeight: 12 }
                        // Restore button — hidden when already premium
                        Rectangle {
                            visible: !appState.isPremium
                            Layout.fillWidth: true; implicitHeight: 40; radius: 8
                            color: appState.purchaseInProgress ? Qt.alpha(Theme.warnYellow, 0.08)
                                                               : Qt.alpha(Theme.warnYellow, 0.12)
                            border { width: 1; color: appState.purchaseInProgress ? Qt.alpha(Theme.warnYellow, 0.3)
                                                                                   : Qt.alpha(Theme.warnYellow, 0.4) }
                            Label {
                                anchors.centerIn: parent
                                text: appState.purchaseInProgress ? "..." : Tr.restoreBtn
                                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"
                                font.pixelSize: 13; font.weight: Font.DemiBold; color: Theme.warnYellow
                            }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                enabled: !appState.purchaseInProgress
                                onClicked: appState.restorePurchases()
                            }
                        }
                        // Restore result toast
                        Label {
                            id: restoreToast
                            Layout.fillWidth: true
                            visible: restoreToastTimer.running
                            font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"
                            font.pixelSize: 11; color: Theme.warnYellow
                            Layout.topMargin: restoreToast.visible ? 8 : 0
                        }
                        Timer { id: restoreToastTimer; interval: 3000 }
                    }
                }
                Item { Layout.preferredHeight: 32 }
            }

            // ── About Section ──────────────────────────────────────────
            SectionHeader { iconName: "info"; title: Tr.aboutSection }
            Item { Layout.preferredHeight: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: aboutCol.implicitHeight + 32; radius: 12
                color: Theme.bgCard; border { width: 1; color: "#2A2A4A" }
                ColumnLayout {
                    id: aboutCol
                    anchors { fill: parent; margins: 16 } spacing: 0
                    // App icon + name
                    RowLayout {
                        Rectangle { implicitWidth: 48; implicitHeight: 48; radius: 12; color: Qt.alpha(Theme.accentBlue, 0.15)
                            AppIcon { anchors.centerIn: parent; name: "wifi"; size: 28; color: Theme.accentBlue } }
                        Item { width: 14 }
                        ColumnLayout { spacing: 2; Layout.fillWidth: true
                            Label { text: "NetDiagnostics" + (appState.isPremium ? "  " + Tr.premiumBadge : ""); font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 18; font.weight: Font.Bold; color: Theme.textPrimary }
                            Label {
                                id: versionLabel
                                property int taps: 0
                                Layout.fillWidth: true
                                text: "Version " + appState.appVersion
                                      + (appState.buildNumber.length > 0 ? " (Build " + appState.buildNumber + ")" : "")
                                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 12; color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                                // Hidden debug toggle: tap the version 7× to toggle premium
                                // (useful for testing on desktop / simulator builds).
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        versionLabel.taps++
                                        if (versionLabel.taps >= 7) {
                                            versionLabel.taps = 0
                                            appState.setPremium(!appState.isPremium)
                                            premiumToast.text = appState.isPremium ? Tr.premiumUnlocked : Tr.premiumLocked
                                            premiumToastTimer.restart()
                                        }
                                    }
                                }
                            }
                            Label {
                                id: premiumToast
                                Layout.fillWidth: true
                                visible: premiumToastTimer.running
                                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 11; color: Theme.cyan
                            }
                            Timer { id: premiumToastTimer; interval: 2500 }
                        }
                    }
                    Item { Layout.preferredHeight: 16 }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#2A2A4A" }
                    Item { Layout.preferredHeight: 12 }
                    Label { Layout.fillWidth: true; text: Tr.aboutDesc
                        font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 13; color: Theme.textSecondary; wrapMode: Text.WordWrap; lineHeight: 1.5 }
                    Item { Layout.preferredHeight: 16 }
                    AboutRow { aboutIcon: "💻"; aboutText: Tr.crossPlat }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "⚡"; aboutText: Tr.realtimeDiag }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "📊"; aboutText: Tr.detailedReport }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "🌙"; aboutText: Tr.darkTheme }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "🖥"; aboutText: Tr.simulatorMode }
                }
            }
            Item { Layout.preferredHeight: 24 }
        }
    }

    // ── Subcomponents ──────────────────────────────────────────────────
    component SectionHeader: RowLayout {
        property string iconName: ""; property string title: ""
        Rectangle { implicitWidth: 30; implicitHeight: 30; radius: 8; color: Qt.alpha(Theme.cyan, 0.1)
            AppIcon { anchors.centerIn: parent; name: iconName; size: 18; color: Theme.cyan } }
        Item { width: 12 }
        Label { text: title; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 16; font.weight: Font.DemiBold; color: Theme.textPrimary }
    }

    component LangBtn: Rectangle {
        property string label: ""; property bool selected: false; property string code: ""
        implicitHeight: 52; radius: 8
        color: selected ? Qt.alpha(Theme.accentBlue, 0.15) : "transparent"
        border { width: selected ? 1.5 : 1; color: selected ? Theme.accentBlue : "#3A3A5A" }
        ColumnLayout {
            anchors.centerIn: parent; spacing: 2
            Label { anchors.horizontalCenter: parent.horizontalCenter; text: label; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 14; font.weight: selected ? Font.DemiBold : Font.Medium; color: selected ? Theme.accentBlue : Theme.textPrimary }
            Label { anchors.horizontalCenter: parent.horizontalCenter; text: code; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 10; color: selected ? Qt.alpha(Theme.accentBlue, 0.6) : Theme.textSecondary }
        }
    }

    component SmtpField: ColumnLayout {
        property string label: ""; property string placeholder: ""
        Label { text: label; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 11; font.weight: Font.Medium; color: Theme.textSecondary }
        Item { Layout.preferredHeight: 4 }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 36; radius: 6
            color: Qt.alpha(Theme.bgDark, 0.6); border { width: 1; color: "#3A3A5A" }
            Label { anchors { fill: parent; leftMargin: 12; rightMargin: 12 } text: placeholder; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 12; color: Qt.alpha(Theme.textSecondary, 0.6); verticalAlignment: Text.AlignVCenter }
        }
    }

    component AboutRow: RowLayout {
        property string aboutIcon: ""; property string aboutText: ""
        Label { text: aboutIcon; font.pixelSize: 16; color: Qt.alpha(Theme.cyan, 0.7); Layout.alignment: Qt.AlignTop }
        Item { width: 10 }
        Label { Layout.fillWidth: true; text: aboutText; wrapMode: Text.WordWrap; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 12; color: Qt.alpha(Theme.textSecondary, 0.8) }
    }
}
