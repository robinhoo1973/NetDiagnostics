#include "Diagnostics/Model/GHelpers.h"
namespace SystemDiagnostics {
DiagnosticResult proxySettings(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Proxy Configuration (table mode):"));
    out.append(QString());

    static const QVector<DiagnosticFormatter::ColSpec> kProxyCols = {
        {"Variable", 16, false},
        {"Value",     0, false},
    };
    QList<QStringList> proxyRows;

#if defined(_WIN32)
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG cfg = {};
    if (WinHttpGetIEProxyConfigForCurrentUser(&cfg)) {
        if (cfg.lpszAutoConfigUrl) proxyRows.append({QStringLiteral("Auto Config URL"), QString::fromWCharArray(cfg.lpszAutoConfigUrl)});
        if (cfg.lpszProxy) proxyRows.append({QStringLiteral("HTTP Proxy"), QString::fromWCharArray(cfg.lpszProxy)});
        if (cfg.lpszProxyBypass) proxyRows.append({QStringLiteral("Bypass"), QString::fromWCharArray(cfg.lpszProxyBypass)});
        // 5WHY: WinHTTP docs require freeing ALL three pointers returned by
        // WinHttpGetIEProxyConfigForCurrentUser.  lpszAutoConfigUrl was leaked
        // on every run — a small but persistent Win32 heap leak.
        if (cfg.lpszAutoConfigUrl) GlobalFree(cfg.lpszAutoConfigUrl);
        if (cfg.lpszProxy) GlobalFree(cfg.lpszProxy);
        if (cfg.lpszProxyBypass) GlobalFree(cfg.lpszProxyBypass);
    }
#else
    const char* vars[] = {"HTTP_PROXY","HTTPS_PROXY","FTP_PROXY","NO_PROXY","http_proxy","https_proxy","no_proxy"};
    for (auto* v : vars) {
        const char* val = getenv(v);
        if (val && val[0])
            proxyRows.append({QString::fromLatin1(v), QString::fromLatin1(val)});
    }
#endif
    if (!proxyRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kProxyCols, proxyRows));
    else
        out.append(QStringLiteral("  No proxy configured"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Info;
    r.summary = QStringLiteral("Proxy Settings Collected");
    r.durationMs = 0;
    // Build structured r.data
    {
        bool hasProxy = false;
        for (const auto& row : proxyRows) {
            const QString key = row.value(0);
            const QString val = row.value(1);
            if (key.contains(QStringLiteral("Auto Config"), Qt::CaseInsensitive))
                r.data[QStringLiteral("autoConfigUrl")] = val;
            else if (key.compare(QStringLiteral("HTTP_PROXY"), Qt::CaseInsensitive) == 0
                     || key.compare(QStringLiteral("http_proxy"), Qt::CaseInsensitive) == 0
                     || key.compare(QStringLiteral("HTTP Proxy"), Qt::CaseInsensitive) == 0)
                r.data[QStringLiteral("httpProxy")] = val;
            else if (key.compare(QStringLiteral("HTTPS_PROXY"), Qt::CaseInsensitive) == 0
                     || key.compare(QStringLiteral("https_proxy"), Qt::CaseInsensitive) == 0)
                r.data[QStringLiteral("httpsProxy")] = val;
            else if (key.compare(QStringLiteral("FTP_PROXY"), Qt::CaseInsensitive) == 0)
                r.data[QStringLiteral("ftpProxy")] = val;
            else if (key.compare(QStringLiteral("NO_PROXY"), Qt::CaseInsensitive) == 0
                     || key.compare(QStringLiteral("no_proxy"), Qt::CaseInsensitive) == 0)
                r.data[QStringLiteral("noProxy")] = val;
            else if (key.compare(QStringLiteral("Bypass"), Qt::CaseInsensitive) == 0)
                r.data[QStringLiteral("noProxy")] = val;
            hasProxy = true;
        }
        r.data[QStringLiteral("hasProxy")] = hasProxy;
    }
    return r;
}

// 闁冲厜鍋撻柍鍏夊亾 G3 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾

}
