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
    color: Qt.alpha(ThemeEngine.colors.card, 0.5)
    border { width: 1; color: Qt.alpha(ThemeEngine.colors.secondary, 0.3) }
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
        if (_privateRegex.test(ip)) return T.tr("ipClassPrivate");
        if (_cgnatRegex.test(ip)) return T.tr("ipClassCgnat");
        if (ip.startsWith("169.254.")) return T.tr("ipClassApipa");
        if (ip.startsWith("127.")) return T.tr("ipClassLoopback");
        return T.tr("ipClassPublic");
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
            AppIcon { name: "info"; size: 14; color: ThemeEngine.colors.secondary }
            Item { width: 6 }
            Label {
                text: T.tr("targetAnalysis")
                font.family: ThemeEngine.monoFont; font.pixelSize: 12
                font.weight: Font.DemiBold; color: ThemeEngine.colors.secondary
            }
        }
        Item { Layout.preferredHeight: 6 }

        // Type
        RowLayout {
            Label { text: T.tr("targetTypeLabel"); font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.colors.textSecondary }
            Label {
                text: isUrl ? T.tr("targetTypeUrl") : (isIp ? T.tr("targetTypeIp") : (target !== "" ? T.tr("targetTypeHostname") : ""))
                font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
            }
        }
        // Host
        RowLayout {
            Label { text: T.tr("hostLabel"); font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.colors.textSecondary }
            Label {
                text: host
                font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary
            }
        }
        // IP Classification
        Label {
            visible: isIp
            // 5WHY: hardcoded leftPadding was LTR-only — indent the value under
            // the "Host" label on the start edge for both scripts.
            leftPadding: T.isRtl ? 0 : 70
            rightPadding: T.isRtl ? 70 : 0
            text: classifyIp(host)
            font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textSecondary
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
                        lines.push([T.tr("urlSchemeLabel"), u.protocol.replace(":","")]);
                        if (u.username) lines.push([T.tr("urlUserLabel"), u.username]);
                        lines.push([T.tr("urlHostLabel"), u.hostname]);
                        if (u.port && u.port !== "80" && u.port !== "443") lines.push([T.tr("urlPortLabel"), u.port]);
                        if (u.pathname && u.pathname !== "/") lines.push([T.tr("urlPathLabel"), u.pathname]);
                        if (u.search) lines.push([T.tr("urlQueryLabel"), u.search.substring(1)]);
                        if (u.hash) lines.push([T.tr("urlFragmentLabel"), u.hash.substring(1)]);
                    } catch(e) {
                        lines.push([T.tr("errorStatus"), T.tr("malformedUrlLabel")]);
                    }
                    return lines;
                }
                delegate: RowLayout {
                    property var pair: modelData
                    // 5WHY: label column padded to 10 chars — translated labels
                    // (e.g. CJK "查询参数") are wider than the old 8-char English
                    // ones; truncating them would corrupt the displayed word.
                    Label { text: (pair[0] + "          ").substring(0,10) + ":"; font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.colors.textSecondary }
                    Label { text: pair[1] || ""; font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textPrimary }
                }
            }
        }

        Item { Layout.preferredHeight: 4 }
        // Known Port Reference
        Label {
            text: T.tr("knownPortRef")
            font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textSecondary
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
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.cyan
                    Layout.preferredWidth: 110
                }
                Label {
                    visible: b !== null
                    text: ("     " + b[0]).slice(-5) + " " + b[1]
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textSecondary
                    Layout.preferredWidth: 110
                }
            }
        }
    }
}
