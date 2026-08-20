// =============================================================================
// DiagnosticFormatter.h — MVC View-layer output formatting (cross-platform)
//
// Standardizes all diagnostic output to match Windows CLI tool formats:
//   ipconfig /all, route print, arp -a, netstat -an, nslookup, ping, tracert
//
// Replaces file-local tblFmt() in SystemDiagnostics.cpp and manual QString::arg()
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

    // ── Display-width helpers (CJK-aware) ─────────────────────
    // Character count != display width for CJK/fullwidth/emoji glyphs.
    // Shared with G4PathPing's manual table so both stay aligned.
    static int displayWidth(const QString& s);
    static QString padToWidth(const QString& val, int targetDisplayWidth, bool rightAlign);

    // ── dig-style DNS ────────────────────────────────────────
    // flags 行与各 SECTION 计数按响应头真实值呈现（dig 1:1）：
    // 5WHY (复核 2026-08-20 去伪造默认): flags 默认参数曾硬编码 "qr rd ra"
    // ——不传 flags 的调用方（Android 无响应头数据）静默打印伪造标志位。
    // 默认改为空串，呈现为 "(unparsed)"（诚实标记）；msgSize≥0 时输出
    // MSG SIZE 行。
    static QStringList formatDnsHeader(const QString& host, const QString& rcode,
                                        uint16_t id, int anCount,
                                        const QString& flags = QString(),
                                        int nsCount = 0, int arCount = 0);
    static QString formatDnsQuestion(const QString& host, const QString& type = "A");
    static QString formatDnsRecord(const QString& owner, int ttl,
                                    const QString& type, const QString& value);
    static QStringList formatDnsFooter(qint64 elapsedMs, const QString& server,
                                       int msgSize = -1);

};
