#include "Diagnostics/Model/GHelpers.h"
#include <QProcess>

namespace SystemDiagnostics {
DiagnosticResult dnsCache(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    int cacheEntries = 0;
    bool hasCache = false;

    out.append(QString());

#if defined(_WIN32)
    out.append(QStringLiteral("Windows IP Configuration"));
    out.append(QString());
    out.append(QStringLiteral("DNS Client Cache (ipconfig /displaydns format)"));
    out.append(QStringLiteral("=============================================="));
    out.append(QString());
    {
        QProcess dnsProc;
        dnsProc.start(QStringLiteral("ipconfig"), QStringList() << QStringLiteral("/displaydns"));
        if (dnsProc.waitForFinished(10000)) {
            QString dnsOut = QString::fromLocal8Bit(dnsProc.readAllStandardOutput());
            if (!dnsOut.trimmed().isEmpty()) {
                hasCache = true;
                for (auto& line : dnsOut.split('\n')) {
                    QString t = line.trimmed();
                    if (t.isEmpty() || t.startsWith("Windows IP")) continue;
                    out.append(t);
                }
            } else {
                out.append(QStringLiteral("  (DNS cache is empty)"));
            }
        } else {
            out.append(QStringLiteral("  (Unable to retrieve DNS cache — timeout)"));
        }
    }
    out.append(QString());
    out.append(QStringLiteral("To flush: ipconfig /flushdns"));
#else
    out.append(QStringLiteral("DNS Cache Information"));
    out.append(QString());
    // 5WHY: /run/systemd/resolve/cache is a BINARY mmap'd database, not text.
    // Reading it as lines produced garbage entries and a spurious "Cache
    // Active" Pass.  Query systemd's official CLI and feed its text output
    // through the shared line-display logic below instead.
    // 5WHY (iOS CI failure 31132720900): resolvectl is a LINUX-ONLY (systemd)
    // command.  Guarding the QProcess use with __linux__ both avoids spawning a
    // nonexistent binary on macOS/iOS AND keeps the QProcess instantiation out
    // of the iOS compile — the iOS toolchain's transitive include chain failed
    // to declare QProcess here ("unknown type name"), even though the header was
    // included.  macOS/iOS now fall straight through to the resolver-config
    // branch, which is the correct behaviour (no systemd-resolved there).
#if defined(__linux__)
    QByteArray data;
    // 5WHY: `resolvectl statistics` needs the systemd-resolved
    // dump-statistics permission — on an interactive desktop a non-root user
    // gets a polkit password prompt and the diagnostic blocks on it
    // mid-run. ND_SKIP_RESOLVECTL=1 skips the privileged query (used by
    // headless test/CI runs and local validation); the resolver-config
    // branch below shows instead.
    if (qEnvironmentVariableIsSet("ND_SKIP_RESOLVECTL")) {
        data = QByteArray();
    } else {
        QProcess proc;
        proc.start(QStringLiteral("resolvectl"), QStringList() << QStringLiteral("statistics"));
        if (!proc.waitForFinished(5000)) {
            proc.start(QStringLiteral("systemd-resolve"), QStringList() << QStringLiteral("--statistics"));
            proc.waitForFinished(5000);
        }
        data = proc.readAllStandardOutput();
    }
#else
    // macOS / iOS / other non-Linux: no systemd-resolved exists — leave data
    // empty so the else branch below shows the resolver configuration.
    QByteArray data;
#endif
    if (!data.trimmed().isEmpty()) {
        hasCache = true;
        out.append(QStringLiteral("systemd-resolved Cache Statistics"));
        out.append(QStringLiteral("=============================================="));
        out.append(QString());
        if (data.size() > 0) {
            // resolvectl statistics lines are shown verbatim below (the old
            // code misparsed the BINARY cache file as text).
            QString text = QString::fromLatin1(data);
            QStringList entries = text.split('\n');
            for (const auto& line : entries) {
                QString trimmed = line.trimmed();
                if (trimmed.isEmpty()) {
                    out.append(QString());
                    continue;
                }
                // Parse: "hostname IN TYPE value" or "hostname IN TYPE ttl value"
                QStringList parts = trimmed.split(' ');
                if (parts.size() >= 4 && parts[1] == "IN") {
                    QString name = parts[0];
                    QString type = parts[2];
                    // Skip "IN" marker, extract TTL if present
                    QString dataPart;
                    int ttl = 0;
                    bool ok = false;
                    if (parts.size() >= 5) {
                        int val = parts[3].toInt(&ok);
                        if (ok && val > 0 && parts.size() >= 6) {
                            ttl = val;
                            dataPart = parts.mid(4).join(' ');
                        } else {
                            dataPart = parts.mid(3).join(' ');
                        }
                    }
                    // Show in ipconfig /displaydns style
                    cacheEntries++;
                    out.append(QStringLiteral("    %1").arg(name));
                    out.append(QStringLiteral("    ----------------------------------------"));
                    out.append(QStringLiteral("    Record Name . . . . . : %1").arg(name));
                    out.append(QStringLiteral("    Record Type . . . . . : %1").arg(type));
                    if (ttl > 0)
                        out.append(QStringLiteral("    Time To Live  . . . . : %1").arg(ttl));
                    out.append(QStringLiteral("    Data . . . . . . . . : %1").arg(dataPart));
                } else {
                    // Unparsed line 闁?show as-is
                    out.append(QStringLiteral("    %1").arg(trimmed));
                }
            }
        } else {
            out.append(QStringLiteral("    (cache is empty)"));
        }
    } else {
        // 闁冲厜鍋撻柍鍏夊亾 No systemd-resolved 闁?check and show resolution setup 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
        out.append(QStringLiteral("DNS Resolution Configuration"));
        out.append(QStringLiteral("=============================================="));
        out.append(QString());

        // Check for nscd
        if (QFile::exists(QStringLiteral("/var/db/nscd/hosts")))
            out.append(QStringLiteral("    nscd: active (hosts cache at /var/db/nscd/hosts)"));
        else if (QFile::exists(QStringLiteral("/var/cache/nscd/hosts")))
            out.append(QStringLiteral("    nscd: active (hosts cache at /var/cache/nscd/hosts)"));

        // Check for dnsmasq
        if (QFile::exists(QStringLiteral("/var/lib/misc/dnsmasq.leases")))
            out.append(QStringLiteral("    dnsmasq: active (leases at /var/lib/misc/dnsmasq.leases)"));

        // Show resolv.conf as the "current resolver" info
        QFile resolv(QStringLiteral("/etc/resolv.conf"));
        if (resolv.open(QIODevice::ReadOnly)) {
            QTextStream ts(&resolv);
            while (!ts.atEnd()) {
                QString line = ts.readLine().trimmed();
                if (line.isEmpty() || line.startsWith('#')) continue;
                if (line.startsWith("nameserver "))
                    out.append(QStringLiteral("    Nameserver . . . . . . . . : %1").arg(line.mid(11)));
                else if (line.startsWith("search "))
                    out.append(QStringLiteral("    DNS Suffix Search List. . : %1").arg(line.mid(7)));
                else if (line.startsWith("domain "))
                    out.append(QStringLiteral("    Connection-specific DNS . . : %1").arg(line.mid(7)));
                else if (line.startsWith("options "))
                    out.append(QStringLiteral("    Options . . . . . . . . . : %1").arg(line.mid(8)));
            }
        }

        // Show hosts file summary
        QFile hosts(QStringLiteral("/etc/hosts"));
        int hostEntryCount = 0;
        if (hosts.open(QIODevice::ReadOnly)) {
            QTextStream ts(&hosts);
            while (!ts.atEnd()) {
                QString line = ts.readLine().trimmed();
                if (!line.isEmpty() && !line.startsWith('#') && line.contains(' '))
                    hostEntryCount++;
            }
        }
        if (hostEntryCount > 0)
            out.append(QStringLiteral("    /etc/hosts entries . . . . : %1 static mappings").arg(hostEntryCount));
    }
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = hasCache ? DiagStatus::Pass : DiagStatus::Info;
    if (hasCache)
        // 5WHY: cacheEntries counted the old (broken) binary-file "IN" lines;
        // resolvectl statistics lines no longer set it.  Report honestly that
        // the stats were collected rather than a fabricated entry count.
        r.summary = QStringLiteral("Cache Active — see statistics above");
    else
        r.summary = QStringLiteral("No Local DNS Cache Detected");
    r.durationMs = (int)t.elapsed();
    return r;
}

}
