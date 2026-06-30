// =============================================================================
// DiagnosticFormatter.h — MVC View-layer output formatting (cross-platform)
//
// Standardizes all diagnostic output to match Windows CLI tool formats:
//   ipconfig /all, route print, arp -a, netstat -an, nslookup, ping, tracert
//
// Replaces file-local tblFmt() in G1G2G3Native.cpp and manual QString::arg()
// formatting scattered across G4RemoteHost, G5WebsiteUrl, IosDnsTask, etc.
// =============================================================================
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QList>

class DiagnosticFormatter {
public:
    // ── Aligned-column table ──────────────────────────────────
    struct ColSpec { const char* header; int minWidth; bool rightAlign; };
    static QStringList formatTable(const QVector<ColSpec>& cols,
                                    const QList<QStringList>& rows);

    // ── ipconfig /all dotted key:value ───────────────────────
    static QString formatIpconfigLine(const QString& label, const QString& value,
                                       int indentSpaces = 3);

    // ── dig-style DNS ────────────────────────────────────────
    static QStringList formatDnsHeader(const QString& host, const QString& rcode,
                                        uint16_t id, int anCount);
    static QString formatDnsQuestion(const QString& host, const QString& type = "A");
    static QString formatDnsRecord(const QString& owner, int ttl,
                                    const QString& type, const QString& value);
    static QStringList formatDnsFooter(qint64 elapsedMs, const QString& server);

    // ── Windows ping.exe ─────────────────────────────────────
    static QString formatPingReply(const QString& ip, int ms, int bytes = 32, int ttl = 128);
    static QString formatPingTimeout();
    static QStringList formatPingStats(const QString& target, int sent, int received,
                                        double lossPct, int minMs, int maxMs, double avgMs);

    // ── Windows tracert.exe ──────────────────────────────────
    static QString formatTracerouteHop(int ttl, int rtt1, int rtt2, int rtt3,
                                        const QString& name, const QString& ip);

    // ── Simple label:value ──────────────────────────────────
    static QString formatProperty(const QString& label, const QString& value,
                                   int indentSpaces = 2);
    static QString formatHeader(const QString& title);
    static QString separatorLine(int width = 75, QChar ch = '=');
};
