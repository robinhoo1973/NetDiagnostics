// =============================================================================
// ReportEngine.cpp — extracted from AppState.cpp (~300 lines)
// =============================================================================
#include "Report/Model/ReportEngine.h"
#include "Common/Utils/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTextDocument>
#include <QPdfWriter>
#include <QPainter>
#include <QImage>
#include <QBuffer>
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
        case DiagStatus::Pass:    return QStringLiteral("#4ADE80");
        case DiagStatus::Warning: return QStringLiteral("#FBBF24");
        case DiagStatus::Fail:    return QStringLiteral("#F87171");
        case DiagStatus::Error:   return QStringLiteral("#F87171");
        case DiagStatus::Skipped: return QStringLiteral("#9CA3AF");
        default:                  return QStringLiteral("#60A5FA");
    }
}

// 5WHY: Reports used colored dots or Unicode glyphs instead of proper
// graphic icons. Unicode characters render inconsistently across fonts
// and platforms (some show as tofu □). QTextDocument CANNOT render SVG
// data URIs without QtSvg (not linked). Instead, render the icons
// programmatically with QPainter → PNG → base64 data URI. This works
// in both QTextDocument (preview + PDF) and browser WebView (rich HTML).
QImage renderStatusIcon(DiagStatus s, int size) {
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    // Colors match resources/icons/badge-*.svg
    QColor bg;
    switch (s) {
        case DiagStatus::Pass:    bg = QColor(0x16, 0xA3, 0x4A); break;
        case DiagStatus::Warning: bg = QColor(0xF5, 0x9E, 0x0B); break;
        case DiagStatus::Fail:    // fallthrough — Error uses same red as Fail
        case DiagStatus::Error:   bg = QColor(0xDC, 0x26, 0x26); break;
        case DiagStatus::Skipped: bg = QColor(0x6B, 0x72, 0x80); break;
        default:                  bg = QColor(0x25, 0x63, 0xEB); break; // Info
    }
    const float margin = size * 0.08f;
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(margin, margin, size - 2*margin, size - 2*margin));
    p.setPen(QPen(Qt::white, size * 0.10f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const float cx = size * 0.5f, cy = size * 0.5f, r = size * 0.28f;
    switch (s) {
        case DiagStatus::Pass:
            p.drawLine(QPointF(cx-r*0.6f, cy), QPointF(cx-r*0.1f, cy+r*0.5f));
            p.drawLine(QPointF(cx-r*0.1f, cy+r*0.5f), QPointF(cx+r*0.7f, cy-r*0.4f));
            break;
        case DiagStatus::Warning:
            p.drawLine(QPointF(cx, cy-r*0.6f), QPointF(cx, cy+r*0.15f));
            p.setPen(Qt::NoPen); p.setBrush(Qt::white);
            p.drawEllipse(QPointF(cx, cy+r*0.55f), r*0.12f, r*0.12f);
            break;
        case DiagStatus::Fail:
        case DiagStatus::Error:
            p.drawLine(QPointF(cx-r*0.5f, cy-r*0.5f), QPointF(cx+r*0.5f, cy+r*0.5f));
            p.drawLine(QPointF(cx+r*0.5f, cy-r*0.5f), QPointF(cx-r*0.5f, cy+r*0.5f));
            break;
        case DiagStatus::Skipped:
            p.drawLine(QPointF(cx-r*0.6f, cy), QPointF(cx+r*0.6f, cy));
            break;
        default: // Info — "i"
            p.setPen(Qt::NoPen); p.setBrush(Qt::white);
            p.drawEllipse(QPointF(cx, cy-r*0.55f), r*0.12f, r*0.12f);
            p.setPen(QPen(Qt::white, size * 0.10f, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(cx, cy-r*0.2f), QPointF(cx, cy+r*0.5f));
            break;
    }
    p.end();
    return img;
}

// 5WHY: SVG data URIs needed QtSvg (not linked) for QTextDocument rendering.
// QPainter→PNG→base64 works universally: QTextDocument preview, PDF export,
// and browser WebView all support PNG data URIs natively.
QString reportStatusIconImg(DiagStatus s, int size) {
    QImage img = renderStatusIcon(s, size);
    QByteArray pngData;
    QBuffer buf(&pngData);
    buf.open(QIODevice::WriteOnly);
    // 5WHY: img.save() return was unchecked — a null QImage (size ≤ 0)
    // would silently produce an empty data URI, showing a broken image.
    if (!img.save(&buf, "PNG")) return {};
    return QStringLiteral("<img src='data:image/png;base64,")
         + QString::fromLatin1(pngData.toBase64())
         + QStringLiteral("' width='%1' height='%1' "
           "style='vertical-align:middle;display:inline-block' alt=''/>")
         .arg(size);
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
        case DiagStatus::Error:   return QStringLiteral("error");
        case DiagStatus::Skipped: return QStringLiteral("skip");
        default:                  return QStringLiteral("info");
    }
}

// 5WHY: buildHtml() and buildRichDocument() each defined their own set of
// ~20 theme-aware color constants — duplicated ~15 tokens between the two.
// Extracted to a shared struct so both methods use the same palette and
// theme changes only need one update point.
struct ReportColors {
    // Status colors (theme-independent)
    QString pass    = QStringLiteral("#4ADE80");
    QString warn    = QStringLiteral("#FBBF24");
    QString fail    = QStringLiteral("#F87171");
    QString skip    = QStringLiteral("#9CA3AF");
    QString info    = QStringLiteral("#38BDF8");
    QString cyan    = QStringLiteral("#22D3EE");
    // Theme-aware colors
    QString textPrimary, textSecondary, textMuted;
    QString bgHeader, bgSection, bgRowAlt, bgRow;
    QString bgCardPass, bgCardInfo, bgCardWarn, bgCardFail, bgCardSkip;
    QString borderColor, codeBlockBg, codeBlockFg, detailBg, footerColor;

    explicit ReportColors(bool dark) {
        if (dark) {
            textPrimary  = QStringLiteral("#F1F5F9");
            textSecondary= QStringLiteral("#94A3B8");
            textMuted    = QStringLiteral("#64748B");
            bgHeader     = QStringLiteral("#0C4A6E");
            bgSection    = QStringLiteral("#1E293B");
            bgRowAlt     = QStringLiteral("#1E293B");
            bgRow        = QStringLiteral("#0F172A");
            bgCardPass   = QStringLiteral("#16281b");
            bgCardInfo   = QStringLiteral("#141f33");
            bgCardWarn   = QStringLiteral("#2b2810");
            bgCardFail   = QStringLiteral("#2b1616");
            bgCardSkip   = QStringLiteral("#1e1e2e");
            borderColor  = QStringLiteral("#334155");
            codeBlockBg  = QStringLiteral("#0a0a14");
            codeBlockFg  = QStringLiteral("#c0c0d0");
            detailBg     = QStringLiteral("#0f1629");
            footerColor  = QStringLiteral("#5a5a72");
        } else {
            textPrimary  = QStringLiteral("#1E293B");
            textSecondary= QStringLiteral("#475569");
            textMuted    = QStringLiteral("#94A3B8");
            bgHeader     = QStringLiteral("#0F172A");
            bgSection    = QStringLiteral("#0F172A");
            bgRowAlt     = QStringLiteral("#F8FAFC");
            bgRow        = QStringLiteral("#FFFFFF");
            bgCardPass   = QStringLiteral("#ECFDF5");
            bgCardInfo   = QStringLiteral("#EFF6FF");
            bgCardWarn   = QStringLiteral("#FFFBEB");
            bgCardFail   = QStringLiteral("#FEF2F2");
            bgCardSkip   = QStringLiteral("#F1F5F9");
            borderColor  = QStringLiteral("#E2E8F0");
            codeBlockBg  = QStringLiteral("#0F172A");
            codeBlockFg  = QStringLiteral("#E2E8F0");
            detailBg     = QStringLiteral("#F8FAFC");
            footerColor  = QStringLiteral("#94A3B8");
        }
    }
};

} // namespace

// ── Public: HTML generation ─────────────────────────────────────────────

QString ReportEngine::buildHtml(const ReportData& data, bool fullDetail, bool darkBackground) {
    const ReportColors c(darkBackground);
    // Aliases for concise reference in the large HTML string below
    const QString& colorPass = c.pass, &colorWarn = c.warn, &colorFail = c.fail;
    const QString& colorSkip = c.skip, &colorInfo = c.info, &colorCyan = c.cyan;
    const QString& textPrimary = c.textPrimary, &textSecondary = c.textSecondary;
    const QString& textMuted = c.textMuted, &bgHeader = c.bgHeader;
    const QString& bgSection = c.bgSection, &bgRowAlt = c.bgRowAlt, &bgRow = c.bgRow;
    const QString& bgCardPass = c.bgCardPass, &bgCardInfo = c.bgCardInfo;
    const QString& bgCardWarn = c.bgCardWarn, &bgCardFail = c.bgCardFail;
    const QString& bgCardSkip = c.bgCardSkip, &borderColor = c.borderColor;
    const QString& codeBlockBg = c.codeBlockBg, &codeBlockFg = c.codeBlockFg;
    const QString& detailBg = c.detailBg, &footerColor = c.footerColor;

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
    h += QStringLiteral("<div style=\"font-family:'Helvetica Neue',Arial,'PingFang SC','Microsoft YaHei',sans-serif;"
        "color:%1;max-width:800px;margin:0 auto\">").arg(textPrimary);

    // ── Header band with gradient-style dark background ─────────────────
    // Header text colours — always light-on-dark for the header band
    const QString headerTargetColor = QStringLiteral("#E2E8F0");
    const QString headerMetaColor   = QStringLiteral("#94A3B8");
    h += QStringLiteral(
        "<table width=\"100%\" cellpadding=\"20\" cellspacing=\"0\" style=\"border-radius:8px\"><tr>"
        "<td bgcolor=\"%1\">"
        "<p style=\"margin:0 0 6px 0\"><span style=\"font-size:22px;color:%5\"><b>Network Diagnostic Report</b></span></p>"
        "<p style=\"margin:0 0 2px 0\"><span style=\"font-size:14px;color:%6\">%2</span></p>"
        "<p style=\"margin:0\"><span style=\"font-size:11px;color:%7\">%3 &middot; v%4 (build %8)</span></p>"
        "</td></tr></table>"
        "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"><tr><td height=\"12\"></td></tr></table>")
        .arg(bgHeader, data.target, data.timestamp, data.appVersion, colorCyan,
             headerTargetColor, headerMetaColor, data.buildNumber);

    // ── Pass-rate progress bar ─────────────────────────────────────────
    int passPercent = tTotal > 0 ? (tPass * 100 / tTotal) : 0;
    const QString barColor = passPercent >= 90 ? QStringLiteral("#4ADE80")
                           : passPercent >= 70 ? QStringLiteral("#FBBF24")
                           : QStringLiteral("#F87171");
    const QString barBg = QStringLiteral("#1E293B");  // progress bar track — always dark
    h += QStringLiteral(
        "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\">"
        "<tr><td style=\"padding:4px 0\">"
        "<span style=\"font-size:13px;color:%3\"><b>Overall Pass Rate: %1%</b></span>"
        "</td></tr>"
        "<tr><td>"
        "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" style=\"border:1px solid %4;border-radius:4px\">"
        "<tr><td bgcolor=\"%5\">"
        "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"><tr>"
        "<td width=\"%1%\" bgcolor=\"%2\" style=\"padding:3px 0\"></td>"
        "<td></td></tr></table>"
        "</td></tr></table>"
        "</td></tr></table>"
        "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"><tr><td height=\"14\"></td></tr></table>")
        .arg(passPercent).arg(barColor).arg(colorCyan)
        .arg(borderColor).arg(barBg);

    // ── Summary cards — 5-column card row ──────────────────────────────
    // 5WHY: Summary cards showed only numbers — no visual status cues.
    // Add base64 SVG icon above the count for color/icon recognition.
    auto card = [](const QString& bg, const QString& fg, int val, const QString& lbl,
                   const QString& iconImg) {
        // Direct concatenation for icon — .arg() would consume HTML
        // attribute characters within the img tag (e.g. %20 in encoded
        // attributes). Safer to concatenate than escape for .arg().
        QString td = QStringLiteral(
            "<td width=\"20%\" align=\"center\" bgcolor=\"%1\""
            " style=\"padding:12px 6px\">")
            .arg(bg);
        td += iconImg;
        td += QStringLiteral("<br/><span style=\"font-size:28px;color:%1\"><b>%2</b></span><br/>"
            "<span style=\"font-size:11px;color:%1\">%3</span></td>")
            .arg(fg).arg(val).arg(lbl);
        return td;
    };
    h += QStringLiteral("<table width=\"100%\" cellpadding=\"0\" cellspacing=\"4\"><tr>");
    h += card(bgCardPass, colorPass, tPass, QStringLiteral("Pass"), reportStatusIconImg(DiagStatus::Pass, 24));
    h += card(bgCardInfo, colorInfo, tInfo, QStringLiteral("Info"), reportStatusIconImg(DiagStatus::Info, 24));
    h += card(bgCardWarn, colorWarn, tWarn, QStringLiteral("Warning"), reportStatusIconImg(DiagStatus::Warning, 24));
    h += card(bgCardFail, colorFail, tFail, QStringLiteral("Fail"), reportStatusIconImg(DiagStatus::Fail, 24));
    h += card(bgCardSkip, colorSkip, tSkip, QStringLiteral("Skipped"), reportStatusIconImg(DiagStatus::Skipped, 24));
    h += QStringLiteral("</tr></table>");
    h += QStringLiteral("<p align=\"center\" style=\"margin:10px 0 18px 0\"><span style=\"font-size:12px;color:%1\">%2 tests total</span></p>")
        .arg(textMuted).arg(tTotal);

    // Per-group results
    for (int g = 0; g < 5; ++g) {
        auto it = data.groupStats.find(g);
        if (it == data.groupStats.end() || it->value(QStringLiteral("total")).toInt() == 0) continue;
        const auto& s = *it;
        const QString glabel = g < data.groupLabels.size() ? data.groupLabels[g].toHtmlEscaped() : QString();
        h += QStringLiteral(
            "<table width=\"100%\" cellpadding=\"10\" cellspacing=\"0\"><tr>"
            "<td bgcolor=\"%1\" style=\"padding:12px 14px\">"
            "<span style=\"font-size:14px;color:%2\"><b>G%3 &middot; %4</b></span>&nbsp;&nbsp;"
            "<span style=\"font-size:12px;color:%5\"><b>P %6</b></span>"
            "<span style=\"font-size:12px;color:%7\"> &middot; </span><span style=\"font-size:12px;color:%8\"><b>I %9</b></span>"
            "<span style=\"font-size:12px;color:%7\"> &middot; </span><span style=\"font-size:12px;color:%10\"><b>W %11</b></span>"
            "<span style=\"font-size:12px;color:%7\"> &middot; </span><span style=\"font-size:12px;color:%12\"><b>F %13</b></span>"
            "<span style=\"font-size:12px;color:%7\"> &middot; </span><span style=\"font-size:12px;color:%14\"><b>S %15</b></span>"
            "</td></tr></table>")
            .arg(bgSection)
            .arg(textPrimary)
            .arg(g+1).arg(glabel)
            .arg(colorPass).arg(s.value(QStringLiteral("pass")).toInt())
            .arg(textMuted)
            .arg(colorInfo).arg(s.value(QStringLiteral("info")).toInt())
            .arg(colorWarn).arg(s.value(QStringLiteral("warn")).toInt())
            .arg(colorFail).arg(s.value(QStringLiteral("fail")).toInt())
            .arg(colorSkip).arg(s.value(QStringLiteral("skip")).toInt());
        h += QStringLiteral(
            "<table width=\"100%\" cellpadding=\"9\" cellspacing=\"0\""
            " style=\"border-collapse:collapse\">"
            "<tr bgcolor=\"%1\">"
            "<th align=\"left\" width=\"42%\" style=\"padding:10px 9px;border-bottom:2px solid %2\">"
            "<span style=\"font-size:11px;color:%3\">TEST</span></th>"
            "<th align=\"left\" width=\"16%\" style=\"padding:10px 9px;border-bottom:2px solid %2\">"
            "<span style=\"font-size:11px;color:%3\">STATUS</span></th>"
            "<th align=\"left\" style=\"padding:10px 9px;border-bottom:2px solid %2\">"
            "<span style=\"font-size:11px;color:%3\">SUMMARY</span></th></tr>")
            .arg(bgRowAlt).arg(borderColor).arg(textMuted);
        bool alt = false;
        auto group = static_cast<DiagGroup>(g);
        auto dgIt = data.diagIdsInGroup.find(group);
        if (dgIt != data.diagIdsInGroup.end()) {
            for (auto id : *dgIt) {
                if (!data.results.contains(id)) continue;
                const auto& r = data.results[id];
                const QString name = (r.displayName.isEmpty() ? data.diagDisplayName(id)
                                                              : r.displayName).toHtmlEscaped();
                const QString rowBg = alt ? bgRowAlt : bgRow;
                // 5WHY: Reports used Unicode glyphs that render as tofu (□)
                // on many fonts/platforms. SVG icons via base64 data URI
                // render identically across browsers, mail clients, and
                // QTextDocument. Direct concatenation avoids .arg() eating
                // base64 percent-encoded characters.
                const QString iconImg = reportStatusIconImg(r.status, 18);
                h += QStringLiteral(
                    "<tr bgcolor=\"%1\" style=\"border-bottom:1px solid %6\">"
                    "<td style=\"padding:10px 9px\"><span style=\"font-size:13px;color:%2\"><b>%3</b></span></td>"
                    "<td style=\"padding:10px 9px\">")
                    .arg(rowBg, textPrimary, name)
                    .arg(reportStatusColor(r.status), reportStatusText(r.status))
                    .arg(borderColor);
                h += iconImg;
                h += QStringLiteral(
                    "&nbsp;<span style=\"font-size:12px;color:%1\"><b>%2</b></span></td>"
                    "<td style=\"padding:10px 9px\"><span style=\"font-size:12px;color:%3\">%4</span></td></tr>")
                    .arg(reportStatusColor(r.status), reportStatusText(r.status),
                         textSecondary,
                         r.summary.isEmpty() ? QStringLiteral("&mdash;") : r.summary.toHtmlEscaped());
                alt = !alt;
            }
        }
        h += QStringLiteral("</table><br/>");
    }

    if (fullDetail) {
        h += QStringLiteral("<table width=\"100%\" cellpadding=\"12\" cellspacing=\"0\"><tr>"
            "<td bgcolor=\"%1\"><span style=\"font-size:18px;color:%2\"><b>Detailed Output</b></span></td>"
            "</tr></table><br/>").arg(bgSection).arg(colorCyan);
        for (int g = 0; g < 5; ++g) {
            auto it = data.groupStats.find(g);
            if (it == data.groupStats.end() || it->value(QStringLiteral("total")).toInt() == 0) continue;
            h += QStringLiteral("<p><span style=\"font-size:14px;color:%1\"><b>G%2 &middot; %3</b></span></p>")
                .arg(textPrimary).arg(g+1).arg(g < data.groupLabels.size() ? data.groupLabels[g].toHtmlEscaped() : QString());
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
                        "<table width=\"100%\" cellpadding=\"10\" cellspacing=\"0\"><tr>"
                        "<td bgcolor=\"%1\" style=\"padding:12px 14px\">"
                        "<span style=\"display:inline-block;width:10px;height:10px;border-radius:5px;background:%2;margin-right:8px;vertical-align:middle\"></span>"
                        "<span style=\"font-size:14px;color:%3;margin-right:10px\"><b>%4</b></span>"
                        "<span style=\"font-size:12px;color:%2;margin-right:12px\"><b>%5</b></span>"
                        "<span style=\"font-size:11px;color:%6\">%7 ms</span>")
                        .arg(detailBg, sc, textPrimary, name, reportStatusText(r.status), textMuted, QString::number(r.durationMs));
                    if (!r.summary.isEmpty())
                        h += QStringLiteral("<br/><span style=\"font-size:12px;color:%1\">%2</span>")
                            .arg(textSecondary, r.summary.toHtmlEscaped());
                    h += QStringLiteral("</td></tr></table>");
                    const QString body = r.details.isEmpty() ? r.rawOutput : r.details;
                    if (!body.trimmed().isEmpty())
                        h += QStringLiteral(
                            "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"0\""
                            " style=\"border:1px solid %2\">"
                            "<tr><td bgcolor=\"%3\"><pre style=\"font-family:'SF Mono','Consolas','Courier New',monospace;"
                            "font-size:11px;color:%4;line-height:1.5;margin:0;width:100%\">%1</pre></td></tr></table><br/>")
                            .arg(body.toHtmlEscaped(),
                                 borderColor, codeBlockBg, codeBlockFg);
                }
            }
        }
    }
    h += QStringLiteral("<p align=\"center\"><span style=\"font-size:11px;color:%1\">"
        "Generated by NetDiagnostics &middot; All times in milliseconds</span></p>")
        .arg(footerColor);
    h += QStringLiteral("</div>");
    return h;
}

QString ReportEngine::buildRichDocument(const ReportData& data, bool darkBackground) {
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

    // 5WHY: Rich HTML CSS was hardcoded dark — HTML preview + export always
    // dark regardless of app theme. Prepend a CSS custom-property theme block
    // so a single color swap at the :root level applies the entire theme.
    // 5WHY: Card/badge backgrounds were hardcoded dark — light mode showed
    // dark cards on a light page. Add CSS variables for both themes.
    const QString cssThemeBlock = darkBackground
        ? QStringLiteral(":root{--bg:#0F172A;--fg:#F1F5F9;--fg2:#94A3B8;--fg3:#64748B;"
            "--card-bg:#1E293B;--header-bg1:#1E293B;--header-bg2:#0C4A6E;"
            "--border:#334155;--footer-fg:#5a5a72;--footer-border:#23233a;"
            "--card-pass-bg:#16281b;--card-info-bg:#141f33;--card-warn-bg:#2b2810;"
            "--card-fail-bg:#2b1616;--card-skip-bg:#1e1e2e;--card-error-bg:#2b1111;"
            "--badge-pass-bg:#16281b;--badge-info-bg:#141f33;--badge-warn-bg:#2b2810;"
            "--badge-fail-bg:#2b1616;--badge-skip-bg:#26262e;"
            "--sec-row-bg:#1a2840;--border-card-pass:#2d5a2d;--border-card-info:#24406a;"
            "--border-card-warn:#5a5020;--border-card-fail:#5a2d2d;--border-card-skip:#333;"
            "--border-card-error:#5a2020}")
        : QStringLiteral(":root{--bg:#F8FAFC;--fg:#0F172A;--fg2:#475569;--fg3:#94A3B8;"
            "--card-bg:#FFFFFF;--header-bg1:#E0F2FE;--header-bg2:#BAE6FD;"
            "--border:#E2E8F0;--footer-fg:#94A3B8;--footer-border:#E2E8F0;"
            "--card-pass-bg:#ECFDF5;--card-info-bg:#EFF6FF;--card-warn-bg:#FFFBEB;"
            "--card-fail-bg:#FEF2F2;--card-skip-bg:#F1F5F9;--card-error-bg:#FEF2F2;"
            "--badge-pass-bg:#DCFCE7;--badge-info-bg:#DBEAFE;--badge-warn-bg:#FEF3C7;"
            "--badge-fail-bg:#FEE2E2;--badge-skip-bg:#E2E8F0;"
            "--sec-row-bg:#E0F2FE;--border-card-pass:#BBF7D0;--border-card-info:#BFDBFE;"
            "--border-card-warn:#FDE68A;--border-card-fail:#FECACA;--border-card-skip:#CBD5E1;"
            "--border-card-error:#FECACA}");
    // 5WHY: html/body width constraints + overflow-wrap were only in
    // QML's injectViewportCss(). Moving them into the generated CSS
    // keeps all report styling in one authoritative location.
    // NOTE: table{display:block} was NOT ported — it breaks the table
    // formatting model (border-collapse, th/td column alignment are
    // undefined on block elements). Rely on .wrap {max-width:960px}
    // to constrain table width instead.
    const QString kCss = cssThemeBlock
        + QStringLiteral(
        "*{margin:0;padding:0;box-sizing:border-box}"
        "html,body{max-width:100%;overflow-x:auto;overflow-wrap:break-word;word-wrap:break-word}"
        "img,svg,pre,code{max-width:100%;height:auto}"
        "body{font-family:'Segoe UI',Roboto,Arial,sans-serif;background:var(--bg);color:var(--fg);padding:24px}"
        ".wrap{max-width:960px;margin:0 auto}"
        ".header{text-align:center;padding:34px 24px;background:linear-gradient(135deg,#1E293B,#0C4A6E);border-radius:14px;margin-bottom:26px}"
        ".header h1{font-size:26px;color:#22D3EE;margin-bottom:10px;letter-spacing:.5px}"
        ".header p{font-size:13px;color:#94A3B8;margin:3px 0}"
        "h2{font-size:18px;color:#22D3EE;margin:26px 0 14px}"
        "h3{font-size:15px;color:#60A5FA;margin:20px 0 10px}"
        ".cards{display:flex;gap:14px;margin-bottom:22px;flex-wrap:wrap}"
        ".card{flex:1;min-width:110px;text-align:center;padding:18px 10px;border-radius:12px}"
        ".card .icon{display:block;font-size:18px;margin-bottom:2px}"
        ".card .count{display:block;font-size:30px;font-weight:700}"
        ".card .label{font-size:11px;color:var(--fg2);margin-top:6px;letter-spacing:1px;text-transform:uppercase}"
        ".card.pass{background:var(--card-pass-bg);border:1px solid var(--border-card-pass)}.card.pass .count{color:#4ADE80}"
        ".card.warn{background:var(--card-warn-bg);border:1px solid var(--border-card-warn)}.card.warn .count{color:#FBBF24}"
        ".card.fail{background:var(--card-fail-bg);border:1px solid var(--border-card-fail)}.card.fail .count{color:#F87171}"
        ".card.skip{background:var(--card-skip-bg);border:1px solid var(--border-card-skip)}.card.skip .count{color:#9CA3AF}"
        ".card.info{background:var(--card-info-bg);border:1px solid var(--border-card-info)}.card.info .count{color:#60A5FA}"
        ".card.error{background:var(--card-error-bg);border:1px solid var(--border-card-error)}.card.error .count{color:#F87171}"
        "table.grid{width:100%;border-collapse:collapse;font-size:13px;border-radius:10px;overflow:hidden}"
        "table.grid th{text-align:left;padding:11px 12px;background:var(--card-bg);color:var(--fg2);font-weight:600}"
        "table.grid td{padding:9px 12px;border-bottom:1px solid var(--border);vertical-align:top}"
        "tr.sec td{background:var(--sec-row-bg);color:#60A5FA;font-weight:700}"
        ".badge{display:inline-block;padding:2px 11px;border-radius:12px;font-size:11px;font-weight:700}"
        ".badge.pass{background:var(--badge-pass-bg);color:#4ADE80}.badge.warn{background:var(--badge-warn-bg);color:#FBBF24}"
        ".badge.fail{background:var(--badge-fail-bg);color:#F87171}.badge.skip{background:var(--badge-skip-bg);color:#9CA3AF}"
        ".badge.info{background:var(--badge-info-bg);color:#60A5FA}"
        "details.test{background:var(--card-bg);border-radius:10px;margin-bottom:12px;overflow:hidden}"
        "details.test>summary{padding:13px 16px;cursor:pointer;font-weight:600;font-size:14px}"
        "details.test.pass>summary{border-left:4px solid #4ADE80}details.test.warn>summary{border-left:4px solid #FBBF24}"
        "details.test.fail>summary{border-left:4px solid #F87171}details.test.skip>summary{border-left:4px solid #9CA3AF}"
        "details.test.info>summary{border-left:4px solid #60A5FA}"
        ".body{padding:14px 16px 18px;border-top:1px solid #334155}"
        ".analysis{background:#0f1629;border-left:3px solid #00bcd4;padding:11px 13px;border-radius:6px;margin-bottom:12px;font-size:13px;line-height:1.6}"
        ".raw{background:#0a0a14;padding:13px;border-radius:6px;font-family:'Consolas','Courier New',monospace;font-size:12px;white-space:pre-wrap;line-height:1.5;color:#c0c0d0;max-height:420px;overflow:auto}"
        ".meta{color:#8890a6;font-size:11px;font-weight:400}"
        ".footer{text-align:center;padding:20px;color:var(--footer-fg);font-size:11px;margin-top:28px;border-top:1px solid var(--footer-border)}");

    // 5WHY: Unicode icons replaced with inline SVG <img> tags using base64
    // data URIs. The .card .icon CSS still applies font-size but we override
    // with explicit width/height on the img element.
    auto card = [](const QString& cls, const QString& iconImg, int n, const QString& lbl) {
        return QStringLiteral("<div class=\"card %1\">%2"
            "<span class=\"count\">%3</span>"
            "<span class=\"label\">%4</span></div>").arg(cls, iconImg).arg(n).arg(lbl);
    };

    QString h;
    // 5WHY: initial-scale=1 on mobile causes height-fit zoom (page height
    // matched to screen height). Use viewport width-fit with user-scalable
    // for correct width-matching and pinch-to-zoom support.
    h += QStringLiteral("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=5.0, user-scalable=yes\">\n"
        "<title>Network Diagnostic Report &mdash; %1</title>\n<style>").arg(data.timestamp);
    h += kCss;
    h += QStringLiteral("</style>\n</head>\n<body>\n<div class=\"wrap\">\n");
    h += QStringLiteral(
        "<div class=\"header\"><h1>Network Diagnostic Report</h1>"
        "<p>Generated: %1</p>"
        "<p>Target: <b style=\"color:#e0e0e0\">%2</b></p>"
        "<p>NetDiagnostics v%3 (build %4)</p></div>\n")
        .arg(data.timestamp, data.target, data.appVersion, data.buildNumber);

    // 5WHY: Unicode card icons (&#10003; etc.) render inconsistently across
    // fonts. Base64-encoded SVG images match the app's QML icon set exactly.
    h += QStringLiteral("<div class=\"cards\">");
    h += card(QStringLiteral("pass"), reportStatusIconImg(DiagStatus::Pass, 32), tPass, QStringLiteral("Pass"));
    h += card(QStringLiteral("info"), reportStatusIconImg(DiagStatus::Info, 32), tInfo, QStringLiteral("Info"));
    h += card(QStringLiteral("warn"), reportStatusIconImg(DiagStatus::Warning, 32), tWarn, QStringLiteral("Warning"));
    h += card(QStringLiteral("fail"), reportStatusIconImg(DiagStatus::Fail, 32), tFail, QStringLiteral("Fail"));
    h += card(QStringLiteral("skip"), reportStatusIconImg(DiagStatus::Skipped, 32), tSkip, QStringLiteral("Skipped"));
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
                // 5WHY: Unicode icons → SVG data URI for consistent rendering.
                // Direct concatenation avoids .arg() consuming base64 % escapes.
                const QString iconImg = reportStatusIconImg(r.status, 18);
                h += QStringLiteral("<tr><td>%1</td><td>%2</td><td>")
                    .arg(idx).arg(name);
                h += iconImg;
                h += QStringLiteral(
                    "&nbsp;<span class=\"badge %1\">%2</span></td><td>%3</td></tr>\n")
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

// ── HTML-to-image rendering ─────────────────────────────────────────────
// Renders Qt Rich Text HTML to a QImage using QTextDocument + QPainter.
// This gives far better fidelity than QML Text.RichText (which cannot render
// border-radius, inline-block, or CSS styles).  Used for in-app report
// preview so the PDF/HTML preview matches the dashboard and shared page.
QImage ReportEngine::renderHtmlToImage(const QString& html, int width) {
    QTextDocument doc;
    doc.setDefaultFont(QFont(QStringLiteral("Helvetica"), 10));
    doc.setHtml(html);

    // QTextDocument::size() gives the natural layout size at the given width
    doc.setTextWidth(width);
    QSizeF docSize = doc.size();
    int h = qMax(100, (int)qCeil(docSize.height()));

    QImage img(width, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    doc.drawContents(&painter, QRectF(0, 0, width, h));
    painter.end();
    return img;
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
    // 5WHY: QPdfWriter default resolution (1200 DPI) makes QTextDocument
    // fonts appear tiny — CSS px sizes (e.g. font-size:13px) are laid out
    // at screen DPI (96) but rendered at printer DPI, producing illegible
    // micro-text. Fix: force PDF resolution to match screen DPI so font
    // sizes are consistent with the in-app QTextDocument preview.
    //   A4 = 210mm - 15mm*2 margins = 180mm usable ≈ 7.09 inches
    //   At 96 DPI: 680px text width for proper page-fit layout.
    const QString path = normalizeReportPath(filePath);
    QPdfWriter writer(path);
    writer.setResolution(96);  // match screen DPI for consistent font sizing
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    writer.setTitle(QStringLiteral("Network Diagnostic Report"));

    QTextDocument doc;
    // 5WHY: 10pt default was too small for PDF reading. 12pt base font
    // with 680px text width produces readable output on A4 pages.
    doc.setDefaultFont(QFont(QStringLiteral("Helvetica"), 12));
    doc.setTextWidth(680);  // fit A4 page width at 96 DPI
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

// ── Email handoff ──────────────────────────────────────────────────────

void ReportEngine::emailReportDesktop(const QString& path) {
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
    const QString subject = QStringLiteral("Network Diagnostic Report");
#if defined(Q_OS_LINUX)
    if (QProcess::startDetached(QStringLiteral("xdg-email"),
            {QStringLiteral("--subject"), subject, QStringLiteral("--attach"), path}))
        return;
#endif
    // 5WHY: openUrl(mailto) + openUrl(folder) opened TWO windows — mail client
    // AND file explorer. On Windows, the file-explorer popup is disorienting;
    // the mail client already has a file-picker for attaching the report.
    QUrl mailto;
    mailto.setScheme(QStringLiteral("mailto"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("subject"), subject);
    q.addQueryItem(QStringLiteral("body"),
        QStringLiteral("The Network Diagnostic report is saved at: %1").arg(path));
    mailto.setQuery(q);
    QDesktopServices::openUrl(mailto);
    // Open the folder only on platforms where the mail client can't attach
    // files directly via the body hint (non-default mail clients on Linux).
#if defined(Q_OS_WIN)
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
#else
    Q_UNUSED(path);
#endif
}
