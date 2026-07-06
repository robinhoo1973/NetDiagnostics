// =============================================================================
// ReportEngine.cpp — extracted from AppState.cpp (~300 lines)
// =============================================================================
#include "app/ReportEngine.h"
#include "util/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTextDocument>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QFont>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QProcess>

#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QFileDialog>
#endif

namespace {

QString normalizeReportPath(const QString& p) {
    return p.startsWith(QStringLiteral("file:")) ? QUrl(p).toLocalFile() : p;
}

QString reportStatusColor(DiagStatus s) {
    switch (s) {
        case DiagStatus::Pass:    return QStringLiteral("#10B981");
        case DiagStatus::Warning: return QStringLiteral("#F59E0B");
        case DiagStatus::Fail:    return QStringLiteral("#EF4444");
        case DiagStatus::Error:   return QStringLiteral("#EF4444");
        case DiagStatus::Skipped: return QStringLiteral("#9CA3AF");
        default:                  return QStringLiteral("#3B82F6");
    }
}

QString reportStatusText(DiagStatus s) {
    switch (s) {
        case DiagStatus::Pass:    return QStringLiteral("Pass");
        case DiagStatus::Warning: return QStringLiteral("Warning");
        case DiagStatus::Fail:    return QStringLiteral("Fail");
        case DiagStatus::Error:   return QStringLiteral("Error");
        case DiagStatus::Skipped: return QStringLiteral("Skipped");
        default:                  return QStringLiteral("Info");
    }
}

QString reportStatusClass(DiagStatus s) {
    switch (s) {
        case DiagStatus::Pass:    return QStringLiteral("pass");
        case DiagStatus::Warning: return QStringLiteral("warn");
        case DiagStatus::Fail:    return QStringLiteral("fail");
        case DiagStatus::Error:   return QStringLiteral("fail");
        case DiagStatus::Skipped: return QStringLiteral("skip");
        default:                  return QStringLiteral("info");
    }
}

} // namespace

// ── Public: HTML generation ─────────────────────────────────────────────

QString ReportEngine::buildHtml(const ReportData& data, bool fullDetail) {
    const QString colorPass = QStringLiteral("#10B981");
    const QString colorWarn = QStringLiteral("#F59E0B");
    const QString colorFail = QStringLiteral("#EF4444");
    const QString colorSkip = QStringLiteral("#9CA3AF");
    const QString colorInfo = QStringLiteral("#3B82F6");

    int tPass=0,tWarn=0,tFail=0,tSkip=0,tInfo=0,tTotal=0;
    for (int g = 0; g < 5; ++g) {
        auto it = data.groupStats.find(g);
        if (it == data.groupStats.end()) continue;
        tPass += it->value(QStringLiteral("pass")).toInt();
        tWarn += it->value(QStringLiteral("warn")).toInt();
        tFail += it->value(QStringLiteral("fail")).toInt();
        tSkip += it->value(QStringLiteral("skip")).toInt();
        tInfo += it->value(QStringLiteral("info")).toInt();
        tTotal += it->value(QStringLiteral("total")).toInt();
    }

    QString h;
    h += QStringLiteral("<div style=\"font-family:'Helvetica Neue',Arial,'PingFang SC','Microsoft YaHei',sans-serif;color:#0F172A\">");

    // Header band
    h += QStringLiteral(
        "<table width=\"100%\" cellpadding=\"16\" cellspacing=\"0\"><tr>"
        "<td bgcolor=\"#0F172A\">"
        "<font color=\"#FFFFFF\" size=\"6\"><b>Network Diagnostic Report</b></font><br/>"
        "<font color=\"#E2E8F0\" size=\"3\">%1</font><br/>"
        "<font color=\"#94A3B8\" size=\"2\">%2 &nbsp;&middot;&nbsp; v%3 (build %4)</font>"
        "</td></tr></table><br/>")
        .arg(data.target, data.timestamp, data.appVersion, data.buildNumber);

    // Summary cards
    auto card = [](const QString& bg, const QString& fg, int val, const QString& lbl) {
        return QStringLiteral(
            "<td width=\"20%\" align=\"center\" bgcolor=\"%1\">"
            "<font color=\"%2\" size=\"6\"><b>%3</b></font><br/>"
            "<font color=\"%2\" size=\"2\">%4</font></td>")
            .arg(bg, fg).arg(val).arg(lbl);
    };
    h += QStringLiteral("<table width=\"100%\" cellpadding=\"12\" cellspacing=\"6\"><tr>");
    h += card(QStringLiteral("#ECFDF5"), colorPass, tPass, QStringLiteral("Pass"));
    h += card(QStringLiteral("#FFFBEB"), colorWarn, tWarn, QStringLiteral("Warning"));
    h += card(QStringLiteral("#FEF2F2"), colorFail, tFail, QStringLiteral("Fail"));
    h += card(QStringLiteral("#F1F5F9"), colorSkip, tSkip, QStringLiteral("Skipped"));
    h += card(QStringLiteral("#EFF6FF"), colorInfo, tInfo, QStringLiteral("Info"));
    h += QStringLiteral("</tr></table>");
    h += QStringLiteral("<p align=\"center\"><font color=\"#64748B\" size=\"2\">%1 tests total</font></p><br/>")
        .arg(tTotal);

    // Per-group results
    for (int g = 0; g < 5; ++g) {
        auto it = data.groupStats.find(g);
        if (it == data.groupStats.end() || it->value(QStringLiteral("total")).toInt() == 0) continue;
        const auto& s = *it;
        const QString glabel = g < data.groupLabels.size() ? data.groupLabels[g].toHtmlEscaped() : QString();
        h += QStringLiteral(
            "<table width=\"100%\" cellpadding=\"9\" cellspacing=\"0\"><tr>"
            "<td bgcolor=\"#1E293B\">"
            "<font color=\"#FFFFFF\" size=\"3\"><b>G%1 &middot; %2</b></font>"
            "&nbsp;&nbsp;<font color=\"%3\" size=\"2\">%4 Pass</font>"
            "<font color=\"#64748B\" size=\"2\"> &middot; </font><font color=\"%5\" size=\"2\">%6 Warn</font>"
            "<font color=\"#64748B\" size=\"2\"> &middot; </font><font color=\"%7\" size=\"2\">%8 Fail</font>"
            "<font color=\"#64748B\" size=\"2\"> &middot; </font><font color=\"%9\" size=\"2\">%10 Skip</font>"
            "</td></tr></table>")
            .arg(g+1).arg(glabel)
            .arg(colorPass).arg(s.value(QStringLiteral("pass")).toInt())
            .arg(colorWarn).arg(s.value(QStringLiteral("warn")).toInt())
            .arg(colorFail).arg(s.value(QStringLiteral("fail")).toInt())
            .arg(colorSkip).arg(s.value(QStringLiteral("skip")).toInt());
        h += QStringLiteral(
            "<table width=\"100%\" border=\"1\" cellpadding=\"9\" cellspacing=\"0\" bordercolor=\"#E2E8F0\">"
            "<tr bgcolor=\"#F8FAFC\">"
            "<th align=\"left\" width=\"40%\"><font color=\"#64748B\" size=\"2\">TEST</font></th>"
            "<th align=\"left\" width=\"16%\"><font color=\"#64748B\" size=\"2\">STATUS</font></th>"
            "<th align=\"left\"><font color=\"#64748B\" size=\"2\">SUMMARY</font></th></tr>");
        bool alt = false;
        auto group = static_cast<DiagGroup>(g);
        auto dgIt = data.diagIdsInGroup.find(group);
        if (dgIt != data.diagIdsInGroup.end()) {
            for (auto id : *dgIt) {
                if (!data.results.contains(id)) continue;
                const auto& r = data.results[id];
                const QString name = (r.displayName.isEmpty() ? data.diagDisplayName(id)
                                                              : r.displayName).toHtmlEscaped();
                h += QStringLiteral(
                    "<tr bgcolor=\"%1\">"
                    "<td><font color=\"#0F172A\"><b>%2</b></font></td>"
                    "<td><font color=\"%3\"><b>&#9679;&nbsp;%4</b></font></td>"
                    "<td><font color=\"#475569\">%5</font></td></tr>")
                    .arg(alt ? QStringLiteral("#F8FAFC") : QStringLiteral("#FFFFFF"))
                    .arg(name)
                    .arg(reportStatusColor(r.status), reportStatusText(r.status))
                    .arg(r.summary.isEmpty() ? QStringLiteral("&mdash;") : r.summary.toHtmlEscaped());
                alt = !alt;
            }
        }
        h += QStringLiteral("</table><br/>");
    }

    if (fullDetail) {
        h += QStringLiteral("<table width=\"100%\" cellpadding=\"10\" cellspacing=\"0\"><tr>"
            "<td bgcolor=\"#0F172A\"><font color=\"#FFFFFF\" size=\"4\"><b>Detailed Output</b></font></td>"
            "</tr></table><br/>");
        for (int g = 0; g < 5; ++g) {
            auto it = data.groupStats.find(g);
            if (it == data.groupStats.end() || it->value(QStringLiteral("total")).toInt() == 0) continue;
            h += QStringLiteral("<p><font color=\"#1E293B\" size=\"3\"><b>G%1 &middot; %2</b></font></p>")
                .arg(g+1).arg(g < data.groupLabels.size() ? data.groupLabels[g].toHtmlEscaped() : QString());
            auto group = static_cast<DiagGroup>(g);
            auto dgIt = data.diagIdsInGroup.find(group);
            if (dgIt != data.diagIdsInGroup.end()) {
                for (auto id : *dgIt) {
                    if (!data.results.contains(id)) continue;
                    const auto& r = data.results[id];
                    const QString name = (r.displayName.isEmpty() ? data.diagDisplayName(id)
                                                                  : r.displayName).toHtmlEscaped();
                    const QString sc = reportStatusColor(r.status);
                    h += QStringLiteral(
                        "<table width=\"100%\" cellpadding=\"9\" cellspacing=\"0\"><tr>"
                        "<td bgcolor=\"#F1F5F9\">"
                        "<font color=\"%1\"><b>&#9679;</b></font> <font color=\"#0F172A\"><b>%2</b></font> "
                        "<font color=\"%1\" size=\"2\"><b>%3</b></font> "
                        "<font color=\"#94A3B8\" size=\"2\">%4 ms</font>")
                        .arg(sc, name, reportStatusText(r.status))
                        .arg(r.durationMs);
                    if (!r.summary.isEmpty())
                        h += QStringLiteral("<br/><font color=\"#475569\" size=\"2\">%1</font>")
                            .arg(r.summary.toHtmlEscaped());
                    h += QStringLiteral("</td></tr></table>");
                    const QString body = r.details.isEmpty() ? r.rawOutput : r.details;
                    if (!body.trimmed().isEmpty())
                        h += QStringLiteral(
                            "<table width=\"100%\" cellpadding=\"10\" cellspacing=\"0\" border=\"1\" bordercolor=\"#334155\">"
                            "<tr><td bgcolor=\"#0F172A\"><pre style=\"font-family:'SF Mono','Consolas','Courier New',monospace;"
                            "font-size:11px;color:#E2E8F0\">%1</pre></td></tr></table><br/>")
                            .arg(body.toHtmlEscaped());
                }
            }
        }
    }
    h += QStringLiteral("<p align=\"center\"><font color=\"#94A3B8\" size=\"2\">"
        "Generated by NetDiagnostics &middot; All times in milliseconds</font></p>");
    h += QStringLiteral("</div>");
    return h;
}

QString ReportEngine::buildRichDocument(const ReportData& data) {
    int tPass=0,tWarn=0,tFail=0,tSkip=0,tInfo=0,tTotal=0;
    for (int g = 0; g < 5; ++g) {
        auto it = data.groupStats.find(g);
        if (it == data.groupStats.end()) continue;
        tPass += it->value(QStringLiteral("pass")).toInt();
        tWarn += it->value(QStringLiteral("warn")).toInt();
        tFail += it->value(QStringLiteral("fail")).toInt();
        tSkip += it->value(QStringLiteral("skip")).toInt();
        tInfo += it->value(QStringLiteral("info")).toInt();
        tTotal += it->value(QStringLiteral("total")).toInt();
    }

    static const char* kCss =
        "*{margin:0;padding:0;box-sizing:border-box}"
        "body{font-family:'Segoe UI',Roboto,Arial,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:24px}"
        ".wrap{max-width:960px;margin:0 auto}"
        ".header{text-align:center;padding:34px 24px;background:linear-gradient(135deg,#16213e,#0f3460);border-radius:14px;margin-bottom:26px}"
        ".header h1{font-size:26px;color:#00bcd4;margin-bottom:10px;letter-spacing:.5px}"
        ".header p{font-size:13px;color:#a0a0b8;margin:3px 0}"
        "h2{font-size:18px;color:#00bcd4;margin:26px 0 14px}"
        "h3{font-size:15px;color:#7fb2e6;margin:20px 0 10px}"
        ".cards{display:flex;gap:14px;margin-bottom:22px;flex-wrap:wrap}"
        ".card{flex:1;min-width:110px;text-align:center;padding:18px 10px;border-radius:12px}"
        ".card .count{display:block;font-size:30px;font-weight:700}"
        ".card .label{font-size:11px;color:#a0a0b8;margin-top:6px;letter-spacing:1px;text-transform:uppercase}"
        ".card.pass{background:#16281b;border:1px solid #2d5a2d}.card.pass .count{color:#4ade80}"
        ".card.warn{background:#2b2810;border:1px solid #5a5020}.card.warn .count{color:#facc15}"
        ".card.fail{background:#2b1616;border:1px solid #5a2d2d}.card.fail .count{color:#ef4444}"
        ".card.skip{background:#1e1e2e;border:1px solid #333}.card.skip .count{color:#9aa0b5}"
        ".card.info{background:#141f33;border:1px solid #24406a}.card.info .count{color:#3b82f6}"
        "table.grid{width:100%;border-collapse:collapse;font-size:13px;border-radius:10px;overflow:hidden}"
        "table.grid th{text-align:left;padding:11px 12px;background:#16213e;color:#a0a0b8;font-weight:600}"
        "table.grid td{padding:9px 12px;border-bottom:1px solid #2a2a4a;vertical-align:top}"
        "tr.sec td{background:#1a2840;color:#7fb2e6;font-weight:700}"
        ".badge{display:inline-block;padding:2px 11px;border-radius:12px;font-size:11px;font-weight:700}"
        ".badge.pass{background:#16281b;color:#4ade80}.badge.warn{background:#2b2810;color:#facc15}"
        ".badge.fail{background:#2b1616;color:#ef4444}.badge.skip{background:#26262e;color:#9aa0b5}"
        ".badge.info{background:#141f33;color:#3b82f6}"
        "details.test{background:#16213e;border-radius:10px;margin-bottom:12px;overflow:hidden}"
        "details.test>summary{padding:13px 16px;cursor:pointer;font-weight:600;font-size:14px}"
        "details.test.pass>summary{border-left:4px solid #4ade80}details.test.warn>summary{border-left:4px solid #facc15}"
        "details.test.fail>summary{border-left:4px solid #ef4444}details.test.skip>summary{border-left:4px solid #666}"
        "details.test.info>summary{border-left:4px solid #3b82f6}"
        ".body{padding:14px 16px 18px;border-top:1px solid #2a2a4a}"
        ".analysis{background:#0f1629;border-left:3px solid #00bcd4;padding:11px 13px;border-radius:6px;margin-bottom:12px;font-size:13px;line-height:1.6}"
        ".raw{background:#0a0a14;padding:13px;border-radius:6px;font-family:'Consolas','Courier New',monospace;font-size:12px;white-space:pre-wrap;line-height:1.5;color:#c0c0d0;max-height:420px;overflow:auto}"
        ".meta{color:#8890a6;font-size:11px;font-weight:400}"
        ".footer{text-align:center;padding:20px;color:#5a5a72;font-size:11px;margin-top:28px;border-top:1px solid #23233a}";

    auto card = [](const QString& cls, int n, const QString& lbl) {
        return QStringLiteral("<div class=\"card %1\"><span class=\"count\">%2</span>"
            "<span class=\"label\">%3</span></div>").arg(cls).arg(n).arg(lbl);
    };

    QString h;
    h += QStringLiteral("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<title>Network Diagnostic Report &mdash; %1</title>\n<style>").arg(data.timestamp);
    h += QString::fromLatin1(kCss);
    h += QStringLiteral("</style>\n</head>\n<body>\n<div class=\"wrap\">\n");
    h += QStringLiteral(
        "<div class=\"header\"><h1>Network Diagnostic Report</h1>"
        "<p>Generated: %1</p>"
        "<p>Target: <b style=\"color:#e0e0e0\">%2</b></p>"
        "<p>NetDiagnostics v%3 (build %4)</p></div>\n")
        .arg(data.timestamp, data.target, data.appVersion, data.buildNumber);

    h += QStringLiteral("<div class=\"cards\">");
    h += card(QStringLiteral("pass"), tPass, QStringLiteral("Pass"));
    h += card(QStringLiteral("warn"), tWarn, QStringLiteral("Warning"));
    h += card(QStringLiteral("fail"), tFail, QStringLiteral("Fail"));
    h += card(QStringLiteral("skip"), tSkip, QStringLiteral("Skipped"));
    h += card(QStringLiteral("info"), tInfo, QStringLiteral("Info"));
    h += QStringLiteral("</div>\n");

    // Summary table
    h += QStringLiteral("<h2>Summary &middot; %1 tests</h2>\n").arg(tTotal);
    h += QStringLiteral("<table class=\"grid\"><thead><tr><th style=\"width:44px\">#</th>"
        "<th>Test</th><th style=\"width:96px\">Status</th><th>Summary</th></tr></thead><tbody>\n");
    int idx = 0;
    for (int g = 0; g < 5; ++g) {
        auto it = data.groupStats.find(g);
        if (it == data.groupStats.end() || it->value(QStringLiteral("total")).toInt() == 0) continue;
        const QString glabel = g < data.groupLabels.size() ? data.groupLabels[g].toHtmlEscaped() : QString();
        h += QStringLiteral("<tr class=\"sec\"><td colspan=\"4\">G%1 &middot; %2</td></tr>\n").arg(g+1).arg(glabel);
        auto group = static_cast<DiagGroup>(g);
        auto dgIt = data.diagIdsInGroup.find(group);
        if (dgIt != data.diagIdsInGroup.end()) {
            for (auto id : *dgIt) {
                if (!data.results.contains(id)) continue;
                const auto& r = data.results[id];
                const QString name = (r.displayName.isEmpty() ? data.diagDisplayName(id)
                                                              : r.displayName).toHtmlEscaped();
                ++idx;
                h += QStringLiteral("<tr><td>%1</td><td>%2</td>"
                    "<td><span class=\"badge %3\">%4</span></td><td>%5</td></tr>\n")
                    .arg(idx).arg(name)
                    .arg(reportStatusClass(r.status), reportStatusText(r.status))
                    .arg(r.summary.isEmpty() ? QStringLiteral("&mdash;") : r.summary.toHtmlEscaped());
            }
        }
    }
    h += QStringLiteral("</tbody></table>\n");

    // Details
    h += QStringLiteral("<h2>Test Details</h2>\n");
    for (int g = 0; g < 5; ++g) {
        auto it = data.groupStats.find(g);
        if (it == data.groupStats.end() || it->value(QStringLiteral("total")).toInt() == 0) continue;
        const QString glabel = g < data.groupLabels.size() ? data.groupLabels[g].toHtmlEscaped() : QString();
        h += QStringLiteral("<h3>G%1 &middot; %2</h3>\n").arg(g+1).arg(glabel);
        auto group = static_cast<DiagGroup>(g);
        auto dgIt = data.diagIdsInGroup.find(group);
        if (dgIt != data.diagIdsInGroup.end()) {
            for (auto id : *dgIt) {
                if (!data.results.contains(id)) continue;
                const auto& r = data.results[id];
                const QString name = (r.displayName.isEmpty() ? data.diagDisplayName(id)
                                                              : r.displayName).toHtmlEscaped();
                const QString cls = reportStatusClass(r.status);
                h += QStringLiteral("<details class=\"test %1\"><summary>"
                    "<span class=\"badge %1\">%2</span> &nbsp;%3 "
                    "<span class=\"meta\">&middot; %4 ms</span></summary><div class=\"body\">")
                    .arg(cls, reportStatusText(r.status), name).arg(r.durationMs);
                if (!r.summary.isEmpty())
                    h += QStringLiteral("<div class=\"analysis\">%1</div>").arg(r.summary.toHtmlEscaped());
                const QString body = r.details.isEmpty() ? r.rawOutput : r.details;
                if (!body.trimmed().isEmpty())
                    h += QStringLiteral("<div class=\"raw\">%1</div>").arg(body.toHtmlEscaped());
                h += QStringLiteral("</div></details>\n");
            }
        }
    }

    h += QStringLiteral("<div class=\"footer\">Generated by NetDiagnostics &middot; "
        "All times in milliseconds</div>\n</div>\n</body>\n</html>\n");
    return h;
}

// ── File I/O ────────────────────────────────────────────────────────────

QString ReportEngine::exportHtml(const QString& filePath, const QString& html) {
    const QString path = normalizeReportPath(filePath);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::instance().event(QStringLiteral("exportHtml: cannot open %1").arg(path));
        return QString();
    }
    QTextStream ts(&f);
    ts << html;
    f.close();
    return path;
}

QString ReportEngine::exportPdf(const QString& filePath, const QString& html) {
    const QString path = normalizeReportPath(filePath);
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    writer.setTitle(QStringLiteral("Network Diagnostic Report"));

    QTextDocument doc;
    doc.setDefaultFont(QFont(QStringLiteral("Helvetica"), 10));
    doc.setHtml(html);
    doc.print(&writer);
    return QFile::exists(path) ? path : QString();
}

// ── Path helpers ────────────────────────────────────────────────────────

QString ReportEngine::defaultReportPath(const QString& ext) {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::tempPath();
    QDir().mkpath(dir);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    return QDir(dir).filePath(QStringLiteral("NetDiagnostics_report_%1.%2").arg(stamp, ext));
}

// ── Save dialog ─────────────────────────────────────────────────────────

void ReportEngine::requestSavePath(const QString& format) {
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
    auto* dlg = new QFileDialog(nullptr, QStringLiteral("Save Report"),
                                defaultReportPath(format));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setAcceptMode(QFileDialog::AcceptSave);
    dlg->setNameFilter(format == QLatin1String("pdf")
        ? QStringLiteral("PDF document (*.pdf)")
        : QStringLiteral("HTML document (*.html)"));
    dlg->setDefaultSuffix(format);
    connect(dlg, &QFileDialog::fileSelected, this, [this, format](const QString& p) {
        if (!p.isEmpty()) emit savePathPicked(format, p);
    });
    dlg->open();
#else
    emit savePathPicked(format, defaultReportPath(format));
#endif
}

// ── Email handoff ──────────────────────────────────────────────────────

void ReportEngine::emailReportDesktop(const QString& path) {
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
    const QString subject = QStringLiteral("Network Diagnostic Report");
#ifdef Q_OS_LINUX
    if (QProcess::startDetached(QStringLiteral("xdg-email"),
            {QStringLiteral("--subject"), subject, QStringLiteral("--attach"), path}))
        return;
#endif
    QUrl mailto;
    mailto.setScheme(QStringLiteral("mailto"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("subject"), subject);
    q.addQueryItem(QStringLiteral("body"),
        QStringLiteral("The Network Diagnostic report is saved at: %1").arg(path));
    mailto.setQuery(q);
    QDesktopServices::openUrl(mailto);
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#else
    Q_UNUSED(path);
#endif
}
