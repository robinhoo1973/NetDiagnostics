// =============================================================================
// V030TerminalFormat.h — v0.0.3 终端文本逐字复刻层（G1 家族）
//
// 5WHY (2026-08-21 用户诉求 "逐字复刻 v0.0.3 终端格式"): 属性派生转储
// （"label: value" 两空格缩进）与历史版本逐探针格式化文本（ipconfig
// 风格头 + 列对齐表 + 分隔线）不符——详情页终端与 v0.0.3 对不上。
// v0.0.3 各探针输出格式由 review/history/NetDiagnostics-master.0.0.3.zip
// 源码逐字提取：本层消费当前探针的结构化 props（数据同源），按 v0.0.3
// 逐字重建文本。仅覆盖 v0.0.3 有专属格式而当前探针无 details 的 G1
// 家族；G2-G5 探针在探针层直接输出 v0.0.3 文本（见各自 Adapters.cpp）。
// 表格式复用 DiagnosticFormatter::formatTable（与 v0.0.3 逐字节一致）。
//
// 5WHY (复核 2026-08-21 逐字复审): 首版复刻层与 v0.0.3 源码逐行比对后
// 修正十处偏差——标签点列数（Physical Address 9/DHCP Enabled 11/Default
// Gateway 9/IP Routing Enabled 8/DNS Servers 11 点）、NicAdvanced Speed
// 曾读不存在的 "speed" 子属性（速度存主值，恒 "-"）、WiFi 曾以
// numericPart 剥掉 v0.0.3 表格内的 " (2.437 GHz)"/" dBm" 显示串、
// Wired Link 列曾恒 "-"（v0.0.3 = /sys carrier）、ActiveConnections 表头
// 曾 "Remote Address"（v0.0.3 = "Foreign Address"）、空态曾发明 v0.0.3
// 没有的文本行（v0.0.3 空态 = 表头 + 空表）、DHCP 附注曾 "Lease Time"
// （v0.0.3 = Lease Renew/Lease Expires）、MAC 空值曾落 "no MAC" 或泄漏
// 速度串（v0.0.3 = "-"）、ipconfig 逐适配器块曾漏 Connection-specific
// DNS Suffix/DNS Servers(逐适配器)/Link Speed/MTU 行且 loopback 曾误标
// Ethernet adapter（v0.0.3 = Unknown adapter）。修正后与 v0.0.3 逐字节
// 对齐（可复现差异：探针无数据源的 v0.0.3 行——Description/DHCP Server/
// GUID——诚实省略而非伪造）。
// =============================================================================
#pragma once

#include "Common/Model/DiagnosticResult.h"
#include "Diagnostics/View/DiagnosticFormatter.h"

namespace SystemDiagnostics {

// 子属性取值（无则空串）
inline QString childVal(const ResultProperty& p, const QString& key) {
    for (const auto& c : p.children)
        if (c.label == key) return c.value;
    return QString();
}

// IPv4 前缀长 → 点分掩码（CIDR 呈现还原 ipconfig 风格 Subnet Mask 行）
inline QString maskFromPrefix(int prefix) {
    if (prefix < 0 || prefix > 32) return QString();
    const quint32 m = prefix == 0 ? 0 : (0xFFFFFFFFu << (32 - prefix));
    return QStringLiteral("%1.%2.%3.%4")
        .arg((m >> 24) & 0xFF).arg((m >> 16) & 0xFF)
        .arg((m >> 8) & 0xFF).arg(m & 0xFF);
}

// ── 逐 DiagId 复刻（返回空列表 = 无该 id 的复刻层，调用方回退 props 转储）──
inline QStringList v030TerminalLines(DiagId id,
                                     const QVector<ResultProperty>& props,
                                     const QVariantMap& data) {
    QStringList out;

    switch (id) {
    // ── G1NetworkAdapters — "Network Adapters (ifconfig -s style)" + 表 ──
    // v0.0.3：恒输出头 + 表（零行时 formatTable 仍出表头/分隔线）；
    // MAC 无值 = "-"（loopback 全零 MAC 亦 "-"）；多 IPv4 以 ',' 连接。
    case DiagId::G1NetworkAdapters: {
        out.append(QString());
        out.append(QStringLiteral("Network Adapters (ifconfig -s style)"));
        out.append(QString());
        static const QVector<DiagnosticFormatter::ColSpec> kNetCols = {
            {"Iface",       12, false},
            {"MTU",          4, true},
            {"Status",      10, false},
            {"MAC Address", 20, false},
            {"IPv4 Address", 0, false},
        };
        QList<QStringList> rows;
        for (const auto& p : props) {
            const bool isLoop = childVal(p, QStringLiteral("type")) == QLatin1String("loopback");
            QString mac = p.value;
            if (mac.isEmpty() || mac == QLatin1String("no MAC")
                || mac == QLatin1String("00:00:00:00:00:00"))
                mac = QStringLiteral("-");   // v0.0.3：无/全零 MAC → "-"
            QStringList ips4;
            for (const auto& c : p.children)
                if (c.label == QLatin1String("IPv4")) ips4.append(c.value);
            const QString ip4 = ips4.isEmpty()
                ? (isLoop ? QStringLiteral("127.0.0.1") : QStringLiteral("-"))
                : ips4.join(QLatin1Char(','));   // v0.0.3: info.ips.join(',')
            const QString mtu = childVal(p, QStringLiteral("MTU"));
            QString status = childVal(p, QStringLiteral("status"));
            if (status.isEmpty()) status = isLoop ? QStringLiteral("UP") : QStringLiteral("DOWN");
            else if (isLoop) status = QStringLiteral("UP");   // v0.0.3 loopback 恒 UP
            rows.append({ p.label,
                mtu.isEmpty() ? QStringLiteral("-") : mtu,
                status,
                mac,
                ip4 });
        }
        out.append(DiagnosticFormatter::formatTable(kNetCols, rows));
        return out;
    }

    // ── G1NicAdvanced — "NIC Advanced Properties (table mode):" + 表 ──
    // v0.0.3 Linux：sysfs 原值（speed 裸 Mbps、duplex/carrier/operstate 原样，
    // 不可读 = "-"）。空态 = 头 + 空表（无发明文本行）。
    case DiagId::G1NicAdvanced: {
        out.append(QString());
        out.append(QStringLiteral("NIC Advanced Properties (table mode):"));
        out.append(QString());
#if defined(_WIN32)
        // v0.0.3 Windows：Speed 列宽 7（"1.0 Gbps"/"100 Mbps"/"N/A"），
        // Carrier On/Off、State Up/Down（"other" 按 v0.0.3 落 Down）。
        static const QVector<DiagnosticFormatter::ColSpec> kNicColsWin = {
            {"Interface",   12, false},
            {"Speed",        7, true},
            {"Duplex",       6, false},
            {"MTU",          4, true},
            {"Carrier",      7, false},
            {"State",       10, false},
            {"MAC Address", 17, false},
        };
        QList<QStringList> rows;
        for (const auto& p : props) {
            const QString speed = p.value == QLatin1String("unknown")
                ? QStringLiteral("N/A") : p.value;
            const QString dup = childVal(p, QStringLiteral("duplex"));
            const QString st = childVal(p, QStringLiteral("status"));
            const QString mac = childVal(p, QStringLiteral("MAC"));
            rows.append({ p.label,
                speed,
                dup.isEmpty() || dup == QLatin1String("unknown") ? QStringLiteral("N/A")
                    : (dup == QLatin1String("full") ? QStringLiteral("Full") : dup),
                childVal(p, QStringLiteral("MTU")).isEmpty() ? QStringLiteral("-")
                    : childVal(p, QStringLiteral("MTU")),
                QStringLiteral("-"),   // v0.0.3 Windows 有 MediaConnectState，本层无数据源 → "-"
                st == QLatin1String("up") ? QStringLiteral("Up") : QStringLiteral("Down"),
                mac.isEmpty() ? QStringLiteral("N/A") : mac });
        }
        out.append(DiagnosticFormatter::formatTable(kNicColsWin, rows));
        return out;
#else
        static const QVector<DiagnosticFormatter::ColSpec> kNicCols = {
            {"Interface",   12, false},
            {"Speed",        6, true},
            {"Duplex",       6, false},
            {"MTU",          4, true},
            {"Carrier",      7, false},
            {"State",       10, false},
            {"MAC Address", 17, false},
        };
        QList<QStringList> rows;
        for (const auto& p : props) {
            const QString rawSpeed = childVal(p, QStringLiteral("speed"));
            QString speed;
            if (!rawSpeed.isEmpty())
                speed = rawSpeed;                       // Linux：sysfs 裸 Mbps（v0.0.3 同源）
            else if (p.value.startsWith(QLatin1String("unknown")))
                speed = QStringLiteral("-");            // v0.0.3：不可读 = "-"
            else
                speed = p.value.section(QLatin1Char(' '), 0, 0);
            const QString dup = childVal(p, QStringLiteral("duplex"));
            const QString mac = childVal(p, QStringLiteral("MAC"));
            const QString st = childVal(p, QStringLiteral("state"));
            rows.append({ p.label,
                speed,
                (dup.isEmpty() || dup == QLatin1String("unknown")) ? QStringLiteral("-") : dup,
                childVal(p, QStringLiteral("MTU")).isEmpty() ? QStringLiteral("-")
                    : childVal(p, QStringLiteral("MTU")),
                childVal(p, QStringLiteral("carrier")).isEmpty() ? QStringLiteral("-")
                    : childVal(p, QStringLiteral("carrier")),
                st.isEmpty() ? QStringLiteral("-") : st,
                mac.isEmpty() ? QStringLiteral("-") : mac });
        }
        out.append(DiagnosticFormatter::formatTable(kNicCols, rows));
        return out;
#endif
    }

    // ── G1WifiDiagnostics — "Wireless LAN information:" + 表 ──
    // v0.0.3 Linux 表格列存完整显示串（channel "6 (2.437 GHz)"、
    // signal "-20 dBm"、bitrate 裸 "54.0"）——不得剥离为纯数值。
    case DiagId::G1WifiDiagnostics: {
        out.append(QString());
        out.append(QStringLiteral("Wireless LAN information:"));
        out.append(QString());
#if defined(_WIN32)
        // v0.0.3 Windows：netsh 风格逐接口行（无表）。GUID 行 v0.0.3 有、
        // 探针无数据源——诚实省略。
        bool any = false;
        for (const auto& p : props) {
            any = true;
            out.append(QStringLiteral("   Name . . . . . . . . . . . . : %1").arg(p.label));
            out.append(QStringLiteral("   State. . . . . . . . . . . . : %1").arg(p.value));
            const bool connected = p.value == QLatin1String("connected");
            if (connected) {
                const QString ssid = childVal(p, QStringLiteral("SSID"));
                const QString bssid = childVal(p, QStringLiteral("BSSID"));
                const QString channel = childVal(p, QStringLiteral("channel"));
                const QString signal = childVal(p, QStringLiteral("signal"));
                if (!ssid.isEmpty() && ssid != QLatin1String("-"))
                    out.append(QStringLiteral("   SSID. . . . . . . . . . . . . : %1").arg(ssid));
                if (!bssid.isEmpty() && bssid != QLatin1String("-"))
                    out.append(QStringLiteral("   BSSID . . . . . . . . . . . . : %1").arg(bssid));
                if (!channel.isEmpty() && channel != QLatin1String("-"))
                    out.append(QStringLiteral("   Channel. . . . . . . . . . . : %1").arg(channel));
                else
                    out.append(QStringLiteral("   Channel. . . . . . . . . . . : N/A"));
                const QString sec = childVal(p, QStringLiteral("security"));
                out.append(QStringLiteral("   Authentication. . . . . . . : %1")
                    .arg(sec.isEmpty() ? QStringLiteral("Unknown") : sec));
                if (!signal.isEmpty() && signal != QLatin1String("-"))
                    out.append(QStringLiteral("   Signal . . . . . . . . . . . : %1").arg(signal));
            }
            out.append(QString());
        }
        if (!any)
            out.append(QStringLiteral("  (no wireless interfaces detected)"));
        return out;
#else
        static const QVector<DiagnosticFormatter::ColSpec> kWifiCols = {
            {"Interface", 12, false},
            {"SSID",      20, false},
            {"BSSID",     17, false},
            {"Channel",    8, true},
            {"Signal",     7, true},
            {"Bitrate",    0, true},
        };
        QList<QStringList> rows;
        for (const auto& p : props) {
            const auto cv = [&p](const char* k) {
                const QString v = childVal(p, QLatin1String(k));
                return v.isEmpty() ? QStringLiteral("-") : v;
            };
            rows.append({ p.label,
                cv("SSID"),
                cv("BSSID"),
                cv("channel"),   // v0.0.3 表内保留 "6 (2.437 GHz)" 完整串
                cv("signal"),    // v0.0.3 表内保留 "-20 dBm" 完整串
                cv("bitrate") });
        }
        out.append(DiagnosticFormatter::formatTable(kWifiCols, rows));
        if (props.isEmpty())
            out.append(QStringLiteral("  (no wireless interfaces detected)"));
        return out;
#endif
    }

    // ── G1DhcpStatus — "DHCP Client Status" + 表 + 附注行 ──
    // v0.0.3：dhclient 文件附注（DHCP Server/Lease Renew/Lease Expires）在
    // 表前；systemd/nmcli/Windows 行无附注。附注仅限 dhclient 来源行
    // （expire/renew 子属性为 dhclient 解析专属标签）。
    case DiagId::G1DhcpStatus: {
        out.append(QString());
        out.append(QStringLiteral("DHCP Client Status"));
        out.append(QString());
        static const QVector<DiagnosticFormatter::ColSpec> kDhcpCols = {
            {"Interface", 18, false},
            {"DHCP",       6, false},
            {"IP Address", 18, false},
            {"Server",     0, false},
        };
        QList<QStringList> rows;
        QStringList extras;   // v0.0.3 dhclient 附注行风格 [iface] 前缀
        for (const auto& p : props) {
            const QString dhcp = childVal(p, QStringLiteral("DHCP"));
            if (dhcp.isEmpty()) continue;
            QString ip = p.value;
            // v0.0.3：无客户端 IP 一律 "-"（Likely 行/无 IP 语义一致）
            if (ip == QLatin1String("DHCP likely") || ip.startsWith(QLatin1String("(DHCP"))
                || ip == QLatin1String("(no IP)"))
                ip = QStringLiteral("-");
            QString server = childVal(p, QStringLiteral("server"));
            if (dhcp == QLatin1String("Likely")) {
                const QString gw = childVal(p, QStringLiteral("gateway"));
                if (server.isEmpty() && !gw.isEmpty()) server = gw;
            }
            // v0.0.3 附注仅出自 dhclient 租约文件（renew/expire 行）；
            // systemd/nmcli/Windows 行只入表（v0.0.3 同）。
            const QString renew = childVal(p, QStringLiteral("renew"));
            const QString expire = childVal(p, QStringLiteral("expire"));
            if (!renew.isEmpty() || !expire.isEmpty()) {
                if (!server.isEmpty())
                    extras.append(QStringLiteral("   [%1] DHCP Server . . . . . . . . . . . : %2")
                        .arg(p.label, server));
                if (!renew.isEmpty())
                    extras.append(QStringLiteral("   [%1] Lease Renew . . . . . . . . . . . : %2")
                        .arg(p.label, renew));
                if (!expire.isEmpty())
                    extras.append(QStringLiteral("   [%1] Lease Expires . . . . . . . . . . : %2")
                        .arg(p.label, expire));
            }
            rows.append({ p.label, dhcp, ip, server.isEmpty() ? QStringLiteral("-") : server });
        }
        for (const auto& e : extras) out.append(e);
        if (rows.isEmpty()) {
            out.append(QStringLiteral("   No DHCP lease information found (static IP or managed externally)"));
            return out;
        }
        out.append(DiagnosticFormatter::formatTable(kDhcpCols, rows));
        out.append(QString());
        return out;
    }

    // ── G1IpConfiguration — ipconfig 风格逐适配器块 ──
    // 标签点列数与 v0.0.3 逐字节一致；loopback = "Unknown adapter"；
    // fe80 前缀判 "fe80:"（v0.0.3 语义）；无 CIDR 前缀不印 Subnet Mask
    // （v0.0.3 无 netmask 同）；IPv6 %scope 剥离（v0.0.3 inet_ntop 裸地址）；
    // DNS Servers 逐适配器逐条（v0.0.3 同串复挂每适配器，非主机级一行）。
    case DiagId::G1IpConfiguration: {
        QString host;
        QStringList dnsList;
        for (const auto& p : props) {
            if (p.label == QLatin1String("Host Name")) host = p.value;
            else if (p.label == QLatin1String("DNS Servers"))
                dnsList = p.value.split(QStringLiteral(", "), Qt::SkipEmptyParts);
        }
#if defined(_WIN32)
        // v0.0.3 Windows ipconfig 头（Primary Dns Suffix/Node Type/WINS 行
        // 为 Windows 专属常量，Linux 头无此三行）。
        out.append(QString());
        out.append(QStringLiteral("Windows IP Configuration"));
        out.append(QString());
        out.append(QStringLiteral("   Host Name . . . . . . . . . . . . : %1")
            .arg(host.isEmpty() ? QStringLiteral("Unknown") : host));
        out.append(QStringLiteral("   Primary Dns Suffix  . . . . . . . :"));
        out.append(QStringLiteral("   Node Type . . . . . . . . . . . . : Hybrid"));
        out.append(QStringLiteral("   IP Routing Enabled. . . . . . . . : No"));
        out.append(QStringLiteral("   WINS Proxy Enabled. . . . . . . . : No"));
        out.append(QString());
#else
        out.append(QString());
        out.append(QStringLiteral("IP Configuration"));
        out.append(QString());
        out.append(QStringLiteral("   Host Name . . . . . . . . . . . . : %1")
            .arg(host.isEmpty() ? QStringLiteral("Unknown") : host));
        // v0.0.3：Linux 读 /proc/sys/net/ipv4/ip_forward（Yes/No），读不到
        // 印 Unknown；Windows 恒 No（上方分支）；iOS/macOS 恒 Unknown。
        const QString routing = data.contains(QStringLiteral("routingEnabled"))
            ? (data.value(QStringLiteral("routingEnabled")).toBool()
                ? QStringLiteral("Yes") : QStringLiteral("No"))
            : QStringLiteral("Unknown");
        out.append(QStringLiteral("   IP Routing Enabled. . . . . . . . : %1").arg(routing));
        out.append(QString());
#endif
        const auto adapterLabel = [](const ResultProperty& p) -> QString {
            const QString type = childVal(p, QStringLiteral("type"));
            if (type == QLatin1String("loopback"))
                return QStringLiteral("Unknown adapter %1:").arg(p.label);   // v0.0.3
#if defined(__APPLE__)
            // v0.0.3 Apple：en* 前缀判无线（无 /sys wireless 文件）
            if (type != QLatin1String("wireless") && p.label.startsWith(QLatin1String("en")))
                return QStringLiteral("Wireless LAN adapter %1:").arg(p.label);
#endif
            return type == QLatin1String("wireless")
                ? QStringLiteral("Wireless LAN adapter %1:").arg(p.label)
                : QStringLiteral("Ethernet adapter %1:").arg(p.label);
        };
        for (const auto& p : props) {
            const QString type = childVal(p, QStringLiteral("type"));
            const QString gw = childVal(p, QStringLiteral("gateway"));
            if (type.isEmpty() && gw.isEmpty())
                continue;   // 主机级平铺行（Host Name/DNS Servers）已在上方处理
            out.append(adapterLabel(p));
            out.append(QString());
            out.append(QStringLiteral("   Connection-specific DNS Suffix  . :"));
            const QString mac = childVal(p, QStringLiteral("MAC"));
            if (!mac.isEmpty())
                out.append(QStringLiteral("   Physical Address. . . . . . . . . : %1").arg(mac));
            const QString dhcp = childVal(p, QStringLiteral("DHCP"));
            if (!dhcp.isEmpty())
                out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : %1").arg(dhcp));
#if defined(__APPLE__)
            // v0.0.3 Apple：DHCP 由系统管理恒 Yes（无子属性数据时）
            else if (type != QLatin1String("loopback"))
                out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : Yes"));
#endif
            out.append(QStringLiteral("   Autoconfiguration Enabled . . . . : Yes"));
            // v0.0.3：先全部 IPv6（fe80: 判 link-local），后全部 IPv4
            for (const auto& c : p.children) {
                if (c.label != QLatin1String("IPv6")) continue;
                const QString ip6 = c.value.section(QLatin1Char('/'), 0, 0)
                    .section(QLatin1Char('%'), 0, 0);   // v0.0.3 inet_ntop 裸地址无 scope
                if (ip6.startsWith(QLatin1String("fe80:")))
                    out.append(QStringLiteral("   Link-local IPv6 Address . . . . . : %1 (Preferred)").arg(ip6));
                else
                    out.append(QStringLiteral("   IPv6 Address. . . . . . . . . . . : %1 (Preferred)").arg(ip6));
            }
            for (const auto& c : p.children) {
                if (c.label != QLatin1String("IPv4")) continue;
                const QString ip = c.value.section(QLatin1Char('/'), 0, 0);
                const QString prefixStr = c.value.section(QLatin1Char('/'), 1, 1);
                out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1 (Preferred)").arg(ip));
                if (!prefixStr.isEmpty()) {   // v0.0.3：无 netmask 不印 Subnet Mask 行
                    const QString mask = maskFromPrefix(prefixStr.toInt());
                    if (!mask.isEmpty())
                        out.append(QStringLiteral("   Subnet Mask . . . . . . . . . . . : %1").arg(mask));
                }
            }
            if (!gw.isEmpty())
                for (const QString& g : gw.split(QStringLiteral(", "), Qt::SkipEmptyParts))
                    out.append(QStringLiteral("   Default Gateway . . . . . . . . . : %1").arg(g));
            // v0.0.3：DNS Servers 行逐适配器逐条重复（同一主机级列表）
            for (const QString& d : dnsList)
                out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . : %1").arg(d));
            const QString linkSpeed = childVal(p, QStringLiteral("link speed"));
            if (!linkSpeed.isEmpty())
                out.append(QStringLiteral("   Link Speed . . . . . . . . . . . . : %1 Mbps").arg(linkSpeed));
            const QString mtu = childVal(p, QStringLiteral("MTU"));
            if (!mtu.isEmpty())
                out.append(QStringLiteral("   MTU . . . . . . . . . . . . . . . : %1").arg(mtu));
#if defined(_WIN32)
            out.append(QStringLiteral("   NetBIOS over Tcpip. . . . . . . . : Enabled"));
#endif
            out.append(QString());
        }
        return out;
    }

    // ── G1ActiveConnections — "Active Connections (netstat -an style)" + 表 ──
    // v0.0.3：Proto 大写（TCP/TCP6/UDP/UDP6）、表头 "Foreign Address"、
    // 端点 ip:port 左对齐填充（ip 列宽取最长、port 最小 5）。
    case DiagId::G1ActiveConnections: {
        out.append(QString());
        out.append(QStringLiteral("Active Connections (netstat -an style)"));
        out.append(QString());
        if (props.isEmpty()) {
            out.append(QStringLiteral("  (no active connections)"));
            return out;
        }
        static const QVector<DiagnosticFormatter::ColSpec> kConnCols = {
            {"Proto",           6, false},
            {"Local Address",   0, false},
            {"Foreign Address", 0, false},
            {"State",           0, false},
        };
        // v0.0.3 行构造：ip.leftJustified(列内最长 ip) : port.leftJustified(≥5)
        struct Ep { QString ip, port; };
        QVector<Ep> locals, remotes;
        int localIpW = 0, remoteIpW = 0, portW = 5;
        const auto splitEp = [](const QString& s) {
            Ep e;
            const int cut = s.lastIndexOf(QLatin1Char(':'));
            if (cut > 0) { e.ip = s.left(cut); e.port = s.mid(cut + 1); }
            else e.ip = s;
            return e;
        };
        for (const auto& p : props) {
            locals.append(splitEp(childVal(p, QStringLiteral("local"))));
            remotes.append(splitEp(childVal(p, QStringLiteral("remote"))));
            localIpW = qMax(localIpW, locals.last().ip.length());
            remoteIpW = qMax(remoteIpW, remotes.last().ip.length());
            portW = qMax(portW, qMax(locals.last().port.length(), remotes.last().port.length()));
        }
        QList<QStringList> rows;
        for (int i = 0; i < props.size(); ++i) {
            const auto fmtEp = [](const Ep& e, int ipW, int portW) {
                return QStringLiteral("%1:%2")
                    .arg(e.ip.leftJustified(ipW), e.port.leftJustified(portW));
            };
            rows.append({ childVal(props[i], QStringLiteral("proto")),
                fmtEp(locals[i], localIpW, portW),
                fmtEp(remotes[i], remoteIpW, portW),
                childVal(props[i], QStringLiteral("state")) });
        }
        out.append(DiagnosticFormatter::formatTable(kConnCols, rows));
        return out;
    }

    // ── G1CellularInfo — "Cellular Information:" + 逐行 ──
    // v0.0.3：iOS 有 SIM 线（Carrier/Radio Access/IP/Gateway/Signal）；
    // 非 iOS 恒 "  [Skipped] Cellular info requires iOS — not applicable
    // on this platform"（Linux 探针 mmcli 有数据时 details 非空，本层仅
    // 兜底 Skipped/空态，语义与 v0.0.3 逐字对齐）。
    case DiagId::G1CellularInfo: {
        out.append(QString());
        out.append(QStringLiteral("Cellular Information:"));
        out.append(QString());
#if !defined(PLATFORM_IOS)
        out.append(QStringLiteral("  [Skipped] Cellular info requires iOS — not applicable on this platform"));
        out.append(QString());
        return out;
#else
        bool any = false;
        for (const auto& p : props) {
            const QString carrier = childVal(p, QStringLiteral("carrier"));
            const QString rat = childVal(p, QStringLiteral("radio access"));
            const QString sig = childVal(p, QStringLiteral("signal"));
            const QStringList ips4 = [&]() {
                QStringList v;
                for (const auto& c : p.children)
                    if (c.label == QLatin1String("IPv4")) v.append(c.value);
                return v;
            }();
            if (carrier.isEmpty() && rat.isEmpty() && sig.isEmpty()
                && ips4.isEmpty() && !p.label.contains(QLatin1String("modem"), Qt::CaseInsensitive))
                continue;
            any = true;
            out.append(QStringLiteral("  %1:").arg(p.label));
            if (!carrier.isEmpty()) out.append(QStringLiteral("  Carrier: %1").arg(carrier));
            if (!rat.isEmpty()) out.append(QStringLiteral("  Radio Access: %1").arg(rat));
            if (!ips4.isEmpty()) out.append(QStringLiteral("  IP Address: %1").arg(ips4.join(QStringLiteral(", "))));
            if (!sig.isEmpty()) out.append(QStringLiteral("  Signal: %1").arg(sig));
            out.append(QString());
        }
        if (!any)
            out.append(QStringLiteral("  No cellular service available"));
        return out;
#endif
    }

    // ── G1WiredDiagnostics — "Wired Information (table mode):" + 表 ──
    // v0.0.3 Linux：Speed 裸 Mbps、Link = /sys carrier（"0"/"1"）、
    // duplex/state 原样、MAC 无值 "-"；空态 = 头 + 空表 + 说明行。
    case DiagId::G1WiredDiagnostics: {
        out.append(QString());
        out.append(QStringLiteral("Wired Information (table mode):"));
        out.append(QString());
        static const QVector<DiagnosticFormatter::ColSpec> kWiredCols = {
            {"Interface",   12, false},
            {"Speed",        6, true},
            {"Duplex",       6, false},
            {"MTU",          4, true},
            {"Link",         4, false},
            {"State",       10, false},
            {"MAC Address", 17, false},
        };
        QList<QStringList> rows;
        for (const auto& p : props) {
            const QString linkSpeed = childVal(p, QStringLiteral("link speed"));
            const QString duplex = childVal(p, QStringLiteral("duplex"));
            const QString state = childVal(p, QStringLiteral("state"));
            const QString mac = childVal(p, QStringLiteral("MAC"));
            rows.append({ p.label,
                linkSpeed.isEmpty() ? QStringLiteral("-") : linkSpeed.section(QLatin1Char(' '), 0, 0),
                duplex.isEmpty() ? QStringLiteral("-") : duplex,
                p.value.isEmpty() ? QStringLiteral("-") : p.value,   // Wired 组主值 = MTU
                childVal(p, QStringLiteral("carrier")).isEmpty() ? QStringLiteral("-")
                    : childVal(p, QStringLiteral("carrier")),   // v0.0.3 Link = /sys carrier
                state.isEmpty() ? QStringLiteral("-") : state,
                mac.isEmpty() ? QStringLiteral("-") : mac });
        }
        out.append(DiagnosticFormatter::formatTable(kWiredCols, rows));
        if (props.isEmpty())
            out.append(QStringLiteral("  (no wired interfaces detected)"));
        return out;
    }

    default:
        return {};   // 无复刻层 → 调用方回退 props 转储
    }
}

} // namespace SystemDiagnostics
