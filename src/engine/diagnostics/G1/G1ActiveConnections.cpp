#include "engine/diagnostics/GBase.h"
#include "engine/diagnostics/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult activeConnections(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.status = DiagStatus::Info;  // 5WHY: cppcheck found uninitialized r.status —
    // if no platform #if branch matches, returned result has undefined status
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Active Connections (netstat -an style)"));
    out.append(QString());

    // Collect raw data first to compute aligned IP:Port widths
    struct ConnEntry { QString proto, localIp, remoteIp, state; int localPort, remotePort; };
    QList<ConnEntry> rawConns;

#if defined(_WIN32)
    ULONG bufLen = 0;
    GetExtendedTcpTable(nullptr, &bufLen, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    QByteArray tcpBuf(bufLen, '\0');
    auto* tcpTable = (MIB_TCPTABLE_OWNER_PID*)tcpBuf.data();
    if (GetExtendedTcpTable(tcpTable, &bufLen, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) {
            auto& row = tcpTable->table[i];
            struct in_addr la, ra;
            la.S_un.S_addr = row.dwLocalAddr; ra.S_un.S_addr = row.dwRemoteAddr;
            rawConns.append({QStringLiteral("TCP"),
                ip4ToStr(la), ip4ToStr(ra),
                QString::fromLatin1(tcpStateName(row.dwState)),
                (int)ntohs((u_short)row.dwLocalPort), (int)ntohs((u_short)row.dwRemotePort)});
        }
    }
#else
    auto parseProcNet = [&](const QString& path, const QString& proto, bool isUdp) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;
        QTextStream ts(&f);
        ts.readLine();
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            QStringList cols = line.split(QRegularExpression("\\s+"));
            if (cols.size() < 10) continue;
            QStringList local = cols[1].split(':');
            QStringList remote = cols[2].split(':');
            auto hexToIp = [](const QString& hex) -> QString {
                bool ok; uint32_t ip = hex.toUInt(&ok, 16);
                return ipToStr(ip);
            };
            int state = cols[3].toInt(nullptr, 16);
            rawConns.append({proto,
                hexToIp(local[0]), remote.size()>0 ? hexToIp(remote[0]) : QStringLiteral("0.0.0.0"),
                isUdp ? QStringLiteral("*:*") : QString::fromLatin1(tcpStateName(state)),
                local[1].toInt(nullptr, 16),
                remote.size()>1 ? remote[1].toInt(nullptr, 16) : 0});
        }
    };
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
    parseProcNet(QStringLiteral("/proc/net/tcp"),  QStringLiteral("TCP"),  false);
    parseProcNet(QStringLiteral("/proc/net/tcp6"), QStringLiteral("TCP6"), false);
    parseProcNet(QStringLiteral("/proc/net/udp"),  QStringLiteral("UDP"),  true);
    parseProcNet(QStringLiteral("/proc/net/udp6"), QStringLiteral("UDP6"), true);
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    // ── macOS: enumerate TCP/UDP connections via sysctl pcblist ──────
    {
        auto macTcpState = [](int st) -> const char* {
            switch(st){case 0:return"CLOSED";case 1:return"LISTEN";
            case 2:return"SYN_SENT";case 3:return"SYN_RCVD";case 4:return"ESTABLISHED";
            case 5:return"CLOSE_WAIT";case 6:return"FIN_WAIT_1";case 7:return"CLOSING";
            case 8:return"LAST_ACK";case 9:return"FIN_WAIT_2";case 10:return"TIME_WAIT";
            default:return"UNKNOWN";}
        };
        auto macOSEnumerate = [&](const char* sysctlName, const QString& proto, bool isUdp) {
            size_t len = 0;
            if (sysctlbyname(sysctlName, nullptr, &len, nullptr, 0) != 0 || len < 32) return;
            QByteArray buf((int)len + 8192, '\0');
            if (sysctlbyname(sysctlName, buf.data(), &len, nullptr, 0) != 0) return;
            char* ptr = buf.data(); char* end = ptr + len;
            while (ptr + 8 <= end) {
                uint32_t elen = *(uint32_t*)ptr;          // xt_len
                if (elen < 40 || ptr + elen > end) break;
                // Scan for AF_INET sockaddr_in pair within this entry
                for (int off = 24; off + 32 <= (int)elen; off += 4) {
                    char* sp = ptr + off;
                    if (*(uint8_t*)sp == 16 && *(uint8_t*)(sp+1) == AF_INET) {
                        uint16_t lp = ntohs(*(uint16_t*)(sp + 2));
                        uint32_t la = ntohl(*(uint32_t*)(sp + 4));
                        uint16_t rp = ntohs(*(uint16_t*)(sp + 16 + 2));
                        uint32_t ra = ntohl(*(uint32_t*)(sp + 16 + 4));
                        // Search for TCP state byte (0–10) in remaining entry
                        int state = 0;
                        for (int o2 = off + 32; o2 + 1 < (int)elen; o2++) {
                            uint8_t v = *(uint8_t*)(ptr + o2);
                            if (v <= 10 && v != *(uint8_t*)(ptr + o2 - 1)) {
                                state = (int)v; break;
                            }
                        }
                        struct in_addr la_, ra_;
                        la_.s_addr = htonl(la); ra_.s_addr = htonl(ra);
                        rawConns.append({proto,
                            ip4ToStr(la_), ip4ToStr(ra_),
                            isUdp ? QStringLiteral("*:*") : QString::fromLatin1(macTcpState(state)),
                            (int)lp, (int)rp});
                        break;
                    }
                }
                ptr += elen;
            }
        };
        macOSEnumerate("net.inet.tcp.pcblist64", QStringLiteral("TCP"), false);
        macOSEnumerate("net.inet.udp.pcblist64", QStringLiteral("UDP"), true);
    }
#endif
#endif

    // Compute max IP width and max port width for each address column
    int localIpW = 0, remoteIpW = 0, portW = 0;
    for (const auto& c : rawConns) {
        localIpW  = qMax(localIpW,  static_cast<int>(c.localIp.length()));
        remoteIpW = qMax(remoteIpW, static_cast<int>(c.remoteIp.length()));
        portW     = qMax(portW, static_cast<int>(QString::number(c.localPort).length()));
        portW     = qMax(portW, static_cast<int>(QString::number(c.remotePort).length()));
    }
    if (portW < 5) portW = 5; // minimum port column width

    // Build formatted rows: IP left-justified + ":" + port left-justified
    QList<QStringList> connRows;
    for (const auto& c : rawConns) {
        connRows.append({
            c.proto,
            QStringLiteral("%1:%2").arg(c.localIp.leftJustified(localIpW)).arg(QString::number(c.localPort).leftJustified(portW)),
            QStringLiteral("%1:%2").arg(c.remoteIp.leftJustified(remoteIpW)).arg(QString::number(c.remotePort).leftJustified(portW)),
            c.state});
    }

    static const QVector<DiagnosticFormatter::ColSpec> kConnCols = {
        {"Proto",           6, false},
        {"Local Address",   0, false},
        {"Foreign Address", 0, false},
        {"State",           0, false},
    };
    if (!connRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kConnCols, connRows));
    else {
#if defined(PLATFORM_IOS)
        out.append(QStringLiteral("  [iOS] Active connections: unavailable (restricted by Apple)"));
        out.append(QStringLiteral("  iOS sandbox prevents reading /proc/net/tcp — use Xcode network monitor"));
#else
        out.append(QStringLiteral("  (no active connections)"));
#endif
    }
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
#if defined(PLATFORM_IOS)
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Unavailable on iOS (sandbox restricts socket enumeration)");
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    r.status = rawConns.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    r.summary = rawConns.isEmpty()
        ? QStringLiteral("No active connections found")
        : QStringLiteral("Active connections enumerated via sysctl");
#else
    r.status = DiagStatus::Pass;
    int tcpCount = 0, udpCount = 0;
    for (const auto& c : rawConns) {
        if (c.proto == QStringLiteral("UDP")) udpCount++;
        else tcpCount++;
    }
    r.summary = rawConns.isEmpty()
        ? QStringLiteral("No active connections")
        : QStringLiteral("%1 TCP + %2 UDP connections").arg(tcpCount).arg(udpCount);
#endif
#endif  // close converted #elif
#endif  // close converted #elif
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?Cellular Info (iOS: CoreTelephony; other platforms: not available)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
}
