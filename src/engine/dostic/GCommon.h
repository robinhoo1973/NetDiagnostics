// =============================================================================
// GCommon.h — shared helpers for G1/G2/G3 diagnostic implementations.
// Included by G1System.cpp, G2Connectivity.cpp, G1G2G3Native.cpp (G3).
// =============================================================================
#pragma once
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <cstring>

namespace G1G2G3Native {

// Forward declarations
static int       tcpPingMs(const QString& host, int port);
static QByteArray httpGet(const QString& host, int port, const QString& path,
                          int timeoutMs, int maxBytes);
struct SpeedResult; // defined in SpeedTest.h

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

// ── IP address formatting ───────────────────────────────────────────
#if defined(_WIN32) || defined(__linux__) || (defined(__APPLE__) && !defined(PLATFORM_IOS))
#include <winsock2.h> // for struct in_addr (Windows)
#include <ws2tcpip.h>
#endif

static QString ip4ToStr(struct in_addr a) {
    char buf[INET_ADDRSTRLEN];
    return QString::fromLatin1(inet_ntop(AF_INET, &a, buf, sizeof(buf)));
}
static QString ipToStr(uint32_t ip) {
    struct in_addr a; a.s_addr = htonl(ip); return ip4ToStr(a);
}

// ── Cellular helpers ────────────────────────────────────────────────
static bool hasNonEmptyValue(const QVariantMap& values, const char* key) {
    auto it = values.find(QLatin1String(key));
    return it != values.end() && !it.value().toString().isEmpty();
}
static bool hasCellularIdentity(const QVariantMap& cell) {
    return hasNonEmptyValue(cell, "carrier") || hasNonEmptyValue(cell, "radio")
        || hasNonEmptyValue(cell, "mcc") || hasNonEmptyValue(cell, "mnc");
}
static QString cellularSummary(const QVariantMap& cell) {
    if (hasCellularIdentity(cell)) {
        QString carrier = cell.value("carrier").toString();
        QString radio = cell.value("radio").toString();
        if (!carrier.isEmpty() && !radio.isEmpty())
            return QStringLiteral("%1 (%2)").arg(carrier, radio);
        if (!carrier.isEmpty()) return carrier;
        if (!radio.isEmpty()) return radio;
    }
    return QStringLiteral("Cellular service detected");
}

// ── TCP state names ─────────────────────────────────────────────────
#ifdef _WIN32
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

} // namespace G1G2G3Native
