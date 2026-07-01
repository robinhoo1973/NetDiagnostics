import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
import "../widgets"

// ── Flutter SettingsScreen 1:1 — with AppBar ───────────────────────────
Item {
    id: page
    objectName: "settings"

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
            SectionHeader { iconName: "globe"; title: Tr.languageSection }
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
                        model: ["English","Français","Deutsch","Русский","Italiano","简体中文","繁體中文"]
                        currentIndex: appState ? appState.languageIndex : 0
                        onActivated: { if (appState) appState.setLanguage(currentIndex) }
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
            Item { Layout.preferredHeight: 32 }

            // ── SMTP Config Section ────────────────────────────────────
            SectionHeader { iconName: "mail"; title: Tr.emailConfigSection }
            Item { Layout.preferredHeight: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: smtpCol.implicitHeight + 32; radius: 12
                color: Theme.bgCard; border { width: 1; color: "#2A2A4A" }
                ColumnLayout {
                    id: smtpCol
                    anchors { fill: parent; margins: 16 } spacing: 12
                    SmtpField { label: Tr.smtpServerLabel; placeholder: "smtp.example.com" }
                    SmtpField { label: Tr.portLabel; placeholder: "587" }
                    SmtpField { label: Tr.usernameLabel; placeholder: "user@example.com" }
                    SmtpField { label: Tr.passwordLabel; placeholder: "••••••••" }
                    SmtpField { label: Tr.fromAddrLabel; placeholder: "noreply@example.com" }
                    // Info notice
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: noticeText.implicitHeight + 24; radius: 8
                        color: Qt.alpha(Theme.warnYellow, 0.08); border { width: 1; color: Qt.alpha(Theme.warnYellow, 0.2) }
                        RowLayout {
                            anchors { fill: parent; margins: 12 }
                            AppIcon { name: "warning"; size: 16; color: Theme.warnYellow }
                            Item { width: 10 }
                            Label {
                                id: noticeText; Layout.fillWidth: true
                                text: Tr.placeholderMsg
                                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 11; color: Qt.alpha(Theme.warnYellow, 0.8); wrapMode: Text.WordWrap; lineHeight: 1.4
                            }
                        }
                    }
                }
            }
            Item { Layout.preferredHeight: 32 }

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
                            Label { text: "NetDiagnostic"; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 18; font.weight: Font.Bold; color: Theme.textPrimary }
                            Label {
                                Layout.fillWidth: true
                                text: "Version " + appState.appVersion
                                      + (appState.buildNumber.length > 0 ? " (Build " + appState.buildNumber + ")" : "")
                                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 12; color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                            }
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
