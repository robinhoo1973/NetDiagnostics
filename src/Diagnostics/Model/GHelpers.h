// GHelpers.h — shared helpers for G1/G2/G3 per-function .cpp files.
#pragma once
#include "Diagnostics/Model/GBase.h"
#include "Diagnostics/View/DiagnosticFormatter.h"
#include "Common/Utils/Logger.h"
#include <cstring>

namespace G1G2G3Native {

// ── MAC address formatting ──────────────────────────────────────────
static QString macToStr(const unsigned char* mac) {
    return QStringLiteral("%1:%2:%3:%4:%5:%6")
        .arg(mac[0], 2, 16, QLatin1Char('0'))
        .arg(mac[1], 2, 16, QLatin1Char('0'))
        .arg(mac[2], 2, 16, QLatin1Char('0'))
        .arg(mac[3], 2, 16, QLatin1Char('0'))
        .arg(mac[4], 2, 16, QLatin1Char('0'))
        .arg(mac[5], 2, 16, QLatin1Char('0'));
}

// ── IPv4 formatting ─────────────────────────────────────────────────
static QString ip4ToStr(struct in_addr a) {
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &a, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}
static QString ipToStr(uint32_t ip) {
    struct in_addr a; a.s_addr = ip;
    return ip4ToStr(a);
}

// ── Cellular helpers ────────────────────────────────────────────────
static bool hasNonEmptyValue(const QVariantMap& values, const char* key) {
    auto it = values.constFind(QLatin1String(key));
    return it != values.cend() && !it->toString().trimmed().isEmpty();
}

static bool hasCellularIdentity(const QVariantMap& cell) {
    return hasNonEmptyValue(cell, "carrierName")
        || hasNonEmptyValue(cell, "radioAccess")
        || (hasNonEmptyValue(cell, "mcc") && hasNonEmptyValue(cell, "mnc"));
}

static QString cellularSummary(const QVariantMap& cell) {
    QString carrier = cell.value(QStringLiteral("carrierName")).toString().trimmed();
    QString radio = cell.value(QStringLiteral("radioAccess")).toString().trimmed();
    if (!carrier.isEmpty() && !radio.isEmpty())
        return QStringLiteral("Carrier: %1 (%2)").arg(carrier, radio);
    if (!carrier.isEmpty()) return QStringLiteral("Carrier: %1").arg(carrier);
    if (!radio.isEmpty()) return QStringLiteral("Radio: %1").arg(radio);
    return QStringLiteral("Cellular service detected");
}

// ── TCP state names ─────────────────────────────────────────────────
#if defined(_WIN32)
static const char* tcpStateName(int st) {
    switch(st){case 1:return"CLOSED";case 2:return"LISTEN";case 3:return"SYN_SENT";
    case 4:return"SYN_RCVD";case 5:return"ESTABLISHED";case 6:return"FIN_WAIT1";
    case 7:return"FIN_WAIT2";case 8:return"CLOSE_WAIT";case 9:return"CLOSING";
    case 10:return"LAST_ACK";case 11:return"TIME_WAIT";case 12:return"DELETE_TCB";
    default:return"UNKNOWN";}
}
#else
static const char* tcpStateName(int st) {
    switch(st){case 1:return"ESTABLISHED";case 2:return"SYN_SENT";case 3:return"SYN_RECV";
    case 4:return"FIN_WAIT1";case 5:return"FIN_WAIT2";case 6:return"TIME_WAIT";
    case 7:return"CLOSE";case 8:return"CLOSE_WAIT";case 9:return"LAST_ACK";
    case 10:return"LISTEN";case 11:return"CLOSING";default:return"UNKNOWN";}
}
#endif

#if !defined(_WIN32)
struct ProcNetConn {
    QString localIp; int localPort;
    QString remoteIp; int remotePort;
    int state; uint32_t uid; bool isIPv6;
};
#endif

// Forward declarations (defined in GCommon.cpp, non-static — shared across TUs)
int      tcpPingMs(const QString& host, int port);
double   tcpPingAvg(const QString& host, int port); // 50x avg for sub-ms differentiation
int      httpLatencyMs(const QString& urlStr, int timeoutMs);
struct SpeedResult { double mbps; int bytes; int durationMs; bool ok; };
SpeedResult httpDownload(const QString& urlStr, int targetBytes, int timeoutMs);
QByteArray httpGet(const QString& host, int port, const QString& path, int timeoutMs, int maxBytes);

} // namespace G1G2G3Native
