// =============================================================================
// G1G2G3Native.cpp 闁?Pure C++ G1/G2/G3 diagnostics 闁?ZERO shell commands
// Linux: getifaddrs, /proc/net, /sys/class/net, ioctl, netlink, socket APIs
// Windows: GetAdaptersAddresses, GetExtendedTcpTable, GetIpForwardTable2, etc.
// Output format: matches Windows CLI tools (ipconfig, route print, arp -a,
// netstat -an, netsh, nslookup)
// =============================================================================
#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#include "engine/diagnostic/G1G2G3Native.h"
#include "util/DebugSwitch.h"
#include <QMutex>
#include <QElapsedTimer>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QVariantMap>
#include <QProcess>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <thread>
#include <climits>
#include <csignal>
#include "util/DnsResolver.h"
#include "util/DiagnosticFormatter.h"
#include "util/NetUtil.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <wlanapi.h>
#include <netioapi.h>
#include <winhttp.h>
#include <tlhelp32.h>
#define close closesocket
#elif defined(__APPLE__)
// macOS / iOS — use AF_LINK+sockaddr_dl, sysctl, routing socket
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#ifndef PLATFORM_IOS
// macOS-only headers (routing socket, kernel TCP/UDP/IP state — not in iOS SDK)
#include <net/route.h>
#include <netinet/tcp_var.h>
#include <netinet/udp_var.h>
#include <netinet/in_var.h>
#endif
#ifdef PLATFORM_IOS
#include "engine/IosWiFiHelper.h"
#include "engine/task/IosNetworkInfo.h"
#endif
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#ifndef PLATFORM_ANDROID
#include <linux/wireless.h>
#include <linux/sockios.h>
#include <linux/if_packet.h>
#endif
#endif

namespace G1G2G3Native {

static int tcpPingMs(const QString& host, int port); // forward


// SpeedTest class extracted to SpeedTest.h (header-only, platform-independent)
#include "engine/diagnostic/SpeedTest.h"


// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁?
// Helpers (continued)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁?

static QString macToStr(const unsigned char* mac) {
    return QStringLiteral("%1-%2-%3-%4-%5-%6")
        .arg(mac[0], 2, 16, QLatin1Char('0'))
        .arg(mac[1], 2, 16, QLatin1Char('0'))
        .arg(mac[2], 2, 16, QLatin1Char('0'))
        .arg(mac[3], 2, 16, QLatin1Char('0'))
        .arg(mac[4], 2, 16, QLatin1Char('0'))
        .arg(mac[5], 2, 16, QLatin1Char('0'));
}

// Thread-safe IPv4 formatting (inet_ntoa uses a shared static buffer and is
// unsafe in the concurrent diagnostic thread pool).
static QString ip4ToStr(struct in_addr a) {
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &a, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

static QString ipToStr(uint32_t ip) {
    struct in_addr a; a.s_addr = ip; // ip is already network byte order from /proc/net/*
    return ip4ToStr(a);
}

static bool hasNonEmptyValue(const QVariantMap& values, const char* key) {
    const auto it = values.constFind(QLatin1String(key));
    return it != values.cend() && !it->toString().trimmed().isEmpty();
}

static bool hasCellularIdentity(const QVariantMap& cell) {
    return hasNonEmptyValue(cell, "carrierName")
        || hasNonEmptyValue(cell, "radioAccess")
        || (hasNonEmptyValue(cell, "mcc") && hasNonEmptyValue(cell, "mnc"));
}

static QString cellularSummary(const QVariantMap& cell) {
    const QString carrier = cell.value(QStringLiteral("carrierName")).toString().trimmed();
    const QString radio = cell.value(QStringLiteral("radioAccess")).toString().trimmed();
    if (!carrier.isEmpty() && !radio.isEmpty())
        return QStringLiteral("Carrier: %1 (%2)").arg(carrier, radio);
    if (!carrier.isEmpty())
        return QStringLiteral("Carrier: %1").arg(carrier);
    if (!radio.isEmpty())
        return QStringLiteral("Radio: %1").arg(radio);
    return QStringLiteral("Cellular service detected");
}

#ifdef _WIN32
// MIB_TCP_STATE numbering (Windows) — differs from Linux /proc/net/tcp
static const char* tcpStateName(int st) {
    switch(st){case 1:return"CLOSED";case 2:return"LISTEN";case 3:return"SYN_SENT";
    case 4:return"SYN_RCVD";case 5:return"ESTABLISHED";case 6:return"FIN_WAIT1";
    case 7:return"FIN_WAIT2";case 8:return"CLOSE_WAIT";case 9:return"CLOSING";
    case 10:return"LAST_ACK";case 11:return"TIME_WAIT";case 12:return"DELETE_TCB";
    default:return"UNKNOWN";}
}
#else
// Linux /proc/net/tcp state numbering
static const char* tcpStateName(int st) {
    switch(st){case 1:return"ESTABLISHED";case 2:return"SYN_SENT";case 3:return"SYN_RECV";
    case 4:return"FIN_WAIT1";case 5:return"FIN_WAIT2";case 6:return"TIME_WAIT";
    case 7:return"CLOSE";case 8:return"CLOSE_WAIT";case 9:return"LAST_ACK";
    case 10:return"LISTEN";case 11:return"CLOSING";default:return"UNKNOWN";}
}
#endif

#ifndef _WIN32
// Parse /proc/net/tcp (or tcp6/udp/udp6) 闁?hex format:
//   sl local_address rem_address st tx_queue rx_queue tr tm->when retrnsmt uid timeout inode
// Example: 0: 00000000:0016 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 12345
struct ProcNetConn {
    QString localIp; int localPort;
    QString remoteIp; int remotePort;
    int state; // TCP state (0A=LISTEN, 01=ESTABLISHED etc.)
    uint32_t uid;
    bool isIPv6;
};
#endif

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?Network Adapters (ipconfig /all format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult netskopeStatus(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Security Proxy Status:"));
    out.append(QString());

    bool found = false;
#ifdef _WIN32
    // Check for nsproxy.exe process
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                QString name = QString::fromWCharArray(pe.szExeFile);
                if (name.contains("nsproxy", Qt::CaseInsensitive) || name.contains("zsproxy", Qt::CaseInsensitive) ||
                    name.contains("zscaler", Qt::CaseInsensitive) || name.contains("netskope", Qt::CaseInsensitive)) {
                    out.append(QStringLiteral("  Found: %1 (PID %2)").arg(name).arg(pe.th32ProcessID));
                    found = true;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
#else
    // Check /proc for nsproxy/netskope/zscaler processes
    QDir procDir(QStringLiteral("/proc"));
    for (const auto& fi : procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok; fi.fileName().toInt(&ok);
        if (!ok) continue; // not a PID directory
        QFile cmdLine(fi.absoluteFilePath() + "/comm");
        if (cmdLine.open(QIODevice::ReadOnly)) {
            QString comm = QString::fromLatin1(cmdLine.readAll().trimmed());
            if (comm.contains("nsproxy", Qt::CaseInsensitive) || comm.contains("zscaler", Qt::CaseInsensitive) ||
                comm.contains("netskope", Qt::CaseInsensitive) || comm.contains("zsproxy", Qt::CaseInsensitive)) {
                out.append(QStringLiteral("  Found: %1 (PID %2)").arg(comm, fi.fileName()));
                found = true;
            }
        }
    }
    if (!found) out.append(QStringLiteral("  No security proxy process detected"));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = found ? DiagStatus::Pass : DiagStatus::Info;
    r.summary = found ? QStringLiteral("Security proxy detected") : QStringLiteral("No security proxy detected");
    r.durationMs = 0;
    return r;
}

DiagnosticResult dnsServers(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out, dnsList;
    out.append(QString());
    out.append(QStringLiteral("DNS Server Configuration (table mode):"));
    out.append(QString());

    static const QVector<DiagnosticFormatter::ColSpec> kDnsCols = {
        {"Source",   20, false},
        {"DNS Server", 0, false},
    };
    QList<QStringList> dnsRows;

#ifdef _WIN32
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_UNICAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_ANYCAST, nullptr, adapters, &bufLen) == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next) {
            QString ifName = QString::fromWCharArray(a->FriendlyName);
            for (auto* dns = a->FirstDnsServerAddress; dns; dns = dns->Next) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(dns->Address.lpSockaddr, dns->Address.iSockaddrLength, nullptr, ip, &ipLen);
                QString ipStr = QString::fromLatin1(ip);
                dnsRows.append({ifName, ipStr});
                if (!dnsList.contains(ipStr)) dnsList.append(ipStr);
            }
        }
    }
#else
#ifdef PLATFORM_IOS
    // iOS: no /etc/resolv.conf 闁?use res_ninit
    struct __res_state res; memset(&res, 0, sizeof(res));
    if (res_ninit(&res) == 0) {
        for (int i = 0; i < res.nscount; i++) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &res.nsaddr_list[i].sin_addr, ip, sizeof(ip));
            dnsRows.append({QStringLiteral("System DNS"), QString::fromLatin1(ip)});
            dnsList.append(QString::fromLatin1(ip));
        }
        res_nclose(&res);
    }
#else
    // Read /etc/resolv.conf
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly)) {
        QTextStream ts(&resolv);
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.startsWith("nameserver ")) {
                QString ns = line.mid(11);
                dnsRows.append({QStringLiteral("resolv.conf"), ns});
                if (!dnsList.contains(ns)) dnsList.append(ns);
            }
            else if (line.startsWith("search "))
                dnsRows.append({QStringLiteral("search domains"), line.mid(7)});
        }
    }
    // Also check systemd-resolved stub
    QFile stub(QStringLiteral("/run/systemd/resolve/resolv.conf"));
    if (stub.open(QIODevice::ReadOnly)) {
        dnsRows.append({QStringLiteral("systemd-resolved"), QStringLiteral("(stub resolver active)")});
    }
#endif
#endif

    if (!dnsRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kDnsCols, dnsRows));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = dnsList.isEmpty() ? QStringLiteral("No DNS servers found")
                 : QStringLiteral("DNS: %1").arg(dnsList.join(QStringLiteral(", ")));
    r.durationMs = 0;
    return r;
}

DiagnosticResult dnsCache(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    int cacheEntries = 0;
    bool hasCache = false;

    out.append(QString());

#ifdef _WIN32
    out.append(QStringLiteral("Windows IP Configuration"));
    out.append(QString());
    out.append(QStringLiteral("DNS Client Cache (ipconfig /displaydns format)"));
    out.append(QStringLiteral("=============================================="));
    out.append(QString());
    out.append(QStringLiteral("(Use 'ipconfig /displaydns' for full cache contents)"));
    out.append(QStringLiteral("To flush: ipconfig /flushdns"));
#else
    out.append(QStringLiteral("DNS Cache Information"));
    out.append(QString());
    // 闁冲厜鍋撻柍鍏夊亾 Try systemd-resolved cache (most common on modern Linux) 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾
    QFile cache(QStringLiteral("/run/systemd/resolve/cache"));
    if (cache.open(QIODevice::ReadOnly)) {
        QByteArray data = cache.readAll();
        hasCache = true;
        out.append(QStringLiteral("systemd-resolved DNS Cache"));
        out.append(QStringLiteral("=============================================="));
        out.append(QString());
        if (data.size() > 0) {
            // Parse cache entries: each entry is separated by blank line
            // Format: "example.com IN A 93.184.216.34" or similar
            QString text = QString::fromLatin1(data);
            QStringList entries = text.split('\n');
            for (const auto& line : entries) {
                QString trimmed = line.trimmed();
                if (trimmed.isEmpty()) {
                    out.append(QString());
                    continue;
                }
                // Parse: "hostname IN TYPE value" or "hostname IN TYPE ttl value"
                QStringList parts = trimmed.split(' ');
                if (parts.size() >= 4 && parts[1] == "IN") {
                    QString name = parts[0];
                    QString type = parts[2];
                    // Skip "IN" marker, extract TTL if present
                    QString dataPart;
                    int ttl = 0;
                    bool ok = false;
                    if (parts.size() >= 5) {
                        int val = parts[3].toInt(&ok);
                        if (ok && val > 0 && parts.size() >= 6) {
                            ttl = val;
                            dataPart = parts.mid(4).join(' ');
                        } else {
                            dataPart = parts.mid(3).join(' ');
                        }
                    }
                    // Show in ipconfig /displaydns style
                    cacheEntries++;
                    out.append(QStringLiteral("    %1").arg(name));
                    out.append(QStringLiteral("    ----------------------------------------"));
                    out.append(QStringLiteral("    Record Name . . . . . : %1").arg(name));
                    out.append(QStringLiteral("    Record Type . . . . . : %1").arg(type));
                    if (ttl > 0)
                        out.append(QStringLiteral("    Time To Live  . . . . : %1").arg(ttl));
                    out.append(QStringLiteral("    Data . . . . . . . . : %1").arg(dataPart));
                } else {
                    // Unparsed line 闁?show as-is
                    out.append(QStringLiteral("    %1").arg(trimmed));
                }
            }
        } else {
            out.append(QStringLiteral("    (cache is empty)"));
        }
    } else {
        // 闁冲厜鍋撻柍鍏夊亾 No systemd-resolved 闁?check and show resolution setup 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
        out.append(QStringLiteral("DNS Resolution Configuration"));
        out.append(QStringLiteral("=============================================="));
        out.append(QString());

        // Check for nscd
        if (QFile::exists(QStringLiteral("/var/db/nscd/hosts")))
            out.append(QStringLiteral("    nscd: active (hosts cache at /var/db/nscd/hosts)"));
        else if (QFile::exists(QStringLiteral("/var/cache/nscd/hosts")))
            out.append(QStringLiteral("    nscd: active (hosts cache at /var/cache/nscd/hosts)"));

        // Check for dnsmasq
        if (QFile::exists(QStringLiteral("/var/lib/misc/dnsmasq.leases")))
            out.append(QStringLiteral("    dnsmasq: active (leases at /var/lib/misc/dnsmasq.leases)"));

        // Show resolv.conf as the "current resolver" info
        QFile resolv(QStringLiteral("/etc/resolv.conf"));
        if (resolv.open(QIODevice::ReadOnly)) {
            QTextStream ts(&resolv);
            while (!ts.atEnd()) {
                QString line = ts.readLine().trimmed();
                if (line.isEmpty() || line.startsWith('#')) continue;
                if (line.startsWith("nameserver "))
                    out.append(QStringLiteral("    Nameserver . . . . . . . . : %1").arg(line.mid(11)));
                else if (line.startsWith("search "))
                    out.append(QStringLiteral("    DNS Suffix Search List. . : %1").arg(line.mid(7)));
                else if (line.startsWith("domain "))
                    out.append(QStringLiteral("    Connection-specific DNS . . : %1").arg(line.mid(7)));
                else if (line.startsWith("options "))
                    out.append(QStringLiteral("    Options . . . . . . . . . : %1").arg(line.mid(8)));
            }
        }

        // Show hosts file summary
        QFile hosts(QStringLiteral("/etc/hosts"));
        int hostEntryCount = 0;
        if (hosts.open(QIODevice::ReadOnly)) {
            QTextStream ts(&hosts);
            while (!ts.atEnd()) {
                QString line = ts.readLine().trimmed();
                if (!line.isEmpty() && !line.startsWith('#') && line.contains(' '))
                    hostEntryCount++;
            }
        }
        if (hostEntryCount > 0)
            out.append(QStringLiteral("    /etc/hosts entries . . . . : %1 static mappings").arg(hostEntryCount));
    }
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = hasCache ? DiagStatus::Pass : DiagStatus::Info;
    if (hasCache)
        r.summary = QStringLiteral("Cache active — %1 cached DNS entries").arg(cacheEntries);
    else
        r.summary = QStringLiteral("No local DNS cache detected");
    r.durationMs = (int)t.elapsed();
    return r;
}

DiagnosticResult dnsPollution(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("DNS Pollution / Hijacking Check"));
    out.append(QStringLiteral("================================="));
    out.append(QString());
    out.append(QStringLiteral("Tests whether non-existent domains resolve to IP addresses."));
    out.append(QStringLiteral("If they do, your DNS provider is redirecting NXDOMAIN responses"));
    out.append(QStringLiteral("(DNS hijacking / DNS pollution). A clean resolver returns NXDOMAIN."));
    out.append(QString());

    // Show current DNS server
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly)) {
        QTextStream ts(&resolv);
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.startsWith("nameserver ")) {
                out.append(QStringLiteral("DNS Server: %1").arg(line.mid(11)));
                break;
            }
        }
    }
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QStringLiteral("Test Domain"), -40)
        .arg(QStringLiteral("Result"), -16)
        .arg(QStringLiteral("Response")));
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QString(40, '-'))
        .arg(QString(16, '-'))
        .arg(QString(20, '-')));

    struct { const char* domain; } testCases[] = {
        {"thisdomainshouldnotexist12345.com"},
        {"nonexistent-test-domain-98765.org"},
        {"definitely-not-real-domain-42.net"},
    };

    int resolved = 0, clean = 0, timedOut = 0;
    QStringList hijackIPs;
    for (auto& tc : testCases) {
        QElapsedTimer probe; probe.start();
        QString ip = DnsResolver::instance().resolve(tc.domain, 4000);
        int elapsed = static_cast<int>(probe.elapsed());
        if (!ip.isEmpty()) {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(tc.domain, -40).arg(QStringLiteral("RESOLVED"), -16).arg(QStringLiteral("%1 (%2 ms)").arg(ip).arg(elapsed)));
            resolved++;
            if (!hijackIPs.contains(ip)) hijackIPs.append(ip);
        } else if (elapsed >= 4000) {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(tc.domain, -40).arg(QStringLiteral("TIMEOUT"), -16).arg(QStringLiteral("%1 ms").arg(elapsed)));
            timedOut++;
        } else {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(tc.domain, -40).arg(QStringLiteral("NXDOMAIN"), -16).arg(QStringLiteral("%1 ms").arg(elapsed)));
            clean++;
        }
    }

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("Results: %1 resolved, %2 clean, %3 timed out")
        .arg(resolved).arg(clean).arg(timedOut));
    if (resolved > 0) {
        out.append(QStringLiteral("Verdict: DNS HIJACKING DETECTED — non-existent domains redirected to:"));
        for (const auto& ip : hijackIPs) out.append(QStringLiteral("  • %1").arg(ip));
        out.append(QString());
        out.append(QStringLiteral("This typically means your ISP or DNS provider is intercepting"));
        out.append(QStringLiteral("NXDOMAIN responses and redirecting to a search/advertising page."));
    } else if (timedOut > 0) {
        out.append(QStringLiteral("Verdict: INCONCLUSIVE — %1 probes timed out (DNS may be slow or filtered)").arg(timedOut));
    } else {
        out.append(QStringLiteral("Verdict: DNS CLEAN — no hijacking detected"));
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = resolved > 0 ? DiagStatus::Warning : (timedOut > 0 ? DiagStatus::Info : DiagStatus::Pass);
    r.summary = resolved > 0 ? QStringLiteral("DNS hijack: %1 IPs").arg(hijackIPs.size())
               : timedOut > 0 ? QStringLiteral("DNS: %1 timeout(s)").arg(timedOut)
               : QStringLiteral("DNS clean");
    r.durationMs = t.elapsed();
    return r;
}

// internetConnectivity() removed 闁?merged into speedTest() (Phase 0 connectivity check)

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// Speed Test 闁?Speedtest.net protocol (Ookla-compatible)
// Mimics "speedtest-cli" output format
// =============================================================================
// Protocol:
//   1. GET /api/js/servers 闁?JSON server list
//   2. TCP ping each candidate 闁?pick lowest latency
//   3. GET {url}/download?size=N 闁?measure download throughput
//   4. POST {url}/upload 闁?measure upload throughput
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// Convert host (name or IP) to sockaddr_in 闁?returns true on success

// 闁冲厜鍋撻柍鍏夊亾 Simple HTTP GET via raw socket (no Qt event loop needed) 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾
static QByteArray httpGet(const QString& host, int port, const QString& path, int timeoutMs, int maxBytes) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return {};
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return {}; }

#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return {}; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
    if (err != 0) { closeSocket(sock); return {}; }

    // Send HTTP request (loop handles partial sends, EAGAIN-safe)
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n")
        .arg(path, host).toUtf8();
    int sent = 0;
    while (sent < req.size()) {
        auto n = ::send(sock, req.constData() + sent, req.size() - sent, 0);
        if (n < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#endif
            break;
        }
        if (n == 0) break;
        sent += n;
    }

    // Read response with wall-clock timeout
    QByteArray response; char buf[8192];
    QElapsedTimer recvTimer; recvTimer.start();
    while (response.size() < maxBytes) {
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
        if (select(sock + 1, &fdset, nullptr, nullptr, &tv) <= 0) break;
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        response.append(buf, (int)n);
        // Wall-clock guard: abort if total recv time exceeds 30 s
        if (recvTimer.elapsed() > 30000) break;
    }
    closeSocket(sock);
    return response;
}

// 闁冲厜鍋撻柍鍏夊亾 HTTP download with throughput measurement 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
struct SpeedResult { double mbps; int bytes; int durationMs; bool ok; };
static SpeedResult httpDownload(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false};
    // Parse URL 闁?host, port, path
    QString u = urlStr;
    if (!u.startsWith("http://")) return r;
    u = u.mid(7); // strip "http://"
    auto slash = u.indexOf('/');
    QString hostPort = (slash > 0) ? u.left(slash) : u;
    QString path = (slash > 0) ? u.mid(slash) : "/";

    QString host = hostPort; int port = 80;
    auto colon = hostPort.lastIndexOf(':');
    if (colon > 0) { host = hostPort.left(colon); port = hostPort.mid(colon + 1).toInt(); }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return r;
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return r; }

#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    QElapsedTimer t; t.start();
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {3, 0};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return r; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
    if (err != 0) { closeSocket(sock); return r; }

    // Send HTTP GET (loop handles partial sends, EAGAIN-safe)
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, host).toUtf8();
    int reqSent = 0;
    while (reqSent < req.size()) {
        auto n = ::send(sock, req.constData() + reqSent, req.size() - reqSent, 0);
        if (n < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#endif
            break;
        }
        if (n == 0) break;
        reqSent += n;
    }

    // Read with timing 闁?measure throughput (wall-clock guarded)
    qint64 startNs = t.nsecsElapsed();
    QByteArray body;
    bool headersDone = false;
    char buf[32768];
    QElapsedTimer recvGuard; recvGuard.start();
    while (body.size() < targetBytes + 65536) {
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
        if (select(sock + 1, &fdset, nullptr, nullptr, &tv) <= 0) break;
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        // Wall-clock guard: abort if total recv time exceeds 60 s
        if (recvGuard.elapsed() > 60000) break;
        if (!headersDone) {
            body.append(buf, (int)n);
            auto hdrEnd = body.indexOf("\r\n\r\n");
            if (hdrEnd >= 0) {
                body = body.mid(hdrEnd + 4);
                headersDone = true;
                startNs = t.nsecsElapsed(); // reset timer to body start
            }
        } else {
            body.append(buf, (int)n);
        }
    }
    closeSocket(sock);

    qint64 elapsedNs = t.nsecsElapsed() - startNs;
    if (elapsedNs <= 0) elapsedNs = 1;
    r.bytes = static_cast<int>(body.size());
    r.durationMs = (int)(elapsedNs / 1000000);
    if (r.bytes > 0 && r.durationMs > 0) {
        double bits = r.bytes * 8.0;
        double secs = r.durationMs / 1000.0;
        r.mbps = bits / secs / 1000000.0;
        r.ok = true;
    }
    return r;
}

// 闁冲厜鍋撻柍鍏夊亾 TCP ping (simple connect RTT) 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
static int tcpPingMs(const QString& host, int port) {
    QElapsedTimer t; t.start();
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return -1; }
#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {2, 0};
    int sel = select(sock + 1, nullptr, &fdset, nullptr, &tv);
    int ms = static_cast<int>(t.elapsed());
    if (sel > 0) { int err = 0; socklen_t len = sizeof(err); getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len); if (err != 0) ms = -1; } else ms = -1;
    closeSocket(sock);
    return ms;
}

// 闁冲厜鍋撻柍鍏夊亾 HTTP latency via tiny file download (speedtest-cli style latency.txt) 闁冲厜鍋撻柍鍏夊亾
// Measures real application-layer RTT: DNS + TCP connect + HTTP request/response
// Much better predictor of download throughput than raw TCP ping.
static int httpLatencyMs(const QString& urlStr, int timeoutMs) {
    QElapsedTimer t; t.start();
    QString u = urlStr;
    if (!u.startsWith("http://")) return -1;
    u = u.mid(7);
    auto slash = u.indexOf('/');
    QString hostPort = (slash > 0) ? u.left(slash) : u;
    QString host = hostPort; int port = 80;
    auto colon = hostPort.lastIndexOf(':');
    if (colon > 0) { host = hostPort.left(colon); port = hostPort.mid(colon + 1).toInt(); }

    // Download latency.txt from server root 闁?speedtest-cli uses the root path
    // regardless of the download/upload URL structure
    QString latPath = QStringLiteral("/latency.txt");
    QByteArray resp = httpGet(host, port, latPath, timeoutMs, 256);
    if (resp.isEmpty()) return -1;

    // Parse HTTP response 闁?extract body after \r\n\r\n header terminator
    auto hdrEnd = resp.indexOf("\r\n\r\n");
    if (hdrEnd < 0) return -1;
    QByteArray body = resp.mid(hdrEnd + 4);
    // latency.txt should contain a small text like "test=...", we just need the time
    if (body.trimmed().isEmpty()) return -1;

    return static_cast<int>(t.elapsed());
}

DiagnosticResult speedTest(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer totalTimer; totalTimer.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Internet Connectivity"));
    out.append(QStringLiteral("Protocol: Speedtest.net (Ookla-compatible)"));
    out.append(QString());

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Phase 0 闁?Quick connectivity check (TCP to well-known hosts)
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    out.append(QStringLiteral("--- Connectivity Check -------------------------------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3  %4  %5")
        .arg(QStringLiteral("Host").leftJustified(16, ' '))
        .arg(QStringLiteral("Address").leftJustified(15, ' '))
        .arg(QStringLiteral("Port").rightJustified(5, ' '))
        .arg(QStringLiteral("Status").leftJustified(6, ' '))
        .arg(QStringLiteral("Latency").rightJustified(7, ' ')));
    out.append(QStringLiteral("  %1  %2  %3  %4  %5")
        .arg(QString(16, QChar('-')))
        .arg(QString(15, QChar('-')))
        .arg(QString(5, QChar('-')))
        .arg(QString(6, QChar('-')))
        .arg(QString(7, QChar('-'))));

    struct { const char* host; int port; const char* name; } checkSites[] = {
        {"223.5.5.5", 53, "Alibaba DNS"},
        {"119.29.29.29", 53, "DNSPod DNS"},
        {"baidu.com", 443, "Baidu"},
    };
    int connOk = 0;
    for (auto& cs : checkSites) {
        int p = tcpPingMs(cs.host, cs.port);
        QString status, latency;
        if (p >= 0) { status = QStringLiteral("[OK]"); latency = QStringLiteral("%1 ms").arg(p); connOk++; }
        else        { status = QStringLiteral("[FAIL]"); latency = QStringLiteral("-"); }
        out.append(QStringLiteral("  %1  %2  %3  %4  %5")
            .arg(QString::fromUtf8(cs.name).leftJustified(16, ' '))
            .arg(QString::fromUtf8(cs.host).leftJustified(15, ' '))
            .arg(cs.port, 5)
            .arg(status.leftJustified(6, ' '))
            .arg(latency.rightJustified(7, ' ')));
    }
    bool hasConnectivity = (connOk > 0);
    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Result: %1").arg(hasConnectivity
        ? QStringLiteral("CONNECTED (%1/%2)").arg(connOk).arg((int)(sizeof(checkSites)/sizeof(checkSites[0])))
        : QStringLiteral("DISCONNECTED")));
    out.append(QString());

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Phase 1 闁?Detect country + load regional servers
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    SpeedTest st;
    QString country = SpeedTest::detectCountry(3000);
    out.append(QStringLiteral("Detected country: %1").arg(country == "XX" ? "Unknown" : country));

    QVector<SpeedTest::Server> servers = st.serversForCountry(country);
    out.append(QStringLiteral("Loaded %1 servers for region").arg(servers.size()));

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Timeout guard 闁?if we've already spent >25s, skip speed measurement
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    if (totalTimer.elapsed() > 25000) {
        out.append(QString());
        out.append(QStringLiteral("  (Speed test skipped: connectivity check took too long)"));
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = hasConnectivity ? QStringLiteral("Connected — speed test timed out") : QStringLiteral("No internet");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Phase 2 闁?Select best server by HTTP latency (speedtest-cli style)
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    out.append(QStringLiteral("--- Server Selection (HTTP latency) -----------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QStringLiteral("#").rightJustified(3, ' '))
        .arg(QStringLiteral("Sponsor").leftJustified(22, ' '))
        .arg(QStringLiteral("Server").leftJustified(17, ' '))
        .arg(QStringLiteral("Latency").rightJustified(7, ' ')));
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QString(3, QChar('-')))
        .arg(QString(22, QChar('-')))
        .arg(QString(17, QChar('-')))
        .arg(QString(7, QChar('-'))));

    struct RankedServer { SpeedTest::Server* srv; int latency; };
    QVector<RankedServer> ranked;
    int maxServers = qMin(8, (int)servers.size()); // cap at 8 to avoid excessive time
    for (auto& s : servers) {
        if (ranked.size() >= maxServers) break;
        if (totalTimer.elapsed() > 25000) break; // global timeout
        int lat = httpLatencyMs(s.url, 5000);
        if (lat > 0) {
            ranked.append({&s, lat});
        }
    }

    if (ranked.isEmpty()) {
        out.append(QStringLiteral("  (no reachable servers)"));
        out.append(QString());
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = hasConnectivity ? QStringLiteral("Connected — no speed test servers reachable")
                                    : QStringLiteral("No internet connectivity");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    // Sort by HTTP latency ascending 闁?fastest first
    std::sort(ranked.begin(), ranked.end(),
              [](const RankedServer& a, const RankedServer& b) { return a.latency < b.latency; });

    for (int i = 0; i < ranked.size(); i++) {
        auto& rs = ranked[i];
        out.append(QStringLiteral("  %1  %2  %3  %4")
            .arg(i + 1, 3)
            .arg(rs.srv->sponsor.leftJustified(22, ' '))
            .arg(rs.srv->name.leftJustified(17, ' '))
            .arg(QStringLiteral("%1 ms").arg(rs.latency).rightJustified(7, ' ')));
    }

    SpeedTest::Server* best = ranked[0].srv;
    int bestLatency = ranked[0].latency;

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Selected: %1 (%2) — %3 ms")
        .arg(best->sponsor, best->name).arg(bestLatency));
    out.append(QString());

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Phase 3 闁?Download test (with server fallback)
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    out.append(QString());
    out.append(QStringLiteral("--- Download Test ------------------------------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  Server: %1").arg(best->host));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QStringLiteral("Size").rightJustified(10, ' '))
        .arg(QStringLiteral("Throughput").leftJustified(16, ' '))
        .arg(QStringLiteral("Time").rightJustified(6, ' ')));
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QString(10, QChar('-')))
        .arg(QString(16, QChar('-')))
        .arg(QString(6, QChar('-'))));

    // Progressive download sizes (KB): start small, ramp up
    int dlSizes[] = {250, 500, 750, 1000, 1500, 2000, 2500, 3000, 4000, 5000, 7500, 10000, 15000, 20000, 25000};
    QVector<double> dlResults;
    int dlTotalBytes = 0, dlTotalMs = 0;
    int dlServerIdx = 0; // current preferred server index into ranked[]

    for (int sizeKb : dlSizes) {
        if (dlTotalMs > 12000) break; // cap at ~12 seconds

        // Try preferred server first, fall back through ranked list independently
        // per size tier 闁?a server that handles 250KB may choke on 25MB.
        bool ok = false;
        for (int si = 0; si < ranked.size(); si++) {
            // Try each server once before marking failure
            int idx = (dlServerIdx + si) % ranked.size();
            SpeedTest::Server* srv = ranked[idx].srv;
            QString dlUrl = QStringLiteral("%1/download?size=%2").arg(srv->url).arg(sizeKb * 1000);
            auto res = httpDownload(dlUrl, sizeKb * 1000, 8000);
            if (res.ok && res.mbps > 0.01) {
                dlResults.append(res.mbps);
                dlTotalBytes += res.bytes;
                dlTotalMs += res.durationMs;
                if (idx != dlServerIdx) {
                    out.append(QStringLiteral("  (switched to %1)").arg(srv->sponsor));
                    best = srv; bestLatency = ranked[idx].latency;
                    dlServerIdx = idx; // make this the new preferred server
                }
                out.append(QStringLiteral("  %1  %2  %3")
                    .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                    .arg(QStringLiteral("%1 Mbit/s").arg(res.mbps, 0, 'f', 2).leftJustified(16, ' '))
                    .arg(QStringLiteral("%1 ms").arg(res.durationMs).rightJustified(6, ' ')));
                ok = true;
                break;
            }
        }
        if (!ok) {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                .arg(QStringLiteral("(timeout)").leftJustified(16, ' '))
                .arg(QStringLiteral("-").rightJustified(6, ' ')));
            // Don't abort 闁?try next size tier even if this one failed
        }
    }

    auto avgTopN = [](QVector<double>& v, int n = 5) -> double {
        if (v.isEmpty()) return 0;
        std::sort(v.begin(), v.end());
        int count = qMin(n, v.size());
        double sum = 0;
        for (auto i = v.size() - count; i < v.size(); i++) sum += v[i];
        return sum / count;
    };
    double dlSpeed = avgTopN(dlResults);

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Download: %1 Mbit/s%2")
        .arg(dlSpeed, 0, 'f', 2)
        .arg(dlResults.size() >= 5 ? QStringLiteral("  (avg of top %1)").arg(qMin(5, (int)dlResults.size())) : QString()));

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Phase 4 闁?Upload test
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    out.append(QString());
    out.append(QStringLiteral("--- Upload Test --------------------------------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QStringLiteral("Size").rightJustified(10, ' '))
        .arg(QStringLiteral("Throughput").leftJustified(16, ' '))
        .arg(QStringLiteral("Time").rightJustified(6, ' ')));
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QString(10, QChar('-')))
        .arg(QString(16, QChar('-')))
        .arg(QString(6, QChar('-'))));

    // Upload test: POST random data, increasing sizes
    int ulSizes[] = {100, 250, 500, 750, 1000, 1500, 2000, 2500, 3000, 4000};
    QVector<double> ulResults;
    int ulTotalMs = 0;

    for (int sizeKb : ulSizes) {
        if (ulTotalMs > 12000) break;
        int dataSize = sizeKb * 1000;

        // HTTP POST with measured upload
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        struct sockaddr_in addr;
        if (!hostToAddr(best->host, best->port, addr)) { closeSocket(sock); continue; }

#ifdef _WIN32
        u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
        ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
        struct timeval tv = {3, 0};
        if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); continue; }
        int err = 0; socklen_t len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        if (err != 0) { closeSocket(sock); continue; }

        // Generate random data
        QByteArray uploadData(dataSize, 'A');
        for (int i = 0; i < qMin(dataSize, 4096); i++) uploadData[i] = (char)('A' + (rand() % 26));

        // POST request headers
        QByteArray postHeaders = QStringLiteral("POST /upload HTTP/1.0\r\nHost: %1\r\nContent-Type: application/octet-stream\r\nContent-Length: %2\r\nConnection: close\r\n\r\n")
            .arg(best->host).arg(dataSize).toUtf8();

        QElapsedTimer ulTimer; ulTimer.start();
        // Send POST headers (EAGAIN-safe: select() for writability on stall)
        int hdrSent = 0;
        while (hdrSent < postHeaders.size()) {
            auto n = ::send(sock, postHeaders.constData() + hdrSent, postHeaders.size() - hdrSent, 0);
            if (n < 0) {
#ifdef _WIN32
                if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval hdrTv={1,0}; select(sock+1,nullptr,&wf,nullptr,&hdrTv); continue; }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval hdrTv={1,0}; select(sock+1,nullptr,&wf,nullptr,&hdrTv); continue; }
#endif
                break;
            }
            if (n == 0) break;
            hdrSent += n;
        }
        // Send body in chunks (EAGAIN-safe: select() for writability on stall)
        // Includes wall-clock guard so outer ulTotalMs check can fire
        int sent = 0; const char* dp = uploadData.constData();
        QElapsedTimer sendGuard; sendGuard.start();
        while (sent < dataSize) {
            int chunk = qMin(dataSize - sent, 32768);
            auto n = ::send(sock, dp + sent, chunk, 0);
            if (n < 0) {
#ifdef _WIN32
                if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval tv2={2,0}; select(sock+1,nullptr,&wf,nullptr,&tv2); if (sendGuard.elapsed() > 10000) break; continue; }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval tv2={2,0}; select(sock+1,nullptr,&wf,nullptr,&tv2); if (sendGuard.elapsed() > 10000) break; continue; }
#endif
                break;
            }
            if (n == 0) break;
            sent += n;
        }
        // Read response with proper error checking
        char buf[4096];
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {5, 0};
        int selRet = select(sock + 1, &fdset, nullptr, nullptr, &tv);
        if (selRet > 0 && FD_ISSET(sock, &fdset)) {
            recv(sock, buf, sizeof(buf), 0);
        }
        int ulMs = static_cast<int>(ulTimer.elapsed());
        closeSocket(sock);

        ulTotalMs += ulMs;
        double mbps = (sent > 0 && ulMs > 0) ? (sent * 8.0 / (ulMs / 1000.0) / 1000000.0) : 0;
        if (mbps > 0.01) {
            ulResults.append(mbps);
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                .arg(QStringLiteral("%1 Mbit/s").arg(mbps, 0, 'f', 2).leftJustified(16, ' '))
                .arg(QStringLiteral("%1 ms").arg(ulMs).rightJustified(6, ' ')));
        } else {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                .arg(QStringLiteral("(timeout)").leftJustified(16, ' '))
                .arg(QStringLiteral("-").rightJustified(6, ' ')));
        }
    }

    double ulSpeed = avgTopN(ulResults);

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Upload: %1 Mbit/s%2")
        .arg(ulSpeed, 0, 'f', 2)
        .arg(ulResults.size() >= 5 ? QStringLiteral("  (avg of top %1)").arg(qMin(5, (int)ulResults.size())) : QString()));

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Results
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    out.append(QString());
    out.append(QString());
    out.append(QStringLiteral("=================================================================="));
    out.append(QStringLiteral("                    Speed Test Results"));
    out.append(QStringLiteral("=================================================================="));
    out.append(QString());
    out.append(QStringLiteral("  Server:      %1 (%2, %3)").arg(best->sponsor, best->name, best->country));
    out.append(QStringLiteral("  Latency:     %1 ms").arg(bestLatency));
    out.append(QStringLiteral("  Download:    %1 Mbit/s").arg(dlSpeed, 0, 'f', 2));
    out.append(QStringLiteral("  Upload:      %1 Mbit/s").arg(ulSpeed, 0, 'f', 2));
    out.append(QStringLiteral("  Duration:    %1 s").arg(totalTimer.elapsed() / 1000.0, 0, 'f', 1));
    out.append(QString());
    out.append(QStringLiteral("=================================================================="));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    if (!hasConnectivity) {
        r.status = DiagStatus::Fail;
        r.summary = QStringLiteral("No internet connectivity");
    } else if (dlSpeed > 0.1 || ulSpeed > 0.1) {
        r.status = DiagStatus::Pass;
        r.summary = QStringLiteral("Connected — ↓%1 ↑%2 Mbit/s").arg(dlSpeed, 0, 'f', 1).arg(ulSpeed, 0, 'f', 1);
    } else {
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("Connected — speed test incomplete");
    }
    r.durationMs = totalTimer.elapsed();
    return r;
}

} // namespace G1G2G3Native
