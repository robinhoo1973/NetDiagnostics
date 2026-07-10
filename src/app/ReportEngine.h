// =============================================================================
// ReportEngine.h — Standalone HTML/PDF report generation + file I/O.
// Extracted from AppState.cpp (~300 lines).
//
// All generation methods take a ReportData snapshot — zero coupling to
// AppState's mutable state.  File I/O methods are self-contained.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QMap>
#include <functional>
#include "models/DiagId.h"
#include "models/DiagnosticResult.h"

// Snapshot of all data needed to generate a report.  Caller builds this once.
struct ReportData {
    QString target;
    QString timestamp;
    QString appVersion;
    QString buildNumber;
    QStringList groupLabels;                         // 5 translated group names
    QMap<DiagId, DiagnosticResult> results;          // all completed results
    QMap<int, QVariantMap> groupStats;               // group index → {pass,warn,fail,skip,info,total}
    // Defaults to "Unknown" to prevent std::bad_function_call if unset.
    // Callers should override with a real display-name function (e.g. AppState::staticDiagDisplayName).
    std::function<QString(DiagId)> diagDisplayName = [](DiagId) { return QStringLiteral("Unknown"); };
    QMap<DiagGroup, QList<DiagId>> diagIdsInGroup;   // group → ordered diag IDs
};

class ReportEngine : public QObject {
    Q_OBJECT
public:
    explicit ReportEngine(QObject* parent = nullptr) : QObject(parent) {}

    // ── HTML generation ────────────────────────────────────────────
    // Qt Rich Text subset (for QML preview / QPdfWriter). fullDetail=false = summary.
    // darkBackground=true uses dark theme colors (QML preview); false = light (PDF printing).
    static QString buildHtml(const ReportData& data, bool fullDetail, bool darkBackground = false);

    // Full standalone HTML document (dark theme, collapsible, browser-grade).
    static QString buildRichDocument(const ReportData& data);

    // ── File I/O ──────────────────────────────────────────────────
    static QString exportHtml(const QString& filePath, const QString& html);
    static QString exportPdf(const QString& filePath, const QString& html);

    // ── Path helpers ───────────────────────────────────────────────
    static QString defaultReportPath(const QString& ext);

    // ── Desktop save dialog (QML-callable, emits savePathPicked) ──
    Q_INVOKABLE void requestSavePath(const QString& format);

    // ── Desktop email handoff ─────────────────────────────────────
    static void emailReportDesktop(const QString& path);

signals:
    void savePathPicked(const QString& format, const QString& path);

private:
    static QString buildHeaderBand(const ReportData& data);
    static QString buildSummaryCards(int pass, int warn, int fail, int skip, int info, int total);
    static QString buildGroupTable(int g, const ReportData& data);
    static QString buildDetailSection(const ReportData& data);
};
