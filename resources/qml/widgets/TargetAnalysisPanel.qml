import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts

// ── TargetAnalysisPanel — mirrors Flutter _buildTargetAnalysis ─────────
// Shows IP classification, URL component breakdown, and known port reference table.
Rectangle {
    id: root
    property string target: ""
    radius: 8
    color: Qt.alpha(ThemeEngine.bgCard, 0.5)
    border { width: 1; color: Qt.alpha(ThemeEngine.accentBlue, 0.3) }
    implicitHeight: analysisColumn.implicitHeight + 20
    visible: target !== ""

    property bool isUrl: target.startsWith("http://") || target.startsWith("https://")
    property bool isIp: _ipv4Regex.test(target)
    property string host: {
        if (isUrl) {
            try {
                var u = new URL(target);
                return u.hostname;
            } catch(e) { return target; }
        }
        return target;
    }

    readonly property var _ipv4Regex: /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$/
    readonly property var _privateRegex: /^(10\.|172\.(1[6-9]|2\d|3[01])\.|192\.168\.)/
    readonly property var _cgnatRegex: /^100\.(6[4-9]|[7-9]\d|1[01]\d|12[0-7])\./

    function classifyIp(ip) {
        if (_privateRegex.test(ip)) return "RFC1918 Private";
        if (_cgnatRegex.test(ip)) return "CGNAT (RFC6598)";
        if (ip.startsWith("169.254.")) return "APIPA Link-Local";
        if (ip.startsWith("127.")) return "Loopback";
        return "Public Routable";
    }

    readonly property var portRef: [
        ["21","FTP"],["22","SSH"],["23","Telnet"],["25","SMTP"],
        ["53","DNS"],["80","HTTP"],["110","POP3"],["135","RPC"],
        ["139","NetBIOS"],["143","IMAP"],["443","HTTPS"],["445","SMB"],
        ["993","IMAPS"],["995","POP3S"],["1433","SQL Svr"],["1723","PPTP"],
        ["3306","MySQL"],["3389","RDP"],["5432","PostgreSQL"],["5900","VNC"],
        ["6379","Redis"],["8080","HTTP-Alt"],["8443","HTTPS-Alt"],["27017","MongoDB"]
    ]

    ColumnLayout {
        id: analysisColumn
        anchors { fill: parent; margins: 10 }
        spacing: 0

        // Header
        RowLayout {
            AppIcon { name: "info"; size: 14; color: ThemeEngine.accentBlue }
            Item { width: 6 }
            Label {
                text: Tr.targetAnalysis
                font.family: ThemeEngine.monoFont; font.pixelSize: 11
                font.weight: Font.DemiBold; color: ThemeEngine.accentBlue
            }
        }
        Item { Layout.preferredHeight: 6 }

        // Type
        RowLayout {
            Label { text: Tr.targetTypeLabel; font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.DemiBold; color: ThemeEngine.textSecondary }
            Label {
                text: isUrl ? Tr.targetTypeUrl : (isIp ? Tr.targetTypeIp : (target !== "" ? Tr.targetTypeHostname : ""))
                font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.textPrimary
            }
        }
        // Host
        RowLayout {
            Label { text: "Host    :"; font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.DemiBold; color: ThemeEngine.textSecondary }
            Label {
                text: host
                font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.textPrimary
            }
        }
        // IP Classification
        Label {
            visible: isIp
            leftPadding: 70
            text: classifyIp(host)
            font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.textSecondary
        }

        // URL component breakdown
        ColumnLayout {
            visible: isUrl
            spacing: 0
            property var urlObj: null
            Component.onCompleted: {
                try { urlObj = new URL(target); } catch(e) {}
            }
            Repeater {
                model: {
                    var lines = [];
                    try {
                        var u = new URL(target);
                        lines.push(["Scheme", u.protocol.replace(":","")]);
                        if (u.username) lines.push(["User", u.username]);
                        lines.push(["Host", u.hostname]);
                        if (u.port && u.port !== "80" && u.port !== "443") lines.push(["Port", u.port]);
                        if (u.pathname && u.pathname !== "/") lines.push(["Path", u.pathname]);
                        if (u.search) lines.push(["Query", u.search.substring(1)]);
                        if (u.hash) lines.push(["Fragment", u.hash.substring(1)]);
                    } catch(e) {
                        lines.push(["Error", "Malformed URL"]);
                    }
                    return lines;
                }
                delegate: RowLayout {
                    property var pair: modelData
                    Label { text: (pair[0] + "       ").substring(0,8) + ":"; font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.DemiBold; color: ThemeEngine.textSecondary }
                    Label { text: pair[1] || ""; font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.textPrimary }
                }
            }
        }

        Item { Layout.preferredHeight: 4 }
        // Known Port Reference
        Label {
            text: Tr.knownPortRef
            font.family: ThemeEngine.monoFont; font.pixelSize: 9; color: ThemeEngine.textSecondary
        }
        Item { Layout.preferredHeight: 2 }
        Repeater {
            model: Math.ceil(portRef.length / 2)
            delegate: RowLayout {
                property int idx: index * 2
                property var a: portRef[idx]
                property var b: (idx + 1 < portRef.length) ? portRef[idx + 1] : null
                Label {
                    text: ("     " + a[0]).slice(-5) + " " + a[1]
                    font.family: ThemeEngine.monoFont; font.pixelSize: 9; color: ThemeEngine.cyan
                    Layout.preferredWidth: 110
                }
                Label {
                    visible: b !== null
                    text: ("     " + b[0]).slice(-5) + " " + b[1]
                    font.family: ThemeEngine.monoFont; font.pixelSize: 9; color: ThemeEngine.textSecondary
                    Layout.preferredWidth: 110
                }
            }
        }
    }
}
