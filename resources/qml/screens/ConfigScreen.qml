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
    property int _cachedConfigGen: -1
    property int configPollVersion: 0
    Connections {
        target: appState
        function onStateVersionChanged() {
            _cachedConfigGen = appState.stateVersion
            configPollVersion++
        }
    }

    // AppBar
    Rectangle {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        implicitHeight: 84; color: "#1A1A2E"
        border { width: 1; color: "#3A3A5A" }
        ColumnLayout {
            anchors.fill: parent; spacing: 0
            RowLayout {
                Layout.fillWidth: true; Layout.preferredHeight: 44
                anchors { leftMargin: 16; rightMargin: 16 }
                AppIcon { name: "config"; size: 20; color: ThemeEngine.cyan }
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
                            contentItem: Label {
                                anchors.centerIn: parent
                                text: "G" + (index + 1)
                                font.family: ThemeEngine.monoFont; font.pixelSize: 12
                                font.weight: index === currentGroup ? Font.DemiBold : Font.Normal
                                color: index === currentGroup ? ThemeEngine.cyan : ThemeEngine.textSecondary
                            }
                            onClicked: currentGroup = index
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
            border { width: 1; color: "#2A2A4A" }
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
                // Select All
                Rectangle {
                    implicitWidth: 110; implicitHeight: 32; radius: 6; color: "transparent"
                    border { width: 1; color: "#3A3A5A" }
                    enabled: !appState.isGroupAllEnabled(currentGroup)
                    opacity: enabled ? 1.0 : 0.4
                    RowLayout { anchors.centerIn: parent; spacing: 4
                        AppIcon { name: "check"; size: 14; color: ThemeEngine.textPrimary }
                        Label { text: Tr.selectAll; font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textPrimary }
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: parent.enabled
                        onClicked: appState.setGroupEnabled(currentGroup, true)
                    }
                }
                Item { width: 8 }
                // Deselect All
                Rectangle {
                    implicitWidth: 110; implicitHeight: 32; radius: 6; color: "transparent"
                    border { width: 1; color: "#3A3A5A" }
                    enabled: appState.isGroupAnyEnabled(currentGroup)
                    opacity: enabled ? 1.0 : 0.4
                    RowLayout { anchors.centerIn: parent; spacing: 4
                        AppIcon { name: "close"; size: 14; color: ThemeEngine.textPrimary }
                        Label { text: Tr.deselectAll; font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textPrimary }
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: parent.enabled
                        onClicked: appState.setGroupEnabled(currentGroup, false)
                    }
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
                        height: 1; color: "#2A2A4A"
                    }
                }

                RowLayout {
                    id: tileCol
                    anchors { fill: parent; leftMargin: 16; rightMargin: 16; topMargin: 8; bottomMargin: 8 }
                    spacing: 12

                    // Leading icon
                    AppIcon {
                        name: appState.isDiagEnabled(modelData) ? "badge-check" : "badge-circle"
                        size: 14
                        color: "white"
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
    property var _enNames: ["Network Adapters","NIC Advanced","WiFi Information","Wired Information",
        "DHCP Status","IP Configuration","Active Connections","Network Profile",
        "TCP Settings","Default Gateway","Routing Table","ARP Table","Proxy Settings",
        "Netskope Status","DNS Servers","DNS Cache","DNS Pollution",
        "Internet Connectivity && Speed","Internet Connectivity && Speed","DNS Resolution","Ping","Traceroute",
        "PathPing","MTU Discovery","Port Scan","URL Parsing","TCP Connect",
        "Service Banner","HTTP Request","HTTP Headers","Security Headers",
        "SSL Certificate","HTTP Redirect","HTTP Compression","HTTP Timing",
        "FTP Diagnostics","SSH Diagnostics","Email Diagnostics"]
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
        "TCP port scan (common / custom range / both)",
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
        "SMTP/IMAP/POP3 service detection and banner"]
    function getDisplayName(diagId) {
        // Use translated name when available (non-EN), fallback to English array
        var tr = Tr.diagName(diagId)
        if (tr !== "") return tr
        return (diagId >= 0 && diagId < _enNames.length) ? _enNames[diagId] : "Diag " + diagId
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
