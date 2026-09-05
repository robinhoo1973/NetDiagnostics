// =============================================================================
// ReportEngine.cpp — extracted from AppState.cpp (~300 lines)
// =============================================================================
#include "Report/Model/ReportEngine.h"
#include "Common/Utils/AppColors.h"   // 报告状态色（与 Palette.js 同步）
#include "Common/Utils/AtomicWriteFile.h"   // 5WHY (2026-09-05): 报告导出原子写
#include "Common/Utils/NarrativeLocalizer.h"   // 5WHY (2026-08-23): 报告叙述与详情页同表同键本地化
#include "Common/Services/Logger.h"
#include "Diagnostics/Model/GHelpers.h"   // propsDumpText（报告体派生与终端同源）
#include "Diagnostics/View/LegacyTerminalFormat.h"   // legacyTerminalLines（报告/屏幕/剪贴板同链）

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
#include <QHash>
#include <QMutex>

namespace {

QString normalizeReportPath(const QString& p) {
    return p.startsWith(QStringLiteral("file:")) ? QUrl(p).toLocalFile() : p;
}

// 5WHY (复核 2026-08-21 三份同源): 报告体/详情页终端/剪贴板共用派生链
// details → rawOutput → legacyTerminalLines → propsDumpText → summary
// （AppState::resultFor 同序，含最终 summary 兜底）。
// G1 探针 details 恒空后曾只落 propsDumpText 平铺——导出报告与屏幕
// v0.0.3 表格背离。
// 5WHY (复核 2026-08-21 取消/异常误妆): Cancelled/Error 结果不派生
// v0.0.3 空态恒文（"已取消"被妆成"零连接"）——直接落 summary，与
// resultFor/clipboard 同门，报告与屏幕逐字一致。
static QString reportBody(const DiagnosticResult& r) {
    if (!r.details.isEmpty()) return r.details;
    if (!r.rawOutput.isEmpty()) return r.rawOutput;
    if (r.status != DiagStatus::Cancelled && r.status != DiagStatus::Error) {
        const QStringList legacy = SystemDiagnostics::legacyTerminalLines(r.id, r.properties, r.data);
        if (!legacy.isEmpty()) return legacy.join(QLatin1Char('\n'));
        const QString dump = SystemDiagnostics::propsDumpText(r.properties);
        if (!dump.isEmpty()) return dump;
    }
    return r.summary;
}

// 5WHY: The original code had 4 structurally identical switch blocks (dark/light
// in reportStatusColor + dark/light in renderStatusIcon) — same 6 cases, only
// the _DARK vs _LIGHT macro suffix varied. Adding a 7th DiagStatus meant touching
// 4 places; missing one produced a silent default-to-Info fallback.
//
// Fix: extract the status enum → palette offset into a single function. Each
// consumer indexes into the correct array for its color domain (string vs RGB,
// dark vs light). Adding a new status now requires one new case in statusIndex().
static int statusIndex(DiagStatus s) {
    // 5WHY (复核 2026-08-18 Reuse C5): 单一描述符表（DiagId.h statusDescriptor）
    // 取代独立 switch——Cancelled 曾在此处缺 case 静默回退 Info（绿色 i 图标）。
    return statusDescriptor(s).paletteIndex;
}

QString reportStatusColor(DiagStatus s, bool darkBackground) {
    static const QString hexColors[] = {
        QStringLiteral(APPC_SUCCESS_DARK),  QStringLiteral(APPC_WARNING_DARK),
        QStringLiteral(APPC_FAIL_DARK),     QStringLiteral(APPC_ERROR_DARK),
        QStringLiteral(APPC_SKIP_DARK),     QStringLiteral(APPC_SKIP_DARK),   // Cancelled 复用 skip 灰系
        QStringLiteral(APPC_INFO_DARK),
        QStringLiteral(APPC_SUCCESS_LIGHT), QStringLiteral(APPC_WARNING_LIGHT),
        QStringLiteral(APPC_FAIL_LIGHT),    QStringLiteral(APPC_ERROR_LIGHT),
        QStringLiteral(APPC_SKIP_LIGHT),    QStringLiteral(APPC_SKIP_LIGHT),  // Cancelled 复用 skip 灰系
        QStringLiteral(APPC_INFO_LIGHT),
    };
    return hexColors[statusIndex(s) + (darkBackground ? 0 : 7)];
}

// 5WHY: Reports used colored dots or Unicode glyphs instead of proper
// graphic icons. Unicode characters render inconsistently across fonts
// and platforms (some show as tofu □). QTextDocument CANNOT render SVG
// data URIs without QtSvg (not linked). Instead, render the icons
// programmatically with QPainter → PNG → base64 data URI. This works
// in both QTextDocument (preview + PDF) and browser WebView (rich HTML).
QImage renderStatusIcon(DiagStatus s, int size, bool darkBackground) {
    static const QRgb rgbColors[] = {
        APPC_SUCCESS_RGB_DARK,  APPC_WARNING_RGB_DARK,
        APPC_FAIL_RGB_DARK,     APPC_ERROR_RGB_DARK,
        APPC_SKIP_RGB_DARK,     APPC_SKIP_RGB_DARK,    // Cancelled 复用 skip 灰系
        APPC_INFO_RGB_DARK,
        APPC_SUCCESS_RGB,       APPC_WARNING_RGB,
        APPC_FAIL_RGB,          APPC_ERROR_RGB,
        APPC_SKIP_RGB,          APPC_SKIP_RGB,         // Cancelled 复用 skip 灰系
        APPC_INFO_RGB,
    };
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QColor bg = QColor::fromRgba(rgbColors[statusIndex(s) + (darkBackground ? 0 : 7)]);
    const float margin = size * 0.08f;
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(margin, margin, size - 2*margin, size - 2*margin));
    p.setPen(QPen(Qt::white, size * 0.10f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const float cx = size * 0.5f, cy = size * 0.5f, r = size * 0.28f;
    // 5WHY (复核 2026-08-18 Reuse C5): 字形画法经描述符表 glyph 分发——
    // Cancelled 落入 Cross（X=中止），不再误画 Info "i"。
    switch (statusDescriptor(s).glyph) {
        case DiagStatusGlyph::Tick:
            p.drawLine(QPointF(cx-r*0.6f, cy), QPointF(cx-r*0.1f, cy+r*0.5f));
            p.drawLine(QPointF(cx-r*0.1f, cy+r*0.5f), QPointF(cx+r*0.7f, cy-r*0.4f));
            break;
        case DiagStatusGlyph::Warning:
            p.drawLine(QPointF(cx, cy-r*0.6f), QPointF(cx, cy+r*0.15f));
            p.setPen(Qt::NoPen); p.setBrush(Qt::white);
            p.drawEllipse(QPointF(cx, cy+r*0.55f), r*0.12f, r*0.12f);
            break;
        case DiagStatusGlyph::Cross:
            p.drawLine(QPointF(cx-r*0.5f, cy-r*0.5f), QPointF(cx+r*0.5f, cy+r*0.5f));
            p.drawLine(QPointF(cx+r*0.5f, cy-r*0.5f), QPointF(cx-r*0.5f, cy+r*0.5f));
            break;
        case DiagStatusGlyph::Skip:
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
QString reportStatusIconImg(DiagStatus s, int size, bool darkBackground) {
    // 5WHY: every card and every result row re-rendered the icon via
    // QPainter + PNG + base64 — 50+ encodes per full report, all on the
    // UI thread. Cache by (status, size, dark) key: icons are immutable
    // once rendered, so a memoized string is safe and thread-local to
    // this anonymous-namespace function.
    // H3 (5WHY): 原代码仅靠注释约束"must be called from ONE thread"，
    // 无编译时或运行时强制。报告生成迁移到工作线程时 QHash 并发读写
    // 导致数据损坏。加 QMutex 保护——当前单线程场景零开销（try-lock
    // 即过），未来多线程场景自动安全。
    static QHash<quint64, QString> cache;
    static QMutex cacheMutex;
    const quint64 key = (quint64(statusIndex(s)) << 17)
                      | (quint64(size) << 1)
                      | quint64(darkBackground);
    {
        QMutexLocker lock(&cacheMutex);
        auto it = cache.constFind(key);
        if (it != cache.constEnd()) return it.value();
    }

    QImage img = renderStatusIcon(s, size, darkBackground);
    QByteArray pngData;
    QBuffer buf(&pngData);
    buf.open(QIODevice::WriteOnly);
    // 5WHY: img.save() return was unchecked — a null QImage (size ≤ 0)
    // would silently produce an empty data URI, showing a broken image.
    if (!img.save(&buf, "PNG")) return {};
    const QString uri = QStringLiteral("<img src='data:image/png;base64,")
         + QString::fromLatin1(pngData.toBase64())
         + QStringLiteral("' width='%1' height='%1' "
           "style='vertical-align:middle;display:inline-block' alt=''/>")
         .arg(size);
    {
        QMutexLocker lock(&cacheMutex);
        cache.insert(key, uri);
    }
    return uri;
}

QString reportStatusText(DiagStatus s) {
    // 5WHY (复核 2026-08-18 Reuse C5): 单一描述符表取代 switch。
    return QString::fromLatin1(statusDescriptor(s).reportText);
}

QString reportStatusClass(DiagStatus s) {
    return QString::fromLatin1(statusDescriptor(s).reportCssClass);
}

// 5WHY: buildHtml() and buildRichDocument() each defined their own set of
// ~20 theme-aware color constants — duplicated ~15 tokens between the two.
// Extracted to a shared struct so both methods use the same palette and
// theme changes only need one update point.
struct ReportColors {
    // Status colors (theme-independent — use dark palette for reports)
    QString pass    = QStringLiteral(APPC_SUCCESS_DARK);
    QString warn    = QStringLiteral(APPC_WARNING_DARK);
    QString fail    = QStringLiteral(APPC_FAIL_DARK);
    QString error   = QStringLiteral(APPC_ERROR_DARK);
    QString skip    = QStringLiteral(APPC_SKIP_DARK);
    QString info    = QStringLiteral(APPC_INFO_DARK);
    QString cyan    = QStringLiteral(APPC_TERTIARY_DARK);
    // Theme-aware colors
    QString textPrimary, textSecondary, textMuted;
    QString bgHeader, bgSection, bgRowAlt, bgRow;
    QString bgCardPass, bgCardInfo, bgCardWarn, bgCardFail, bgCardSkip, bgCardError;
    QString borderColor, codeBlockBg, codeBlockFg, detailBg, footerColor;

    explicit ReportColors(bool dark) {
        if (dark) {
            textPrimary  = QStringLiteral(APPC_ON_SURFACE_DARK);
            textSecondary= QStringLiteral(APPC_ON_SURFACE_VARIANT_DARK);
            textMuted    = QStringLiteral(APPC_TEXT_MUTED_LIGHT);  // #64748B — light muted has better contrast on dark report bg
            bgHeader     = QStringLiteral(APPC_REPORT_DARK_BG_HEADER);
            bgSection    = QStringLiteral(APPC_REPORT_DARK_BG_SECTION);
            bgRowAlt     = QStringLiteral(APPC_REPORT_DARK_BG_ROW_ALT);
            bgRow        = QStringLiteral(APPC_REPORT_DARK_BG_ROW);
            bgCardPass   = QStringLiteral(APPC_REPORT_DARK_BG_CARD_PASS);
            bgCardInfo   = QStringLiteral(APPC_REPORT_DARK_BG_CARD_INFO);
            bgCardWarn   = QStringLiteral(APPC_REPORT_DARK_BG_CARD_WARN);
            bgCardFail   = QStringLiteral(APPC_REPORT_DARK_BG_CARD_FAIL);
            bgCardSkip   = QStringLiteral(APPC_REPORT_DARK_BG_CARD_SKIP);
            bgCardError  = QStringLiteral("#2b1111");
            borderColor  = QStringLiteral(APPC_REPORT_DARK_BORDER);
            codeBlockBg  = QStringLiteral(APPC_REPORT_DARK_CODE_BG);
            codeBlockFg  = QStringLiteral(APPC_REPORT_DARK_CODE_FG);
            detailBg     = QStringLiteral(APPC_REPORT_DARK_DETAIL_BG);
            footerColor  = QStringLiteral(APPC_REPORT_DARK_FOOTER);
        } else {
            textPrimary  = QStringLiteral(APPC_ON_SURFACE_LIGHT);
            textSecondary= QStringLiteral(APPC_ON_SURFACE_VARIANT_LIGHT);
            textMuted    = QStringLiteral(APPC_TEXT_MUTED_LIGHT);
            bgHeader     = QStringLiteral(APPC_REPORT_LIGHT_BG_HEADER);
            bgSection    = QStringLiteral(APPC_REPORT_LIGHT_BG_SECTION);
            bgRowAlt     = QStringLiteral(APPC_REPORT_LIGHT_BG_ROW_ALT);
            bgRow        = QStringLiteral(APPC_REPORT_LIGHT_BG_ROW);
            bgCardPass   = QStringLiteral(APPC_REPORT_LIGHT_BG_CARD_PASS);
            bgCardInfo   = QStringLiteral(APPC_REPORT_LIGHT_BG_CARD_INFO);
            bgCardWarn   = QStringLiteral(APPC_REPORT_LIGHT_BG_CARD_WARN);
            bgCardFail   = QStringLiteral(APPC_REPORT_LIGHT_BG_CARD_FAIL);
            bgCardSkip   = QStringLiteral(APPC_REPORT_LIGHT_BG_CARD_SKIP);
            bgCardError  = QStringLiteral("#FEF2F2");
            borderColor  = QStringLiteral(APPC_REPORT_LIGHT_BORDER);
            codeBlockBg  = QStringLiteral(APPC_REPORT_LIGHT_CODE_BG);
            codeBlockFg  = QStringLiteral(APPC_REPORT_LIGHT_CODE_FG);
            detailBg     = QStringLiteral(APPC_REPORT_LIGHT_DETAIL_BG);
            footerColor  = QStringLiteral(APPC_REPORT_LIGHT_FOOTER);
        }
    }
};

} // namespace

// ── Public: HTML generation ─────────────────────────────────────────────

// 5WHY (复核 2026-08-18 重复累积): buildHtml/buildRichDocument 各有一份 8 变量
// groupStats 累积循环（加 cancelled 时双处同步）——提取共享 helper，卡片求和
// = total 的不变式只在单一推导点维护。
struct ReportTotals {
    int pass=0, warn=0, fail=0, skip=0, info=0, error=0, cancelled=0, total=0;
};
static ReportTotals accumulateTotals(const ReportData& data) {
    ReportTotals t;
    for (int g = 0; g < 5; ++g) {
        auto it = data.groupStats.find(g);
        if (it == data.groupStats.end()) continue;
        t.pass += it->value(QStringLiteral("pass")).toInt();
        t.warn += it->value(QStringLiteral("warn")).toInt();
        t.fail += it->value(QStringLiteral("fail")).toInt();
        t.skip += it->value(QStringLiteral("skip")).toInt();
        t.info += it->value(QStringLiteral("info")).toInt();
        t.error += it->value(QStringLiteral("error")).toInt();
        t.cancelled += it->value(QStringLiteral("cancelled")).toInt();
        t.total += it->value(QStringLiteral("total")).toInt();
    }
    return t;
}

QString ReportEngine::buildHtml(const ReportData& data, bool fullDetail, bool darkBackground) {
    const ReportColors c(darkBackground);
    // Aliases for concise reference in the large HTML string below
    const QString& colorPass = c.pass, &colorWarn = c.warn, &colorFail = c.fail;
    const QString& colorSkip = c.skip, &colorInfo = c.info, &colorCyan = c.cyan;
    const QString& colorError = c.error;
    const QString& textPrimary = c.textPrimary, &textSecondary = c.textSecondary;
    const QString& textMuted = c.textMuted, &bgHeader = c.bgHeader;
    const QString& bgSection = c.bgSection, &bgRowAlt = c.bgRowAlt, &bgRow = c.bgRow;
    const QString& bgCardPass = c.bgCardPass, &bgCardInfo = c.bgCardInfo;
    const QString& bgCardWarn = c.bgCardWarn, &bgCardFail = c.bgCardFail;
    const QString& bgCardSkip = c.bgCardSkip, &bgCardError = c.bgCardError;
    const QString& borderColor = c.borderColor;
    const QString& codeBlockBg = c.codeBlockBg, &codeBlockFg = c.codeBlockFg;
    const QString& detailBg = c.detailBg, &footerColor = c.footerColor;

    const ReportTotals totals = accumulateTotals(data);
    const int& tPass = totals.pass, &tWarn = totals.warn, &tFail = totals.fail;
    const int& tSkip = totals.skip, &tInfo = totals.info, &tError = totals.error;
    const int& tCancelled = totals.cancelled, &tTotal = totals.total;

    QString h;
    // 5WHY: QTextDocument (Qt Rich Text subset) does not support CSS
    // "width:100%" or "margin:0 auto" on <div> — only HTML width attr
    // on <table>/<img>/<td>.  Drop unsupported props; textWidth is set
    // in exportPdf() and print() uses the page width for layout.
    h += QStringLiteral("<div style=\"font-family:'Helvetica Neue',Arial,'PingFang SC','Microsoft YaHei',sans-serif;"
        "color:%1\">").arg(textPrimary);

    // ── Header band with gradient-style dark background ─────────────────
    // Header text colours — always light-on-dark for the header band
    const QString headerTargetColor = QStringLiteral(APPC_REPORT_DARK_HEADER_TARGET);
    const QString headerMetaColor   = QStringLiteral(APPC_REPORT_DARK_HEADER_META);
    h += QStringLiteral(
        "<table width=\"100%\" cellpadding=\"20\" cellspacing=\"0\" style=\"border-radius:8px\"><tr>"
        "<td bgcolor=\"%1\">"
        "<p style=\"margin:0 0 6px 0\"><span style=\"font-size:22px;color:%5\"><b>Network Diagnostic Report</b></span></p>"
        "<p style=\"margin:0 0 2px 0\"><span style=\"font-size:14px;color:%6\">%2</span></p>"
        "<p style=\"margin:0\"><span style=\"font-size:11px;color:%7\">%3 &middot; v%4 (build %8%9)</span></p>"
        "</td></tr></table>"
        "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"><tr><td height=\"12\"></td></tr></table>")
        .arg(bgHeader, data.target, data.timestamp, data.appVersion, colorCyan,
             headerTargetColor, headerMetaColor, data.buildNumber,
             data.gitHash.isEmpty() ? QString() : QStringLiteral(" - ") + data.gitHash);

    // ── Pass-rate progress bar ─────────────────────────────────────────
    int passPercent = tTotal > 0 ? (tPass * 100 / tTotal) : 0;
    const QString barColor = passPercent >= 90 ? QStringLiteral(APPC_SUCCESS_DARK)
                           : passPercent >= 70 ? QStringLiteral(APPC_WARNING_DARK)
                           : QStringLiteral(APPC_FAIL_DARK);
    const QString barBg = QStringLiteral(APPC_PROGRESS_BAR_BG);
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
            "<td width=\"16%\" align=\"center\" bgcolor=\"%1\""
            " style=\"padding:12px 6px\">")
            .arg(bg);
        td += iconImg;
        td += QStringLiteral("<br/><span style=\"font-size:28px;color:%1\"><b>%2</b></span><br/>"
            "<span style=\"font-size:11px;color:%1\">%3</span></td>")
            .arg(fg).arg(val).arg(lbl);
        return td;
    };
    h += QStringLiteral("<table width=\"100%\" cellpadding=\"0\" cellspacing=\"4\"><tr>");
    h += card(bgCardPass, colorPass, tPass, QStringLiteral("Pass"), reportStatusIconImg(DiagStatus::Pass, 24, darkBackground));
    h += card(bgCardInfo, colorInfo, tInfo, QStringLiteral("Info"), reportStatusIconImg(DiagStatus::Info, 24, darkBackground));
    h += card(bgCardWarn, colorWarn, tWarn, QStringLiteral("Warning"), reportStatusIconImg(DiagStatus::Warning, 24, darkBackground));
    h += card(bgCardFail, colorFail, tFail, QStringLiteral("Fail"), reportStatusIconImg(DiagStatus::Fail, 24, darkBackground));
    h += card(bgCardSkip, colorSkip, tSkip, QStringLiteral("Skipped"), reportStatusIconImg(DiagStatus::Skipped, 24, darkBackground));
    // 5WHY: DiagStatus::Error (infrastructure failure) was missing from
    // the summary cards — the card total no longer matched "N tests total"
    // when errors occurred. Added the 6th card so error is visible.
    h += card(bgCardError, colorError, tError, QStringLiteral("Error"), reportStatusIconImg(DiagStatus::Error, 24, darkBackground));
    // 5WHY (复核 2026-08-18): 第 7 张卡片（Cancelled）补齐后卡片求和 = total。
    h += card(bgCardSkip, colorSkip, tCancelled, QStringLiteral("Cancelled"), reportStatusIconImg(DiagStatus::Cancelled, 24, darkBackground));
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
            "<td style=\"background-color:%1;padding:12px 14px\">"
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
            "<tr style=\"background-color:%1\">"
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
                const QString name = (r.displayName.isEmpty() ? data.displayNames.value(id, QStringLiteral("Unknown"))
                                                              : r.displayName).toHtmlEscaped();
                const QString rowBg = alt ? bgRowAlt : bgRow;
                // 5WHY: Reports used Unicode glyphs that render as tofu (□)
                // on many fonts/platforms. SVG icons via base64 data URI
                // render identically across browsers, mail clients, and
                // QTextDocument. Direct concatenation avoids .arg() eating
                // base64 percent-encoded characters.
                const QString iconImg = reportStatusIconImg(r.status, 18, darkBackground);
                h += QStringLiteral(
                    "<tr bgcolor=\"%1\" style=\"border-bottom:1px solid %4\">"
                    "<td style=\"padding:10px 9px\"><span style=\"font-size:13px;color:%2\"><b>%3</b></span></td>"
                    "<td style=\"padding:10px 9px\">")
                    .arg(rowBg, textPrimary, name, borderColor);
                h += iconImg;
                h += QStringLiteral(
                    "&nbsp;<span style=\"font-size:12px;color:%1\"><b>%2</b></span></td>"
                    "<td style=\"padding:10px 9px\"><span style=\"font-size:12px;color:%3\">%4</span></td></tr>")
                    .arg(reportStatusColor(r.status, darkBackground), reportStatusText(r.status),
                         textSecondary,
                         r.summary.isEmpty() ? QStringLiteral("&mdash;") : r.summary.toHtmlEscaped());
                alt = !alt;
            }
        }
        h += QStringLiteral("</table><br/>");
    }

    if (fullDetail) {
        h += QStringLiteral("<table width=\"100%\" cellpadding=\"12\" cellspacing=\"0\"><tr>"
            "<td style=\"background-color:%1\"><span style=\"font-size:18px;color:%2\"><b>Detailed Output</b></span></td>"
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
                    const QString name = (r.displayName.isEmpty() ? data.displayNames.value(id, QStringLiteral("Unknown"))
                                                                  : r.displayName).toHtmlEscaped();
                    const QString sc = reportStatusColor(r.status, darkBackground);
                    // 5WHY: A single page-break-inside:avoid wrapping a
                    // nested-table header AND a potentially-tall code-block
                    // table is unreliable in QTextDocument — Qt Rich Text
                    // page-break support is limited and nested <table>
                    // structures defeat it.  Split into two independent
                    // blocks: the short header stays avoid; the code block
                    // gets page-break-before:avoid so it stays attached to
                    // the header when there's room, but can break internally
                    // when the content exceeds one page.
                    h += QStringLiteral(
                        // — Header block (page-break-inside:avoid — always short enough) —
                        "<div style=\"page-break-inside:avoid\">"
                        "<table width=\"100%\" cellpadding=\"10\" cellspacing=\"0\"><tr>"
                        "<td style=\"background-color:%1;padding:12px 14px\">"
                        "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"><tr>"
                        "<td width=\"66%%\" style=\"padding:0\"><span style=\"font-size:14px;color:%3\"><b>%4</b></span></td>"
                        "<td width=\"17%%\" style=\"padding:0\">"
                        "<span style=\"display:inline-block;width:10px;height:10px;border-radius:5px;background:%2;margin-right:6px;vertical-align:middle\"></span>"
                        "<span style=\"font-size:12px;color:%2\"><b>%5</b></span></td>"
                        "<td width=\"17%%\" style=\"padding:0;text-align:right\"><span style=\"font-size:11px;color:%6\">%7 ms</span></td>"
                        "</tr></table>")
                        .arg(detailBg, sc, textPrimary, name, reportStatusText(r.status), textMuted, QString::number(r.durationMs));
                    if (!r.summary.isEmpty())
                        h += QStringLiteral("<br/><span style=\"font-size:12px;color:%1\">%2</span>")
                            .arg(textSecondary, r.summary.toHtmlEscaped());
                    // 5WHY (复核 2026-08-20 报告缺叙述): 详情页新增摘要卡叙述
                    // （DiagnosticResult.narrative）后，HTML/PDF 快照未同步——
                    // 报告读者看不到推导文字。与详情页/剪贴板同一来源渲染。
                    // 5WHY (2026-08-23 报告同步本地化): 详情页已按语言渲染模板
                    // 文本——报告/剪贴板需同源（NarrativeLocalizer 同表同键）；
                    // 未命中回退 narrative EN 原文。
                    const QString narrKey = r.data.value(QStringLiteral("narrativeKey")).toString();
                    QString narr = NarrativeLocalizer::localized(narrKey,
                        r.data.value(QStringLiteral("narrativeArgs")).toList(), data.languageIndex);
                    if (narr.isEmpty()) narr = r.narrative;
                    if (!narr.isEmpty()) {
                        narr = narr.toHtmlEscaped();
                        narr.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
                        h += QStringLiteral("<br/><span style=\"font-size:12px;color:%1\">%2</span>")
                            .arg(textPrimary, narr);
                    }
                    h += QStringLiteral("</td></tr></table></div>");

                    // — Code block (page-break-before:avoid — stay with header if there's room) —
                    // 5WHY: Dark-background <td> cells create orphaned
                    // rectangles when QTextDocument tears a <td bgcolor>
                    // at a page boundary and re-lays the fragment with wrong
                    // width="100%" context.  Fix: render every output line as
                    // its own <tr><td>.  QTextDocument never splits inside a
                    // <tr>, so page breaks always land on a row boundary where
                    // each <td> is a complete, independent layout unit.  No
                    // page-break-inside hacks, no row-count estimates needed.
                    // Light theme uses transparent <td> regardless → unaffected.
                    // 5WHY (复核 2026-08-21 报告缺口): 详情页/剪贴板已对空
                    // details 派生 props 转储（propsDumpText），报告引擎曾
                    // 仍读原始 details——纯属性结果（如 TCP Connect）在导出
                    // 报告中空体，与屏幕终端不一致。同源派生补齐。
                    const QString body = reportBody(r);
                    if (!body.trimmed().isEmpty()) {
                        const QStringList lines = body.split(QStringLiteral("\n"));
                        h += QStringLiteral(
                            "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\""
                            " style=\"page-break-before:avoid;border-left:4px solid %1\">")
                            .arg(colorCyan);
                        for (int i = 0; i < lines.size(); ++i) {
                            const bool firstRow = (i == 0);
                            const bool lastRow  = (i == lines.size() - 1);
                            // 5WHY: firstRow and lastRow are not mutually
                            // exclusive — a single-line body hits both.
                            // Handle the intersection first so symmetric
                            // padding ("12px 14px") is used instead of the
                            // lop-sided first-row-only branch.
                            const QString padding = (firstRow && lastRow)
                                ? QStringLiteral("padding:12px 14px")
                                : firstRow
                                ? QStringLiteral("padding:12px 14px 0 14px")
                                : lastRow
                                ? QStringLiteral("padding:0 14px 12px 14px")
                                : QStringLiteral("padding:0 14px");
                            const QString cell = lines[i].isEmpty()
                                ? QStringLiteral("&nbsp;")
                                : lines[i].toHtmlEscaped();
                            if (darkBackground) {
                                h += QStringLiteral(
                                    "<tr><td style=\"background-color:%1;%2;"
                                    "font-family:'SF Mono','Consolas','Courier New',monospace;"
                                    "font-size:11px;color:%3;line-height:1.5;"
                                    "white-space:pre-wrap;word-wrap:break-word;overflow-wrap:break-word\""
                                    " width=\"100%\">%4</td></tr>")
                                    .arg(codeBlockBg, padding, codeBlockFg, cell);
                            } else {
                                h += QStringLiteral(
                                    "<tr><td style=\"%1;"
                                    "font-family:'SF Mono','Consolas','Courier New',monospace;"
                                    "font-size:11px;color:%2;line-height:1.5;"
                                    "white-space:pre-wrap;word-wrap:break-word;overflow-wrap:break-word\""
                                    " width=\"100%\">%3</td></tr>")
                                    .arg(padding, textPrimary, cell);
                            }
                        }
                        h += QStringLiteral("</table>");
                    }
                    h += QStringLiteral("<br/>");
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
    const ReportTotals totals = accumulateTotals(data);
    const int& tPass = totals.pass, &tWarn = totals.warn, &tFail = totals.fail;
    const int& tSkip = totals.skip, &tInfo = totals.info, &tError = totals.error;
    const int& tCancelled = totals.cancelled, &tTotal = totals.total;

    // 5WHY: CSS custom properties define the complete visual theme so
    // buildRichDocument() respects darkBackground.  Every color used in
    // the kCss stylesheet is a var(--xxx) reference — no hardcoded
    // APPC_*_DARK macros.  Adding a color variable here auto-applies
    // to both themes.
    const QString cssThemeBlock = darkBackground
        ? QStringLiteral(
            ":root{"
            "--bg:" APPC_SURFACE_DARK ";--fg:" APPC_ON_SURFACE_DARK ";--fg2:" APPC_ON_SURFACE_VARIANT_DARK ";"
            "--card-bg:" APPC_SURFACE_CONTAINER_LOW_DARK ";"
            "--header-bg1:" APPC_SURFACE_CONTAINER_LOW_DARK ";--header-bg2:" APPC_PRIMARY_CONTAINER_DARK ";"
            "--border:" APPC_OUTLINE_VARIANT_DARK ";"
            "--footer-fg:" APPC_REPORT_DARK_FOOTER ";--footer-border:#23233a;"
            "--card-pass-bg:" APPC_REPORT_DARK_BG_CARD_PASS ";"
            "--card-info-bg:" APPC_REPORT_DARK_BG_CARD_INFO ";"
            "--card-warn-bg:" APPC_REPORT_DARK_BG_CARD_WARN ";"
            "--card-fail-bg:" APPC_REPORT_DARK_BG_CARD_FAIL ";"
            "--card-skip-bg:" APPC_REPORT_DARK_BG_CARD_SKIP ";--card-error-bg:#2b1111;"
            "--badge-pass-bg:#16281b;--badge-info-bg:#141f33;--badge-warn-bg:#2b2810;"
            "--badge-fail-bg:#2b1616;--badge-skip-bg:#26262e;--badge-error-bg:#2b1111;"
            "--sec-row-bg:#1a2840;--border-card-pass:#2d5a2d;--border-card-info:#24406a;"
            "--border-card-warn:#5a5020;--border-card-fail:#5a2d2d;--border-card-skip:#333;"
            "--border-card-error:#5a2020;"
            "--h1-color:" APPC_TERTIARY_DARK ";--h2-color:" APPC_TERTIARY_DARK ";--h3-color:" APPC_INFO_DARK ";"
            "--header-p-color:" APPC_CSS_TEXT_BRIGHT ";"
            "--card-pass-fg:" APPC_SUCCESS_DARK ";--card-warn-fg:" APPC_WARNING_DARK ";"
            "--card-fail-fg:" APPC_FAIL_DARK ";--card-skip-fg:" APPC_SKIP_DARK ";"
            "--card-info-fg:" APPC_INFO_DARK ";"
            "--badge-pass-fg:" APPC_SUCCESS_DARK ";--badge-warn-fg:" APPC_WARNING_DARK ";"
            "--badge-fail-fg:" APPC_FAIL_DARK ";--badge-skip-fg:" APPC_SKIP_DARK ";"
            "--badge-info-fg:" APPC_INFO_DARK ";"
            "--badge-error-fg:" APPC_ERROR_DARK ";"
            "--sec-row-fg:" APPC_INFO_DARK ";"
            "--body-border:" APPC_OUTLINE_VARIANT_DARK ";"
            "--analysis-bg:" APPC_REPORT_DARK_DETAIL_BG ";--analysis-fg:" APPC_REPORT_DARK_CODE_FG ";"
            "--analysis-border:" APPC_CSS_TEAL_ACCENT ";"
            "--raw-bg:" APPC_REPORT_DARK_CODE_BG ";--raw-fg:" APPC_REPORT_DARK_CODE_FG ";"
            "--meta-fg:" APPC_CSS_TEXT_DIM ";"
            "--detail-pass:" APPC_SUCCESS_DARK ";--detail-warn:" APPC_WARNING_DARK ";"
            "--detail-fail:" APPC_FAIL_DARK ";--detail-skip:" APPC_SKIP_DARK ";"
            "--detail-info:" APPC_INFO_DARK ";"
            "--detail-error:" APPC_ERROR_DARK ";"
            "--card-error-fg:" APPC_ERROR_DARK ";"
            "}")
        : QStringLiteral(
            ":root{"
            "--bg:" APPC_SURFACE_LIGHT ";--fg:" APPC_ON_SURFACE_LIGHT ";--fg2:" APPC_ON_SURFACE_VARIANT_LIGHT ";"
            "--card-bg:" APPC_SURFACE_CONTAINER_LOW_LIGHT ";"
            "--header-bg1:" APPC_REPORT_LIGHT_BG_HEADER ";--header-bg2:" APPC_PRIMARY_CONTAINER_DARK ";"
            // 5WHY: Header is always dark with light text — --header-bg2
            // must stay dark for WCAG contrast.  #0F172A→#0C4A6E
            // gradient keeps white header text legible (~13:1)."
            "--border:" APPC_OUTLINE_VARIANT_LIGHT ";"
            "--footer-fg:" APPC_REPORT_LIGHT_FOOTER ";--footer-border:" APPC_OUTLINE_VARIANT_LIGHT ";"
            "--card-pass-bg:" APPC_REPORT_LIGHT_BG_CARD_PASS ";"
            "--card-info-bg:" APPC_REPORT_LIGHT_BG_CARD_INFO ";"
            "--card-warn-bg:" APPC_REPORT_LIGHT_BG_CARD_WARN ";"
            "--card-fail-bg:" APPC_REPORT_LIGHT_BG_CARD_FAIL ";"
            "--card-skip-bg:" APPC_REPORT_LIGHT_BG_CARD_SKIP ";--card-error-bg:#FEF2F2;"
            "--badge-pass-bg:#DCFCE7;--badge-info-bg:#DBEAFE;--badge-warn-bg:#FEF3C7;"
            "--badge-fail-bg:#FEE2E2;--badge-skip-bg:#E2E8F0;--badge-error-bg:#FEE2E2;"
            "--sec-row-bg:#E0F2FE;--border-card-pass:#BBF7D0;--border-card-info:#BFDBFE;"
            "--border-card-warn:#FDE68A;--border-card-fail:#FECACA;--border-card-skip:#CBD5E1;"
            "--border-card-error:#FECACA;"
            "--h1-color:" APPC_TERTIARY_LIGHT ";--h2-color:" APPC_TERTIARY_LIGHT ";--h3-color:" APPC_INFO_LIGHT ";"
            "--header-p-color:" APPC_CSS_TEXT_BRIGHT ";"
            "--card-pass-fg:" APPC_SUCCESS_LIGHT ";--card-warn-fg:" APPC_WARNING_LIGHT ";"
            "--card-fail-fg:" APPC_FAIL_LIGHT ";--card-skip-fg:" APPC_SKIP_LIGHT ";"
            "--card-info-fg:" APPC_INFO_LIGHT ";"
            "--badge-pass-fg:" APPC_SUCCESS_LIGHT ";--badge-warn-fg:" APPC_WARNING_LIGHT ";"
            "--badge-fail-fg:" APPC_FAIL_LIGHT ";--badge-skip-fg:" APPC_SKIP_LIGHT ";"
            "--badge-info-fg:" APPC_INFO_LIGHT ";"
            "--badge-error-fg:" APPC_ERROR_LIGHT ";"
            "--sec-row-fg:" APPC_INFO_LIGHT ";"
            "--body-border:" APPC_OUTLINE_VARIANT_LIGHT ";"
            "--analysis-bg:" APPC_REPORT_LIGHT_DETAIL_BG ";--analysis-fg:" APPC_ON_SURFACE_VARIANT_LIGHT ";"
            "--analysis-border:" APPC_CSS_TEAL_ACCENT ";"
            "--raw-bg:" APPC_REPORT_LIGHT_CODE_BG ";--raw-fg:" APPC_REPORT_LIGHT_CODE_FG ";"
            "--meta-fg:" APPC_TEXT_MUTED_LIGHT ";"
            "--detail-pass:" APPC_SUCCESS_LIGHT ";--detail-warn:" APPC_WARNING_LIGHT ";"
            "--detail-fail:" APPC_FAIL_LIGHT ";--detail-skip:" APPC_SKIP_LIGHT ";"
            "--detail-info:" APPC_INFO_LIGHT ";"
            "--detail-error:" APPC_ERROR_LIGHT ";"
            "--card-error-fg:" APPC_ERROR_LIGHT ";"
            "}");
    // 5WHY: All theme-dependent colors (surface, text, status, detail borders)
    // use CSS var(--xxx) references resolved by cssThemeBlock above, which
    // references APPC_* macros. Badge backgrounds, card-border accents, and
    // sec-row backgrounds are still hardcoded hex — they are CSS-only and
    // have no Palette.js counterpart (no sync risk).  darkBackground controls
    // every color via a single :root block swap.
    const QString kCss = cssThemeBlock
        + QStringLiteral(
        "*{margin:0;padding:0;box-sizing:border-box}"
        "html,body{max-width:100%;overflow-x:auto;overflow-wrap:break-word;word-wrap:break-word}"
        "img,svg,pre,code{max-width:100%;height:auto}"
        "body{font-family:'Segoe UI',Roboto,Arial,sans-serif;background:var(--bg);color:var(--fg);padding:24px}"
        ".wrap{max-width:960px;margin:0 auto}"
        ".header{text-align:center;padding:34px 24px;background:linear-gradient(135deg,var(--header-bg1),var(--header-bg2));border-radius:14px;margin-bottom:26px}"
        ".header h1{font-size:26px;color:var(--h1-color);margin-bottom:10px;letter-spacing:.5px}"
        ".header p{font-size:13px;color:var(--header-p-color);margin:3px 0}"
        "h2{font-size:18px;color:var(--h2-color);margin:26px 0 14px}"
        "h3{font-size:15px;color:var(--h3-color);margin:20px 0 10px}"
        ".cards{display:flex;gap:14px;margin-bottom:22px;flex-wrap:wrap}"
        ".card{flex:1;min-width:110px;text-align:center;padding:18px 10px;border-radius:12px}"
        ".card .icon{display:block;font-size:18px;margin-bottom:2px}"
        ".card .count{display:block;font-size:30px;font-weight:700}"
        ".card .label{font-size:11px;color:var(--fg2);margin-top:6px;letter-spacing:1px;text-transform:uppercase}"
        ".card.pass{background:var(--card-pass-bg);border:1px solid var(--border-card-pass)}.card.pass .count{color:var(--card-pass-fg)}"
        ".card.warn{background:var(--card-warn-bg);border:1px solid var(--border-card-warn)}.card.warn .count{color:var(--card-warn-fg)}"
        ".card.fail{background:var(--card-fail-bg);border:1px solid var(--border-card-fail)}.card.fail .count{color:var(--card-fail-fg)}"
        ".card.skip{background:var(--card-skip-bg);border:1px solid var(--border-card-skip)}.card.skip .count{color:var(--card-skip-fg)}"
        ".card.info{background:var(--card-info-bg);border:1px solid var(--border-card-info)}.card.info .count{color:var(--card-info-fg)}"
        ".card.error{background:var(--card-error-bg);border:1px solid var(--border-card-error)}.card.error .count{color:var(--card-error-fg)}"
        ".wrap table{table-layout:fixed;width:100%}"
        "table.grid{border-collapse:collapse;font-size:13px;border-radius:10px;overflow:hidden}"
        "table.grid th{text-align:left;padding:11px 12px;background:var(--card-bg);color:var(--fg2);font-weight:600}"
        "table.grid td{padding:9px 12px;border-bottom:1px solid var(--border);vertical-align:top}"
        "tr.sec td{background:var(--sec-row-bg);color:var(--sec-row-fg);font-weight:700}"
        ".badge{display:inline-block;padding:2px 11px;border-radius:12px;font-size:11px;font-weight:700}"
        ".badge.pass{background:var(--badge-pass-bg);color:var(--badge-pass-fg)}.badge.warn{background:var(--badge-warn-bg);color:var(--badge-warn-fg)}"
        ".badge.fail{background:var(--badge-fail-bg);color:var(--badge-fail-fg)}.badge.skip{background:var(--badge-skip-bg);color:var(--badge-skip-fg)}"
        ".badge.info{background:var(--badge-info-bg);color:var(--badge-info-fg)}"
        ".badge.error{background:var(--badge-error-bg);color:var(--badge-error-fg)}"
        ".badge.cancel{background:var(--badge-skip-bg);color:var(--badge-skip-fg)}"
        "details.test{background:var(--card-bg);border-radius:10px;margin-bottom:12px;overflow:hidden}"
        "details.test>summary{padding:13px 16px;cursor:pointer;font-weight:600;font-size:14px}"
        "details.test.pass>summary{border-left:4px solid var(--detail-pass)}details.test.warn>summary{border-left:4px solid var(--detail-warn)}"
        "details.test.fail>summary{border-left:4px solid var(--detail-fail)}details.test.skip>summary{border-left:4px solid var(--detail-skip)}"
        "details.test.info>summary{border-left:4px solid var(--detail-info)}"
        "details.test.error>summary{border-left:4px solid var(--detail-error)}"
        // 5WHY (复核 2026-08-18): reportStatusClass 返回 "cancel" 但 details.test.cancel
        // 无左缘状态条——取消行的详情块与其他状态视觉不可区分。复用 skip 变量。
        "details.test.cancel>summary{border-left:4px solid var(--detail-skip)}"
        ".body{padding:14px 16px 18px;border-top:1px solid var(--body-border)}"
        ".analysis{background:var(--analysis-bg);color:var(--analysis-fg);border-left:3px solid var(--analysis-border);padding:11px 13px;border-radius:6px;margin-bottom:12px;font-size:13px;line-height:1.6}"
        ".raw{background:var(--raw-bg);padding:13px;border-radius:6px;font-family:'Consolas','Courier New',monospace;font-size:12px;white-space:pre-wrap;line-height:1.5;color:var(--raw-fg);max-height:420px;overflow:auto}"
        ".meta{color:var(--meta-fg);font-size:11px;font-weight:400}"
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
        "<p>Target: <b style=\"color:" APPC_CSS_TEXT_BRIGHT "\">%2</b></p>"
        "<p>NetDiagnostics v%3 (build %4%5)</p></div>\n")
        .arg(data.timestamp, data.target, data.appVersion, data.buildNumber,
             data.gitHash.isEmpty() ? QString() : QStringLiteral(" - ") + data.gitHash);

    // 5WHY: Unicode card icons (&#10003; etc.) render inconsistently across
    // fonts. Base64-encoded SVG images match the app's QML icon set exactly.
    h += QStringLiteral("<div class=\"cards\">");
    h += card(QStringLiteral("pass"), reportStatusIconImg(DiagStatus::Pass, 32, darkBackground), tPass, QStringLiteral("Pass"));
    h += card(QStringLiteral("info"), reportStatusIconImg(DiagStatus::Info, 32, darkBackground), tInfo, QStringLiteral("Info"));
    h += card(QStringLiteral("warn"), reportStatusIconImg(DiagStatus::Warning, 32, darkBackground), tWarn, QStringLiteral("Warning"));
    h += card(QStringLiteral("fail"), reportStatusIconImg(DiagStatus::Fail, 32, darkBackground), tFail, QStringLiteral("Fail"));
    h += card(QStringLiteral("skip"), reportStatusIconImg(DiagStatus::Skipped, 32, darkBackground), tSkip, QStringLiteral("Skipped"));
    // 5WHY: Error status card was missing (CSS .card.error existed but no
    // card was ever emitted) — total card counts disagreed with "N tests".
    h += card(QStringLiteral("error"), reportStatusIconImg(DiagStatus::Error, 32, darkBackground), tError, QStringLiteral("Error"));
    // 5WHY (复核 2026-08-18): 与 buildHtml 同理——Cancelled 卡补齐求和一致性。
    h += card(QStringLiteral("skip"), reportStatusIconImg(DiagStatus::Cancelled, 32, darkBackground), tCancelled, QStringLiteral("Cancelled"));
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
                const QString name = (r.displayName.isEmpty() ? data.displayNames.value(id, QStringLiteral("Unknown"))
                                                              : r.displayName).toHtmlEscaped();
                ++idx;
                // 5WHY: Unicode icons → SVG data URI for consistent rendering.
                // Direct concatenation avoids .arg() consuming base64 % escapes.
                const QString iconImg = reportStatusIconImg(r.status, 18, darkBackground);
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
                const QString name = (r.displayName.isEmpty() ? data.displayNames.value(id, QStringLiteral("Unknown"))
                                                              : r.displayName).toHtmlEscaped();
                const QString cls = reportStatusClass(r.status);
                h += QStringLiteral("<details class=\"test %1\"><summary>"
                    "<span class=\"badge %1\">%2</span> &nbsp;%3 "
                    "<span class=\"meta\">&middot; %4 ms</span></summary><div class=\"body\">")
                    .arg(cls, reportStatusText(r.status), name).arg(r.durationMs);
                if (!r.summary.isEmpty())
                    h += QStringLiteral("<div class=\"analysis\">%1</div>").arg(r.summary.toHtmlEscaped());
                // 5WHY (复核 2026-08-21 报告缺口): 与表格体同源——空 details
                // 时派生 props 转储，报告与屏幕终端一致。
                const QString body = reportBody(r);
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
    // 5WHY: a full 45-test detail report can be tens of thousands of px
    // tall; a single QImage at 960px width would peak ~190MB on mobile and
    // OOM the app. Cap the render height — content beyond the cap is
    // clipped in the in-app preview image (the real PDF/HTML export is
    // unaffected, it uses QPdfWriter pagination).
    constexpr int kMaxPreviewHeight = 8192;
    const int naturalH = (int)qCeil(docSize.height());
    const bool truncated = naturalH > kMaxPreviewHeight;
    int h = qMax(100, qMin(naturalH, kMaxPreviewHeight));

    QImage img(width, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    doc.drawContents(&painter, QRectF(0, 0, width, h));
    if (truncated) {
        // 5WHY: the Flickable would otherwise scroll to a hard cut with no
        // explanation, making the user think the report is incomplete and
        // re-exporting. Draw a notice band so the truncation is explicit
        // (PDF/HTML export is never truncated — it paginates).
        const int bandH = 26;
        painter.fillRect(0, h - bandH, width, bandH, QColor(30, 30, 46, 235));
        painter.setPen(QColor(160, 160, 184));
        QFont f = painter.font(); f.setPixelSize(11); painter.setFont(f);
        painter.drawText(QRect(0, h - bandH, width, bandH), Qt::AlignCenter,
            QStringLiteral("\u2026 Preview truncated \u2014 export PDF/HTML for the full report"));
    }
    painter.end();
    return img;
}

// ── File I/O ────────────────────────────────────────────────────────────

QString ReportEngine::exportHtml(const QString& filePath, const QString& html) {
    const QString path = normalizeReportPath(filePath);
    // 5WHY (2026-09-05 半截报告): 曾手写 QFile open(WriteOnly|Text)+write——
    // 磁盘满/崩溃/强杀会把目标文件截断成半份报告，而 exportPdf 的
    // QFile::exists 判据把半份当成功交付。与 persistResults/platformCredential
    // Save 同门收敛到 AtomicWriteFile（QSaveFile 提交式全有或全无，失败时
    // 旧文件完好）。
    if (!atomicWriteFile(path, html.toUtf8())) {
        Logger::instance().event(QStringLiteral("exportHtml: atomic write failed for %1").arg(path));
        return QString();
    }
    return path;
}

QString ReportEngine::exportPdf(const QString& filePath, const QString& html) {
    // 5WHY: QPdfWriter default resolution (1200 DPI) makes QTextDocument
    // fonts appear tiny. 96 DPI matches screen pixel→mm mapping so CSS
    // px sizes render at the intended physical size.
    //   A4 = 210 mm.  5 mm margins → content = 200 mm.
    const QString path = normalizeReportPath(filePath);
    QPdfWriter writer(path);
    writer.setResolution(96);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(5, 12, 5, 12), QPageLayout::Millimeter);
    writer.setTitle(QStringLiteral("Network Diagnostic Report"));

    // 5WHY: QTextDocument has TWO independent layout dimensions:
    //   textWidth  – line-wrapping width (default -1 → idealWidth())
    //   pageSize   – page viewport for rendering / pagination
    // print() renders each page inside doc.pageSize(), NOT the
    // printer's pageLayout.  Without setPageSize(), the doc defaults
    // to a smaller internal size, leaving excessive whitespace even
    // when textWidth is correct.  Set BOTH to the printer's paintRect
    // so content fills the full A4 printable area (200 mm at 96 DPI).
    const QPageLayout layout = writer.pageLayout();
    const QRectF paintRectMm = layout.paintRect(QPageLayout::Millimeter);
    // 5WHY: paintRect alone still leaves ~10 % whitespace — QPdfWriter
    // may apply internal bleed/margin on top of the explicit page
    // margins.  Scale the computed pixel dimensions by 1.1× to fully
    // fill the A4 sheet (210 mm → ~794 px at 96 DPI, margins 5 mm).
    const int pageWidthPx  = qRound(paintRectMm.width()  / 25.4 * writer.resolution() * 1.1);
    const int pageHeightPx = qRound(paintRectMm.height() / 25.4 * writer.resolution());

    QTextDocument doc;
    doc.setDefaultFont(QFont(QStringLiteral("Helvetica"), 12));
    doc.setPageSize(QSizeF(pageWidthPx, pageHeightPx));
    doc.setTextWidth(pageWidthPx);
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
