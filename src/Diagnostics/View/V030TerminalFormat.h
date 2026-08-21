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

// 信号/channel 数值剥离（props 存 "6 (2.437 GHz)" / "-20 dBm" 类显示串，
// v0.0.3 表格列为纯数值）
inline QString numericPart(const QString& v) {
    return v.section(QLatin1Char(' '), 0, 0);
}

// ── 逐 DiagId 复刻（返回空列表 = 无该 id 的复刻层，调用方回退 props 转储）──
inline QStringList v030TerminalLines(DiagId id,
                                     const QVector<ResultProperty>& props,
                                     const QVariantMap& data) {
    QStringList out;
    Q_UNUSED(data);

    switch (id) {
    // ── G1NetworkAdapters — "Network Adapters (ifconfig -s style)" + 表 ──
    case DiagId::G1NetworkAdapters: {
        if (props.isEmpty())
            return { QStringLiteral("No adapters found") };
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
            const QString ip4 = childVal(p, QStringLiteral("IPv4"));
            rows.append({ p.label,
                childVal(p, QStringLiteral("MTU")),
                childVal(p, QStringLiteral("status")),
                p.value,
                ip4.isEmpty() ? (childVal(p, QStringLiteral("type")) == QLatin1String("loopback")
                                  ? QStringLiteral("127.0.0.1") : QStringLiteral("-")) : ip4 });
        }
        out.append(DiagnosticFormatter::formatTable(kNetCols, rows));
        return out;
    }

    // ── G1NicAdvanced — "NIC Advanced Properties (table mode):" + 表 ──
    case DiagId::G1NicAdvanced: {
        out.append(QString());
        out.append(QStringLiteral("NIC Advanced Properties (table mode):"));
        out.append(QString());
        if (props.isEmpty()) {
            out.append(QStringLiteral("  (no interfaces detected)"));
            return out;
        }
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
            rows.append({ p.label,
                childVal(p, QStringLiteral("speed")).isEmpty() ? QStringLiteral("-")
                    : numericPart(childVal(p, QStringLiteral("speed"))),
                childVal(p, QStringLiteral("duplex")).isEmpty() ? QStringLiteral("-")
                    : childVal(p, QStringLiteral("duplex")),
                childVal(p, QStringLiteral("MTU")).isEmpty() ? QStringLiteral("-")
                    : childVal(p, QStringLiteral("MTU")),
                childVal(p, QStringLiteral("carrier")).isEmpty() ? QStringLiteral("-")
                    : childVal(p, QStringLiteral("carrier")),
                childVal(p, QStringLiteral("state")).isEmpty() ? QStringLiteral("-")
                    : childVal(p, QStringLiteral("state")),
                childVal(p, QStringLiteral("MAC")).isEmpty() ? p.value
                    : childVal(p, QStringLiteral("MAC")) });
        }
        out.append(DiagnosticFormatter::formatTable(kNicCols, rows));
        return out;
    }

    // ── G1WifiDiagnostics — "Wireless LAN information:" + 表 ──
    case DiagId::G1WifiDiagnostics: {
        out.append(QString());
        out.append(QStringLiteral("Wireless LAN information:"));
        out.append(QString());
        if (props.isEmpty()) {
            out.append(QStringLiteral("  (no wireless interfaces detected)"));
            return out;
        }
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
            rows.append({ p.label,
                childVal(p, QStringLiteral("SSID")),
                childVal(p, QStringLiteral("BSSID")),
                numericPart(childVal(p, QStringLiteral("channel"))),
                numericPart(childVal(p, QStringLiteral("signal"))),
                numericPart(childVal(p, QStringLiteral("bitrate"))) });
        }
        out.append(DiagnosticFormatter::formatTable(kWifiCols, rows));
        return out;
    }

    // ── G1DhcpStatus — "DHCP Client Status" + 表 + 附注行 ──
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
        QStringList extras;   // v0.0.3 dhclient 补充行风格 [iface] 前缀
        for (const auto& p : props) {
            const QString dhcp = childVal(p, QStringLiteral("DHCP"));
            if (dhcp.isEmpty()) continue;
            QString ip = p.value;
            if (ip == QLatin1String("DHCP likely") || ip.startsWith(QLatin1String("(DHCP")))
                ip = QStringLiteral("-");
            QString server = childVal(p, QStringLiteral("server"));
            if (dhcp == QLatin1String("Likely")) {
                const QString gw = childVal(p, QStringLiteral("gateway"));
                if (server.isEmpty() && !gw.isEmpty()) server = gw;
            }
            rows.append({ p.label, dhcp, ip, server.isEmpty() ? QStringLiteral("-") : server });
            if (!server.isEmpty())
                extras.append(QStringLiteral("   [%1] DHCP Server . . . . . . . . . . . : %2")
                    .arg(p.label, server));
            const QString lt = childVal(p, QStringLiteral("lease time"));
            if (!lt.isEmpty())
                extras.append(QStringLiteral("   [%1] Lease Time . . . . . . . . . . . . : %2")
                    .arg(p.label, lt));
        }
        if (rows.isEmpty()) {
            out.append(QStringLiteral("   No DHCP lease information found (static IP or managed externally)"));
            return out;
        }
        out.append(DiagnosticFormatter::formatTable(kDhcpCols, rows));
        for (const auto& e : extras) out.append(e);
        out.append(QString());
        return out;
    }

    // ── G1IpConfiguration — ipconfig 风格逐适配器块 ──
    case DiagId::G1IpConfiguration: {
        out.append(QString());
        out.append(QStringLiteral("IP Configuration"));
        out.append(QString());
        QString host;
        QStringList dnsList;
        for (const auto& p : props) {
            if (p.label == QLatin1String("Host Name")) host = p.value;
            else if (p.label == QLatin1String("DNS Servers"))
                dnsList = p.value.split(QStringLiteral(", "), Qt::SkipEmptyParts);
        }
        out.append(QStringLiteral("   Host Name . . . . . . . . . . . . : %1")
            .arg(host.isEmpty() ? QStringLiteral("Unknown") : host));
        out.append(QStringLiteral("   IP Routing Enabled. . . . . . . . . : %1")
            .arg(data.value(QStringLiteral("routingEnabled")).toBool()
                ? QStringLiteral("Yes") : QStringLiteral("No")));
        if (!dnsList.isEmpty())
            out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . . : %1")
                .arg(dnsList.join(QLatin1Char(' '))));
        out.append(QString());
        for (const auto& p : props) {
            const QString type = childVal(p, QStringLiteral("type"));
            if (type.isEmpty()) continue;   // 非适配器组（主机级平铺行已在上方处理）
            out.append(type == QLatin1String("wireless")
                ? QStringLiteral("Wireless LAN adapter %1:").arg(p.label)
                : QStringLiteral("Ethernet adapter %1:").arg(p.label));
            out.append(QString());
            const QString mac = childVal(p, QStringLiteral("MAC"));
            if (!mac.isEmpty())
                out.append(QStringLiteral("   Physical Address. . . . . . . . . . : %1").arg(mac));
            out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . . : %1")
                .arg(childVal(p, QStringLiteral("DHCP")).isEmpty()
                    ? QStringLiteral("No") : childVal(p, QStringLiteral("DHCP"))));
            out.append(QStringLiteral("   Autoconfiguration Enabled . . . . : Yes"));
            for (const auto& c : p.children) {
                if (c.label == QLatin1String("IPv6")) {
                    // v0.0.3 为裸地址（props 带 /64 CIDR——复刻剥离，与 IPv4 一致）
                    const QString ip6 = c.value.section(QLatin1Char('/'), 0, 0);
                    if (ip6.startsWith(QLatin1String("fe80")))
                        out.append(QStringLiteral("   Link-local IPv6 Address . . . . . : %1 (Preferred)").arg(ip6));
                    else
                        out.append(QStringLiteral("   IPv6 Address. . . . . . . . . . . : %1 (Preferred)").arg(ip6));
                } else if (c.label == QLatin1String("IPv4")) {
                    const QString ip = c.value.section(QLatin1Char('/'), 0, 0);
                    const QString prefixStr = c.value.section(QLatin1Char('/'), 1, 1);
                    const int prefix = prefixStr.toInt();
                    out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1 (Preferred)").arg(ip));
                    const QString mask = maskFromPrefix(prefix);
                    if (!mask.isEmpty())
                        out.append(QStringLiteral("   Subnet Mask . . . . . . . . . . . : %1").arg(mask));
                }
            }
            const QString gw = childVal(p, QStringLiteral("gateway"));
            if (!gw.isEmpty())
                out.append(QStringLiteral("   Default Gateway . . . . . . . . . . : %1").arg(gw));
            out.append(QString());
        }
        return out;
    }

    // ── G1ActiveConnections — "Active Connections (netstat -an style)" + 表 ──
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
            {"Remote Address",  0, false},
            {"State",           0, false},
        };
        QList<QStringList> rows;
        for (const auto& p : props) {
            rows.append({ childVal(p, QStringLiteral("proto")),
                childVal(p, QStringLiteral("local")),
                childVal(p, QStringLiteral("remote")),
                childVal(p, QStringLiteral("state")) });
        }
        out.append(DiagnosticFormatter::formatTable(kConnCols, rows));
        return out;
    }

    // ── G1CellularInfo — "Cellular Information:" + 逐行 ──
    case DiagId::G1CellularInfo: {
        out.append(QString());
        out.append(QStringLiteral("Cellular Information:"));
        out.append(QString());
        bool any = false;
        for (const auto& p : props) {
            const QString carrier = childVal(p, QStringLiteral("carrier"));
            const QString rat = childVal(p, QStringLiteral("radio access"));
            const QString sig = childVal(p, QStringLiteral("signal"));
            const QString mcc = childVal(p, QStringLiteral("MCC/MNC"));
            const QStringList ips4 = [&]() {
                QStringList v;
                for (const auto& c : p.children)
                    if (c.label == QLatin1String("IPv4")) v.append(c.value);
                return v;
            }();
            if (carrier.isEmpty() && rat.isEmpty() && sig.isEmpty() && mcc.isEmpty()
                && ips4.isEmpty() && !p.label.contains(QLatin1String("modem"), Qt::CaseInsensitive))
                continue;
            any = true;
            out.append(QStringLiteral("  %1:").arg(p.label));
            if (!carrier.isEmpty()) out.append(QStringLiteral("  Carrier: %1").arg(carrier));
            if (!rat.isEmpty()) out.append(QStringLiteral("  Radio Access: %1").arg(rat));
            if (!ips4.isEmpty()) out.append(QStringLiteral("  IP Address: %1").arg(ips4.join(QStringLiteral(", "))));
            if (!mcc.isEmpty()) out.append(QStringLiteral("  Operator Code: %1").arg(mcc));
            if (!sig.isEmpty()) out.append(QStringLiteral("  Signal: %1").arg(sig));
            out.append(QString());
        }
        if (!any)
            out.append(QStringLiteral("  No cellular service available"));
        return out;
    }

    // ── G1WiredDiagnostics — "Wired Information (table mode):" + 表 ──
    case DiagId::G1WiredDiagnostics: {
        out.append(QString());
        out.append(QStringLiteral("Wired Information (table mode):"));
        out.append(QString());
        if (props.isEmpty()) {
            out.append(QStringLiteral("  (no wired interfaces detected)"));
            return out;
        }
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
            rows.append({ p.label,
                numericPart(childVal(p, QStringLiteral("link speed"))),
                childVal(p, QStringLiteral("duplex")).isEmpty() ? QStringLiteral("-")
                    : childVal(p, QStringLiteral("duplex")),
                p.value,   // Wired 组主值 = MTU
                QStringLiteral("-"),
                childVal(p, QStringLiteral("state")).isEmpty() ? QStringLiteral("-")
                    : childVal(p, QStringLiteral("state")),
                childVal(p, QStringLiteral("MAC")) });
        }
        out.append(DiagnosticFormatter::formatTable(kWiredCols, rows));
        return out;
    }

    default:
        return {};   // 无复刻层 → 调用方回退 props 转储
    }
}

} // namespace SystemDiagnostics
