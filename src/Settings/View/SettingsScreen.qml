import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
import "../widgets"

// ── Flutter SettingsScreen 1:1 — with AppBar ───────────────────────────
Item {
    id: page
    objectName: "settings"

    // Language toast — shared across the page (scoped to root so all children can access)
    property string languageToastText: ""

    // Listen for restore-purchases result
    Connections {
        target: appState
        function onRestoreCompleted(restoredAny, isError) {
            if (isError) {
                restoreToast.text = T.tr("restoreError")
            } else if (restoredAny) {
                restoreToast.text = T.tr("restoreOk")
            } else {
                restoreToast.text = T.tr("restoreFail")
            }
            restoreToastTimer.restart()
        }
        // 5WHY: "Ask to Buy" defers the purchase — show a notice instead of
        // leaving the user staring at a stuck "Processing…" state.
        function onPurchaseDeferred() {
            restoreToast.text = T.tr("purchaseDeferred")
            restoreToastTimer.restart()
        }
    }

    // AppBar
    AppBar {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        iconName: "gear"
        title: T.tr("settings")
        Item { Layout.fillWidth: true }
    }

    Flickable {
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        clip: true
        ScrollBar.vertical: ScrollBar { }
        contentHeight: setCol.implicitHeight

        ColumnLayout {
            id: setCol; width: parent.width - 48; x: 24; spacing: 0

            Item { Layout.preferredHeight: 24 }

            // ── Appearance Section ────────────────────────────────────
            SectionHeader { iconName: "brightness"; title: T.tr("appearanceSection") }
            Item { Layout.preferredHeight: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: themeCol.implicitHeight + 32; radius: 12
                color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout {
                    id: themeCol
                    anchors { fill: parent; margins: 16 } spacing: 0
                    Label {
                        text: T.tr("themeLabel")
                        font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: ThemeEngine.colors.textPrimary
                        Layout.bottomMargin: 12
                    }
                    RowLayout {
                        spacing: 6
                        Repeater {
                            model: [
                                { label: T.tr("themeLight"),  mode: ThemeEngine.litMode, icon: "brightness" },
                                { label: T.tr("themeDark"),   mode: ThemeEngine.drkMode, icon: "moon" }
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
                                // 5WHY: No hover feedback meant users got zero
                                // interactive affordance — the button looked
                                // like a static label.  Show a subtle tint on
                                // hover to indicate clickability.
                                color: isActive ? ThemeEngine.colors.primaryContainer
                                     : themeBtnArea.containsMouse ? Qt.alpha(ThemeEngine.colors.primary, 0.06)
                                     : "transparent"
                                Behavior on color { ColorAnimation { duration: 150 } }
                                border {
                                    width: 1
                                    color: isActive ? ThemeEngine.colors.primary
                                         : themeBtnArea.containsMouse ? Qt.alpha(ThemeEngine.colors.primary, 0.3)
                                         : ThemeEngine.colors.borderCard
                                }
                                Behavior on border.color { ColorAnimation { duration: 150 } }
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
                                        font.family: ThemeEngine.fontUi; font.pixelSize: 11
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
                                    hoverEnabled: true
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
                                Accessible.description: isActive ? T.tr("accActiveTheme") : T.tr("accSwitchTheme")
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 24 }

            // ── Language Section ───────────────────────────────────────
            SectionHeader { iconName: "translate"; title: T.tr("languageSection") }
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
                        // Alphabetical by display name; idx = internal language index
                        // 0=ZH_CN,1=ZH_TW,2=JA,3=KO,4=HI,5=VI,6=TR,7=EN,8=FR,9=DE,10=RU,11=IT,12=ES,13=PT,14=AR
                        readonly property var langItems: [
                            { name: "العربية",    idx: 14 },
                            { name: "Deutsch",    idx: 9 },
                            { name: "English",    idx: 7 },
                            { name: "Español",    idx: 12 },
                            { name: "Français",   idx: 8 },
                            { name: "हिन्दी",       idx: 4 },
                            { name: "Italiano",   idx: 11 },
                            { name: "日本語",      idx: 2 },
                            { name: "한국어",      idx: 3 },
                            { name: "Português",  idx: 13 },
                            { name: "Русский",    idx: 10 },
                            { name: "Tiếng Việt", idx: 5 },
                            { name: "Türkçe",     idx: 6 },
                            { name: "简体中文",    idx: 0 },
                            { name: "繁體中文",    idx: 1 }
                        ]
                        model: langItems.map(function(e) { return e.name })
                        currentIndex: {
                            if (!appState) return 0
                            for (var i = 0; i < langItems.length; i++)
                                if (langItems[i].idx === appState.languageIndex) return i
                            return 0
                        }
                        // 5WHY: Language switch was immediate with no feedback.
                        // If the user taps the wrong language, the entire UI
                        // changes to an unfamiliar language with no way to
                        // know what happened.  Show a toast with the selected
                        // language name for 3 seconds.
                        onActivated: function(index) {
                            if (appState) appState.setLanguage(langItems[index].idx)
                            page.languageToastText = langItems[index].name + "  ✓"
                            languageToastTimer.restart()
                        }
                        font.family: ThemeEngine.fontUi; font.pixelSize: 13
                        background: Rectangle {
                            radius: 6; color: ThemeEngine.colors.input; border { width: 1; color: ThemeEngine.colors.borderCard }
                        }
                        contentItem: Label {
                            text: langCombo.displayText
                            font: langCombo.font; color: ThemeEngine.colors.textPrimary
                            verticalAlignment: Text.AlignVCenter
                            // 5WHY: ComboBox text was hardcoded left-aligned with
                            // leftPadding — in RTL (Arabic) the selected language
                            // must hug the start edge with padding on the right.
                            horizontalAlignment: T.textAlignStart
                            leftPadding: T.isRtl ? 0 : 12
                            rightPadding: T.isRtl ? 12 : 0
                        }
                        indicator: Rectangle {
                            width: 24; height: 24; radius: 4; color: "transparent"
                            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                            // 5WHY: Replaced ▾ Unicode triangle with chevron-down SVG.
                            AppIcon {
                                anchors.centerIn: parent
                                name: "chevron-down"; size: 12; color: ThemeEngine.colors.textSecondary
                            }
                        }
                        delegate: ItemDelegate {
                            width: langCombo.width
                            contentItem: Label {
                                text: modelData; font.family: ThemeEngine.fontUi; font.pixelSize: 13
                                color: highlighted ? ThemeEngine.colors.cyan : ThemeEngine.colors.textPrimary
                                verticalAlignment: Text.AlignVCenter
                                // 5WHY: Same RTL fix as the combo contentItem —
                                // dropdown items align to the start edge.
                                horizontalAlignment: T.textAlignStart
                                leftPadding: T.isRtl ? 0 : 12
                                rightPadding: T.isRtl ? 12 : 0
                            }
                            background: Rectangle { color: highlighted ? Qt.alpha(ThemeEngine.colors.cyan, 0.1) : "transparent" }
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
                SectionHeader { iconName: "check"; title: T.tr("subscribeTitle") }
                Item { Layout.preferredHeight: 12 }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: restoreBtnCol.implicitHeight + 32; radius: 12
                    color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
                    ColumnLayout {
                        id: restoreBtnCol
                        anchors { fill: parent; margins: 16 } spacing: 0
                        AppLabel {
                            Layout.fillWidth: true
                            text: appState.isPremium ? T.tr("premiumUnlocked") : T.tr("premiumRequiredMsg")
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: 12; color: ThemeEngine.colors.textSecondary; wrapMode: Text.WordWrap; lineHeight: 1.4
                        }
                        Item { Layout.preferredHeight: 12 }
                        // Restore button — hidden when already premium
                        Rectangle {
                            visible: !appState.isPremium
                            Layout.fillWidth: true; implicitHeight: 42; radius: 8
                            color: appState.purchaseInProgress ? Qt.alpha(ThemeEngine.colors.warnYellow, 0.08)
                                                               : Qt.alpha(ThemeEngine.colors.warnYellow, 0.12)
                            border { width: 1; color: appState.purchaseInProgress ? Qt.alpha(ThemeEngine.colors.warnYellow, 0.3)
                                                                                   : Qt.alpha(ThemeEngine.colors.warnYellow, 0.4) }
                            Label {
                                anchors.centerIn: parent
                                text: appState.purchaseInProgress ? T.tr("purchaseInProgress") : T.tr("restoreBtn")
                                font.family: ThemeEngine.monoFont
                                font.pixelSize: 13; font.weight: Font.DemiBold; color: ThemeEngine.colors.warnYellow
                            }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                enabled: !appState.purchaseInProgress
                                onClicked: appState.restorePurchases()
                            }
                        }
                        // Restore result toast
                        AppLabel {
                            id: restoreToast
                            Layout.fillWidth: true
                            visible: restoreToastTimer.running
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: 11; color: ThemeEngine.colors.warnYellow
                            Layout.topMargin: restoreToast.visible ? 8 : 0
                        }
                        Timer { id: restoreToastTimer; interval: ThemeEngine.toastDurationMs }
                    }
                }
                Item { Layout.preferredHeight: 32 }
            }

            // 5WHY: Language switch was previously silent — users got no
            // confirmation after selecting a language.  This toast shows
            // the selected language name for 3 seconds after switching.
            Timer { id: languageToastTimer; interval: ThemeEngine.toastDurationMs }
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                Layout.topMargin: languageToastTimer.running ? 4 : 0
                visible: languageToastTimer.running
                text: page.languageToastText
                font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.passGreen
            }

            // ── About Section ──────────────────────────────────────────
            SectionHeader { iconName: "info"; title: T.tr("aboutSection") }
            Item { Layout.preferredHeight: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: aboutCol.implicitHeight + 32; radius: 12
                color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout {
                    id: aboutCol
                    anchors { fill: parent; margins: 16 } spacing: 0
                    // App icon + name
                    RowLayout {
                        Rectangle {
                            implicitWidth: 48; implicitHeight: 48; radius: 12
                            color: Qt.alpha(ThemeEngine.colors.secondary, 0.15)
                            AppIcon {
                                anchors.centerIn: parent
                                name: "wifi"
                                size: 28
                                color: ThemeEngine.colors.secondary
                            }
                        }
                        Item { width: 14 }
                        ColumnLayout { spacing: 2; Layout.fillWidth: true
                            Label { text: T.tr("appName") + (appState.isPremium ? "  " + T.tr("premiumBadge") : ""); font.family: ThemeEngine.monoFont; font.pixelSize: 18; font.weight: Font.Bold; color: ThemeEngine.colors.textPrimary }
                            AppLabel {
                                Layout.fillWidth: true
                                text: T.tr("versionLabel") + " " + appState.appVersion
                                      + (appState.appEdition.length > 0 ? " (" + appState.appEdition + ")" : "")
                                      + (appState.buildNumber.length > 0 ? " " + T.tr("buildLabel") + " " + appState.buildNumber : "")
                                      + (appState.gitHash.length > 0 ? " (" + appState.gitHash + ")" : "")
                                font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.colors.textSecondary
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                    Item { Layout.preferredHeight: 16 }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: ThemeEngine.colors.borderCard }
                    Item { Layout.preferredHeight: 12 }
                    AppLabel { Layout.fillWidth: true; text: T.tr("aboutDesc")
                        font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: ThemeEngine.colors.textSecondary; wrapMode: Text.WordWrap; lineHeight: 1.5 }
                    Item { Layout.preferredHeight: 16 }
                    AboutRow { aboutIcon: "monitor"; aboutText: T.tr("crossPlat") }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "zap"; aboutText: T.tr("realtimeDiag") }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "chart"; aboutText: T.tr("detailedReport") }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "moon"; aboutText: T.tr("darkTheme") }
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
        Item { width: ThemeEngine.spacing.md }
        Label { text: title; font.family: ThemeEngine.fontUi; font.pixelSize: 16; font.weight: Font.DemiBold; color: ThemeEngine.colors.textPrimary }
    }

    // 5WHY: LangBtn and SmtpField were defined but never instantiated — dead code.
    // LangBtn: reserved for future language selector grid (currently using ComboBox).
    // SmtpField: reserved for future SMTP configuration section.
    // Removed to reduce QML component registry overhead and maintenance burden.

    // component LangBtn: Rectangle { ... } — removed (unused)
    // component SmtpField: ColumnLayout { ... } — removed (unused)

    component AboutRow: RowLayout {
        property string aboutIcon: ""; property string aboutText: ""
        // 5WHY: Replaced emoji Label with AppIcon for consistent SVG iconography.
        AppIcon { name: aboutIcon; size: 16; color: ThemeEngine.colors.textSecondary; Layout.alignment: Qt.AlignTop }
        Item { width: 10 }
        AppLabel { Layout.fillWidth: true; text: aboutText; wrapMode: Text.WordWrap; font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.8) }
    }
}
