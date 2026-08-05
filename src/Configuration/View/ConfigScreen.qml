import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"

// ── Flutter ConfigScreen 1:1 — TabBar + SwitchListTile ───────────────
Item {
    id: page
    objectName: "config"
    property int currentGroup: 0
    // 5WHY: Direct binding to appState.stateVersion (Q_PROPERTY int stateVersion
    // NOTIFY stateVersionChanged) — QML tracks this natively. All JS block
    // expressions that reference configPollVersion re-evaluate automatically
    // when any appState mutation calls bumpVersion().
    property int configPollVersion: appState.stateVersion

    // AppBar
    AppBar {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        iconName: "config"
        title: T.tr("config")
        Item { Layout.fillWidth: true }
    }
    // TabBar — Flutter: G1..G5 tabs
    Rectangle {
        id: tabBar
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; topMargin: -1 }
        // 5WHY: 38px was below the M3 48dp / Apple 44pt minimum touch target.
        // 44px keeps the bar compact while making each G1-G5 tab comfortably
        // tappable and giving the active-tab underline more breathing room.
        implicitHeight: 44; color: ThemeEngine.colors.navBar
        border { width: 1; color: ThemeEngine.colors.borderCard }
        RowLayout {
            anchors.fill: parent; spacing: 0
                Repeater {
                        model: appState.groupLabels
                        delegate: ItemDelegate {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            background: Rectangle {
                                color: "transparent"
                                Rectangle {
                                    anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                                    height: 2; color: index === currentGroup ? ThemeEngine.colors.cyan : "transparent"
                                }
                            }
                            contentItem: RowLayout {
                                spacing: 3
                                // 5WHY: Clicking the tab BOTH toggled group activation AND
                                // switched tabs — conflating navigation with configuration.
                                // Now: green dot toggles activation; tab label navigates.
                                Rectangle {
                                    Layout.preferredWidth: 14; Layout.preferredHeight: 14; radius: 7
                                    color: { let _ = configPollVersion; return appState.isGroupActive(index) ? ThemeEngine.colors.passGreen : ThemeEngine.colors.textMuted }
                                    border { width: 1; color: { let _ = configPollVersion; return appState.isGroupActive(index) ? ThemeEngine.colors.passGreen : ThemeEngine.colors.textMuted } }
                                    MouseArea {
                                        anchors.fill: parent
                                        // 5WHY: Visually small dot (14dp) needs a large touch
                                        // target (48dp Material Design 3 minimum).  Expand
                                        // the hit area without changing the visual size.
                                        anchors.margins: -17  // 14 + 17*2 = 48dp touch target
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            let _ = configPollVersion
                                            appState.setGroupActive(index, !appState.isGroupActive(index))
                                        }
                                    }
                                }
                                Label {
                                    text: T.groupPrefix(index)
                                    font.family: ThemeEngine.monoFont; font.pixelSize: 12
                                    font.weight: index === currentGroup ? Font.DemiBold : Font.Normal
                                    color: { let _ = configPollVersion; return appState.isGroupActive(index) ? (index === currentGroup ? ThemeEngine.colors.cyan : ThemeEngine.colors.textPrimary) : ThemeEngine.colors.textMuted }
                                }
                            }
                            onClicked: {
                                currentGroup = index
                            }
                        }
                }
        }
    }

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; top: tabBar.bottom; bottom: parent.bottom }
        spacing: 0

        // ── Action Bar — Flutter: Container(padding h16 v12, bgCard alpha 0.5) ─
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 60
            color: Qt.alpha(ThemeEngine.colors.card, 0.5)
            border { width: 1; color: ThemeEngine.colors.borderCard }
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                ColumnLayout { spacing: 2
                    Label {
                        // 5WHY: configPollVersion forces re-evaluation after
                        // language switch so T.groupName() picks up the new
                        // language. Without this, QML may not track the
                        // dependency through the JS function call on all
                        // Qt versions.
                        text: { let _ = configPollVersion; return T.groupName(currentGroup) }
                        font.family: ThemeEngine.monoFont; font.pixelSize: 14; font.weight: Font.DemiBold; color: ThemeEngine.colors.textPrimary
                    }
                    Label {
                        text: { let _ = configPollVersion; return getDiagCountForGroup(currentGroup) + T.tr("diagsSuffix") }
                        font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textSecondary
                    }
                }
                Item { Layout.fillWidth: true }
                // Select All — icon-only (badge-check = multi-select, Material Design 3)
                // 40×40dp dense icon button, 20dp icon, tooltip via Accessible.name
                Rectangle {
                    id: selectAllBtn
                    implicitWidth: 48; implicitHeight: 48; radius: 8; color: "transparent"
                    border { width: 1; color: ThemeEngine.colors.borderCard }
                    enabled: { let _ = configPollVersion; return !appState.isGroupAllEnabled(currentGroup) }
                    opacity: enabled ? 1.0 : 0.4
                    AppIcon { anchors.centerIn: parent; name: "badge-check"; size: 20; color: enabled ? ThemeEngine.colors.passGreen : ThemeEngine.colors.textMuted }
                    MouseArea {
                        anchors.fill: parent
                        enabled: parent.enabled
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: appState.setGroupEnabled(currentGroup, true)
                    }
                    activeFocusOnTab: true
                    Keys.onPressed: function(event) {
                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Space) && selectAllBtn.enabled)
                            appState.setGroupEnabled(currentGroup, true)
                    }
                    Accessible.name: T.tr("selectAll")
                }
                Item { width: 6 }
                // Deselect All — icon-only (badge-close = multi-clear)
                Rectangle {
                    id: deselectAllBtn
                    implicitWidth: 48; implicitHeight: 48; radius: 8; color: "transparent"
                    border { width: 1; color: ThemeEngine.colors.borderCard }
                    enabled: { let _ = configPollVersion; return appState.isGroupAnyEnabled(currentGroup) }
                    opacity: enabled ? 1.0 : 0.4
                    AppIcon { anchors.centerIn: parent; name: "badge-close"; size: 20; color: enabled ? ThemeEngine.colors.failRed : ThemeEngine.colors.textMuted }
                    MouseArea {
                        anchors.fill: parent
                        enabled: parent.enabled
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: appState.setGroupEnabled(currentGroup, false)
                    }
                    activeFocusOnTab: true
                    Keys.onPressed: function(event) {
                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Space) && deselectAllBtn.enabled)
                            appState.setGroupEnabled(currentGroup, false)
                    }
                    Accessible.name: T.tr("deselectAll")
                }
            }
        }

        // ── Test list — Flutter: ListView.separated with SwitchListTile ─
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            ScrollBar.vertical: ScrollBar { }
            model: appState.allDiagIdsForGroup(currentGroup)
            delegate: ItemDelegate {
                id: tile
                width: ListView.view.width
                implicitHeight: tileCol.implicitHeight + 16
                background: Rectangle { color: "transparent"
                    Rectangle {
                        anchors { bottom: parent.bottom; left: parent.left; right: parent.right; leftMargin: 16 }
                        height: 1; color: ThemeEngine.colors.borderCard
                    }
                }

                RowLayout {
                    id: tileCol
                    anchors { fill: parent; leftMargin: 16; rightMargin: 16; topMargin: 8; bottomMargin: 8 }
                    spacing: 12

                    // Leading icon
                    // 5WHY: isDiagEnabled binding was stale — icon didn't
                    // update when Switch was toggled. configPollVersion forces
                    // re-evaluation with the same pattern as the Switch binding.
                    AppIcon {
                        name: { let _ = configPollVersion; return appState.isDiagEnabled(modelData) ? "badge-check" : "badge-circle" }
                        size: 14
                        color: { let _ = configPollVersion; return appState.isDiagEnabled(modelData) ? ThemeEngine.colors.passGreen : ThemeEngine.colors.textMuted }
                    }

                    // Title + subtitle
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        Label {
                            Layout.fillWidth: true
                            // 5WHY: configPollVersion forces re-evaluation
                            // after language switch. T.diagName() returns
                            // "" for English (which falls back to C++ name),
                            // or the translated name for other languages.
                            text: { let _ = configPollVersion; return getDisplayName(modelData) }
                            font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.Medium; color: ThemeEngine.colors.textPrimary
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: { let _ = configPollVersion; return getDiagDescription(modelData) }
                            font.family: ThemeEngine.monoFont; font.pixelSize: 11
                            color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.6)
                            elide: Text.ElideRight; maximumLineCount: 2
                            visible: text !== ""
                        }
                    }

                    // Switch — Flutter: activeColor accentBlue, inactive #5A5A7A
                    Switch {
                        checked: {
                            let _force = configPollVersion  // re-evaluate when poll timer fires
                            return appState.isDiagEnabled(modelData)
                        }
                        onToggled: appState.setDiagEnabled(modelData, checked)
                        // 5WHY: Without Accessible.name, screen readers
                        // announce just "Switch, on/off" — users don't
                        // know which diagnostic is being toggled.
                        Accessible.name: getDisplayName(modelData) + T.tr("accDiagnosticSuffix")
                    }
                }
            }
        }
    }

    // ── Display names + descriptions — routed through T.diagName/diagDesc ──
    // 5WHY: _enNames duplicated AppState::staticDiagDisplayName() (C++).
    // Removed the parallel array; getDisplayName() now calls the Q_INVOKABLE
    // appState.diagDisplayName(diagId) directly — single source of truth.
    // 5WHY: _enDescs was a 46-entry English duplicate of T.diagDesc()'s EN
    // column — a DRY violation that could drift.  T.diagDesc() now serves
    // English too (t() returns the EN argument when lang<=0), so this array
    // was removed; getDiagDescription() has a single canonical source.
    function getDisplayName(diagId) {
        // Use translated name when available (non-EN), fallback to C++ static array
        var tr = T.diagName(diagId)
        if (tr !== "") return tr
        // 5WHY: _enNames was a stale duplicate of staticDiagDisplayName().
        // Call the Q_INVOKABLE directly for a single source of truth.
        return appState.diagDisplayName(diagId)
    }
    function getDiagDescription(diagId) {
        return T.diagDesc(diagId)
    }
    function getDiagCountForGroup(groupIdx) {
        return appState.allDiagIdsForGroup(groupIdx).length
    }
}
