import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
import "../widgets"
import "../dialogs"

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
        implicitHeight: 48; color: ThemeEngine.colors.navBar
        border { width: 1; color: ThemeEngine.colors.borderCard }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            AppIcon { name: "gear"; size: 20; color: ThemeEngine.colors.primary }
            Item { width: 10 }
            Label { text: Tr.settings; font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.DemiBold; color: ThemeEngine.colors.textPrimary }
        }
    }

    Flickable {
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        clip: true
        contentHeight: setCol.implicitHeight

        ColumnLayout {
            id: setCol; width: parent.width - 48; x: 24; spacing: 0

            Item { Layout.preferredHeight: 24 }

            // ── Appearance Section ────────────────────────────────────
            SectionHeader { iconName: "brightness"; title: Tr.appearanceSection }
            Item { Layout.preferredHeight: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: themeCol.implicitHeight + 32; radius: 12
                color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout {
                    id: themeCol
                    anchors { fill: parent; margins: 16 } spacing: 0
                    Label {
                        text: Tr.themeLabel
                        font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: ThemeEngine.colors.textPrimary
                        Layout.bottomMargin: 12
                    }
                    RowLayout {
                        spacing: 6
                        Repeater {
                            model: [
                                { label: Tr.themeLight,  mode: ThemeEngine.litMode, icon: "brightness" },
                                { label: Tr.themeDark,   mode: ThemeEngine.drkMode, icon: "moon" }
                            ]
                            delegate: Rectangle {
                                // 5WHY: Cache ThemeEngine.mode comparison — evaluated 6 times
                                // in color/border/icon/label properties below.
                                readonly property bool isActive: ThemeEngine.mode === modelData.mode
                                // Adaptive: fill available RowLayout space evenly
                                Layout.fillWidth: true
                                Layout.minimumWidth: 80
                                // 5WHY: Theme buttons were 36pt — below 44pt Apple HIG minimum.
                                // Increased for accessible touch interaction.
                                // 48pt mobile (MD3 + HIG compliant), 40pt desktop.
                                implicitHeight: ThemeEngine.isMobile ? 48 : 40; radius: ThemeEngine.radius.md
                                color: isActive ? ThemeEngine.colors.primaryContainer : "transparent"
                                border {
                                    width: 1
                                    color: isActive ? ThemeEngine.colors.primary : ThemeEngine.colors.borderCard
                                }
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    AppIcon {
                                        Layout.alignment: Qt.AlignHCenter
                                        name: modelData.icon; size: 14
                                        color: isActive ? ThemeEngine.colors.primary : ThemeEngine.colors.textSecondary
                                    }
                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: modelData.label
                                        font.family: ThemeEngine.monoFont; font.pixelSize: 11
                                        font.weight: isActive ? Font.DemiBold : Font.Normal
                                        color: isActive ? ThemeEngine.colors.primary : ThemeEngine.colors.textSecondary
                                    }
                                }
                                // 5WHY: MouseArea-only controls lack keyboard accessibility
                                // (WCAG 2.1 SC 2.1.1). Add Keys.onPressed for Enter/Space.
                                // 5WHY: Missing Accessible properties break screen-reader
                                // (VoiceOver/TalkBack) identification of theme toggle buttons.
                                MouseArea {
                                    id: themeBtnArea
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        appState.themeMode = modelData.mode
                                        ThemeEngine.mode = modelData.mode
                                    }
                                }
                                activeFocusOnTab: true
                                Keys.onPressed: function(event) {
                                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                                        appState.themeMode = modelData.mode
                                        ThemeEngine.mode = modelData.mode
                                    }
                                }
                                Accessible.name: modelData.label
                                Accessible.role: Accessible.Button
                                Accessible.description: isActive ? qsTr("Active theme") : qsTr("Switch to this theme")
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 24 }

            // ── Language Section ───────────────────────────────────────
            SectionHeader { iconName: "translate"; title: Tr.languageSection }
            Item { Layout.preferredHeight: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: langCol.implicitHeight + 32; radius: 12
                color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
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
                        font.family: ThemeEngine.monoFont; font.pixelSize: 13
                        background: Rectangle {
                            radius: 6; color: ThemeEngine.bgInput; border { width: 1; color: ThemeEngine.colors.borderCard }
                        }
                        contentItem: Label {
                            text: langCombo.displayText
                            font: langCombo.font; color: ThemeEngine.textPrimary
                            verticalAlignment: Text.AlignVCenter; leftPadding: 12
                        }
                        indicator: Rectangle {
                            width: 24; height: 24; radius: 4; color: "transparent"
                            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                            Label {
                                anchors.centerIn: parent
                                text: "▾"; font.pixelSize: 12; color: ThemeEngine.textSecondary
                            }
                        }
                        delegate: ItemDelegate {
                            width: langCombo.width
                            contentItem: Label {
                                text: modelData; font.family: ThemeEngine.monoFont; font.pixelSize: 13
                                color: highlighted ? ThemeEngine.cyan : ThemeEngine.textPrimary
                                verticalAlignment: Text.AlignVCenter; leftPadding: 12
                            }
                            background: Rectangle { color: highlighted ? Qt.alpha(ThemeEngine.cyan, 0.1) : "transparent" }
                        }
                        popup: Popup {
                            y: langCombo.height + 4
                            width: langCombo.width
                            height: Math.min(implicitHeight, 280)
                            padding: 4
                            background: Rectangle { radius: 8; color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard } }
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
                visible: ThemeEngine.isMobile
                Layout.fillWidth: true; spacing: 0
                SectionHeader { iconName: "check"; title: Tr.subscribeTitle }
                Item { Layout.preferredHeight: 12 }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: restoreBtnCol.implicitHeight + 32; radius: 12
                    color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
                    ColumnLayout {
                        id: restoreBtnCol
                        anchors { fill: parent; margins: 16 } spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: appState.isPremium ? Tr.premiumUnlocked : Tr.premiumRequiredMsg
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: 12; color: ThemeEngine.textSecondary; wrapMode: Text.WordWrap; lineHeight: 1.4
                        }
                        Item { Layout.preferredHeight: 12 }
                        // Restore button — hidden when already premium
                        Rectangle {
                            visible: !appState.isPremium
                            Layout.fillWidth: true; implicitHeight: 42; radius: 8
                            color: appState.purchaseInProgress ? Qt.alpha(ThemeEngine.warnYellow, 0.08)
                                                               : Qt.alpha(ThemeEngine.warnYellow, 0.12)
                            border { width: 1; color: appState.purchaseInProgress ? Qt.alpha(ThemeEngine.warnYellow, 0.3)
                                                                                   : Qt.alpha(ThemeEngine.warnYellow, 0.4) }
                            Label {
                                anchors.centerIn: parent
                                text: appState.purchaseInProgress ? "..." : Tr.restoreBtn
                                font.family: ThemeEngine.monoFont
                                font.pixelSize: 13; font.weight: Font.DemiBold; color: ThemeEngine.warnYellow
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
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: 11; color: ThemeEngine.warnYellow
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
                color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout {
                    id: aboutCol
                    anchors { fill: parent; margins: 16 } spacing: 0
                    // App icon + name
                    RowLayout {
                        Rectangle { implicitWidth: 48; implicitHeight: 48; radius: 12; color: Qt.alpha(ThemeEngine.accentBlue, 0.15)
                            AppIcon { anchors.centerIn: parent; name: "wifi"; size: 28; color: ThemeEngine.accentBlue } }
                        Item { width: 14 }
                        ColumnLayout { spacing: 2; Layout.fillWidth: true
                            Label { text: "NetDiagnostics" + (appState.isPremium ? "  " + Tr.premiumBadge : ""); font.family: ThemeEngine.monoFont; font.pixelSize: 18; font.weight: Font.Bold; color: ThemeEngine.textPrimary }
                            Label {
                                Layout.fillWidth: true
                                text: "Version " + appState.appVersion
                                      + (appState.appEdition.length > 0 ? " (" + appState.appEdition + ")" : "")
                                      + (appState.buildNumber.length > 0 ? " Build " + appState.buildNumber : "")
                                font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textSecondary
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                    Item { Layout.preferredHeight: 16 }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: ThemeEngine.colors.borderCard }
                    Item { Layout.preferredHeight: 12 }
                    Label { Layout.fillWidth: true; text: Tr.aboutDesc
                        font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: ThemeEngine.textSecondary; wrapMode: Text.WordWrap; lineHeight: 1.5 }
                    Item { Layout.preferredHeight: 16 }
                    AboutRow { aboutIcon: "💻"; aboutText: Tr.crossPlat }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "⚡"; aboutText: Tr.realtimeDiag }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "📊"; aboutText: Tr.detailedReport }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "🌙"; aboutText: Tr.darkTheme }
                    Item { Layout.preferredHeight: 8 }

                }
            }
            Item { Layout.preferredHeight: 24 }
        }
    }

    // ── Subcomponents ──────────────────────────────────────────────────
    component SectionHeader: RowLayout {
        property string iconName: ""; property string title: ""
        Rectangle { implicitWidth: 30; implicitHeight: 30; radius: 8; color: Qt.alpha(ThemeEngine.colors.primary, 0.1)
            AppIcon { anchors.centerIn: parent; name: iconName; size: 18; color: ThemeEngine.colors.textPrimary } }
        Item { width: 12 }
        Label { text: title; font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary }
    }

    // 5WHY: LangBtn and SmtpField were defined but never instantiated — dead code.
    // LangBtn: reserved for future language selector grid (currently using ComboBox).
    // SmtpField: reserved for future SMTP configuration section.
    // Removed to reduce QML component registry overhead and maintenance burden.

    // component LangBtn: Rectangle { ... } — removed (unused)
    // component SmtpField: ColumnLayout { ... } — removed (unused)

    component AboutRow: RowLayout {
        property string aboutIcon: ""; property string aboutText: ""
        Label { text: aboutIcon; font.pixelSize: 16; color: ThemeEngine.colors.textSecondary; Layout.alignment: Qt.AlignTop }
        Item { width: 10 }
        Label { Layout.fillWidth: true; text: aboutText; wrapMode: Text.WordWrap; font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: Qt.alpha(ThemeEngine.textSecondary, 0.8) }
    }
}
