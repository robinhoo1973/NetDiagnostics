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
    Rectangle {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        implicitHeight: 84; color: ThemeEngine.colors.navBar
        border { width: 1; color: ThemeEngine.colors.borderCard }
        ColumnLayout {
            anchors.fill: parent; spacing: 0
            RowLayout {
                Layout.fillWidth: true; Layout.preferredHeight: 44
                anchors { leftMargin: 16; rightMargin: 16 }
                AppIcon { name: "config"; size: 20; color: ThemeEngine.colors.textPrimary }
                Item { width: 10 }
                Label { text: Tr.diagConfig; font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary }
            }
            // TabBar — Flutter: G1..G5 tabs
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 38
                color: "transparent"
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
                                    height: 2; color: index === currentGroup ? ThemeEngine.cyan : "transparent"
                                }
                            }
                            contentItem: RowLayout {
                                spacing: 3
                                // 5WHY: Clicking the tab BOTH toggled group activation AND
                                // switched tabs — conflating navigation with configuration.
                                // Now: green dot toggles activation; tab label navigates.
                                Rectangle {
                                    Layout.preferredWidth: 14; Layout.preferredHeight: 14; radius: 7
                                    color: { let _ = configPollVersion; return appState.isGroupActive(index) ? ThemeEngine.passGreen : ThemeEngine.textMuted }
                                    border { width: 1; color: { let _ = configPollVersion; return appState.isGroupActive(index) ? ThemeEngine.passGreen : ThemeEngine.textMuted } }
                                    MouseArea {
                                        anchors.fill: parent
                                        anchors.margins: -4  // expand hit area for touch
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            let _ = configPollVersion
                                            appState.setGroupActive(index, !appState.isGroupActive(index))
                                        }
                                    }
                                }
                                Label {
                                    text: "G" + (index + 1)
                                    font.family: ThemeEngine.monoFont; font.pixelSize: 12
                                    font.weight: index === currentGroup ? Font.DemiBold : Font.Normal
                                    color: { let _ = configPollVersion; return appState.isGroupActive(index) ? (index === currentGroup ? ThemeEngine.cyan : ThemeEngine.textPrimary) : ThemeEngine.textMuted }
                                }
                            }
                            onClicked: {
                                currentGroup = index
                            }
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        spacing: 0

        // ── Action Bar — Flutter: Container(padding h16 v12, bgCard alpha 0.5) ─
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 60
            color: Qt.alpha(ThemeEngine.bgCard, 0.5)
            border { width: 1; color: ThemeEngine.colors.borderCard }
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                ColumnLayout { spacing: 2
                    Label {
                        text: Tr.groupName(currentGroup)
                        font.family: ThemeEngine.monoFont; font.pixelSize: 14; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary
                    }
                    Label {
                        text: getDiagCountForGroup(currentGroup) + Tr.diagsSuffix
                        font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textSecondary
                    }
                }
                Item { Layout.fillWidth: true }
                // Select All — icon-only (badge-check = multi-select, Material Design 3)
                // 40×40dp dense icon button, 20dp icon, tooltip via Accessible.name
                Rectangle {
                    id: selectAllBtn
                    implicitWidth: 40; implicitHeight: 40; radius: 8; color: "transparent"
                    border { width: 1; color: ThemeEngine.colors.borderCard }
                    enabled: { let _ = configPollVersion; return !appState.isGroupAllEnabled(currentGroup) }
                    opacity: enabled ? 1.0 : 0.4
                    AppIcon { anchors.centerIn: parent; name: "badge-check"; size: 20; color: enabled ? ThemeEngine.passGreen : ThemeEngine.textMuted }
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
                    Accessible.name: Tr.selectAll
                }
                Item { width: 6 }
                // Deselect All — icon-only (badge-close = multi-clear)
                Rectangle {
                    id: deselectAllBtn
                    implicitWidth: 40; implicitHeight: 40; radius: 8; color: "transparent"
                    border { width: 1; color: ThemeEngine.colors.borderCard }
                    enabled: { let _ = configPollVersion; return appState.isGroupAnyEnabled(currentGroup) }
                    opacity: enabled ? 1.0 : 0.4
                    AppIcon { anchors.centerIn: parent; name: "badge-close"; size: 20; color: enabled ? ThemeEngine.failRed : ThemeEngine.textMuted }
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
                    Accessible.name: Tr.deselectAll
                }
            }
        }

        // ── Test list — Flutter: ListView.separated with SwitchListTile ─
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
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
                        color: { let _ = configPollVersion; return appState.isDiagEnabled(modelData) ? ThemeEngine.passGreen : ThemeEngine.textMuted }
                    }

                    // Title + subtitle
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: getDisplayName(modelData)
                            font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.Medium; color: ThemeEngine.textPrimary
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: getDiagDescription(modelData)
                            font.family: ThemeEngine.monoFont; font.pixelSize: 11
                            color: Qt.alpha(ThemeEngine.textSecondary, 0.6)
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
                    }
                }
            }
        }
    }

    // ── Display names + descriptions — routed through Tr.diagName/diagDesc ──
    // 5WHY: _enNames duplicated AppState::staticDiagDisplayName() (C++).
    // Removed the parallel array; getDisplayName() now calls the Q_INVOKABLE
    // appState.diagDisplayName(diagId) directly — single source of truth.
    property var _enDescs: ["List all network adapters and their operational state",
        "Driver version, hardware info, and negotiated link speed",
        "Signal strength, SSID, channel, and link quality",
        "Ethernet link status, speed, and duplex mode",
        "DHCP lease info, server address, and expiration",
        "IP addresses, subnet mask, default gateway, DNS servers",
        "TCP/UDP connections: ESTABLISHED, LISTENING, etc.",
        "Active network profile type (Domain/Private/Public)",
        "TCP/IP stack parameters and configurations",
        "Default gateway reachability and response time",
        "IPv4 and IPv6 routing table entries",
        "ARP cache entries for local network discovery",
        "System proxy configuration and auto-detection",
        "Netskope client status and connection health",
        "Configured DNS servers and their responsiveness",
        "DNS resolver cache entries and statistics",
        "Check for DNS hijacking or spoofing indicators",
        "Connectivity check + Speedtest.net bandwidth test",
        "Connectivity check + Speedtest.net bandwidth test",
        "Resolve target hostname to IP address(es)",
        "TCP connect round-trip time and packet loss",
        "Route path and per-hop latency to target",
        "Combined traceroute and ping with per-hop loss",
        "Path MTU discovery to target host",
        "Parse and validate the target URL components",
        "TCP connectivity check to the URL host on default port",
        "Service banner detection for text-based protocols",
        "HTTP request/response headers and timing",
        "HTTP response headers from the target server",
        "Security-related HTTP headers (HSTS, CSP, etc.)",
        "SSL/TLS certificate chain and validity check",
        "HTTP redirect chain and final destination",
        "Supported compression methods and encoding",
        "HTTP request timing breakdown (DNS, connect, SSL, etc.)",
        "FTP service reachability and banner detection",
        "SSH version and key exchange detection",
        "SMTP/IMAP/POP3 service detection and banner",
        "Telnet service connectivity and banner",
        "MySQL server connectivity and version",
        "PostgreSQL server connectivity and version",
        "Redis server connectivity and response",
        "MongoDB server connectivity and status",
        "LDAP server connectivity and schema",
        "MQTT broker connectivity and response"]
    function getDisplayName(diagId) {
        // Use translated name when available (non-EN), fallback to C++ static array
        var tr = Tr.diagName(diagId)
        if (tr !== "") return tr
        // 5WHY: _enNames was a stale duplicate of staticDiagDisplayName().
        // Call the Q_INVOKABLE directly for a single source of truth.
        return appState.diagDisplayName(diagId)
    }
    function getDiagDescription(diagId) {
        var tr = Tr.diagDesc(diagId)
        if (tr !== "") return tr
        return (diagId >= 0 && diagId < _enDescs.length) ? _enDescs[diagId] : ""
    }
    function getDiagCountForGroup(groupIdx) {
        return appState.allDiagIdsForGroup(groupIdx).length
    }
}
