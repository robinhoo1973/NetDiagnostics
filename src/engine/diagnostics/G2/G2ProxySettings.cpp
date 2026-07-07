#include "engine/diagnostics/GHelpers.h"
namespace G1G2G3Native {
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

#ifdef _WIN32
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG cfg = {};
    if (WinHttpGetIEProxyConfigForCurrentUser(&cfg)) {
        if (cfg.lpszProxy) proxyRows.append({QStringLiteral("HTTP Proxy"), QString::fromWCharArray(cfg.lpszProxy)});
        if (cfg.lpszProxyBypass) proxyRows.append({QStringLiteral("Bypass"), QString::fromWCharArray(cfg.lpszProxyBypass)});
        GlobalFree(cfg.lpszProxy); GlobalFree(cfg.lpszProxyBypass);
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
    r.summary = QStringLiteral("Proxy settings collected");
    r.durationMs = 0;
    return r;
}

// 闁冲厜鍋撻柍鍏夊亾 G3 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾

}
