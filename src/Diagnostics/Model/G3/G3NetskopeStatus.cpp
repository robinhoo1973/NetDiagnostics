#include "Diagnostics/Model/GHelpers.h"

namespace G1G2G3Native {
DiagnosticResult netskopeStatus(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Security Proxy Status:"));
    out.append(QString());

    bool found = false;
#if defined(_WIN32)
    // Check for nsproxy.exe process
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                QString name = QString::fromWCharArray(pe.szExeFile);
                if (name.contains("nsproxy", Qt::CaseInsensitive) || name.contains("zsproxy", Qt::CaseInsensitive) ||
                    name.contains("zscaler", Qt::CaseInsensitive) || name.contains("netskope", Qt::CaseInsensitive)) {
                    out.append(QStringLiteral("  Found: %1 (PID %2)").arg(name).arg(pe.th32ProcessID));
                    found = true;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
#else
    // Check /proc for nsproxy/netskope/zscaler processes
    QDir procDir(QStringLiteral("/proc"));
    for (const auto& fi : procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok; fi.fileName().toInt(&ok);
        if (!ok) continue; // not a PID directory
        QFile cmdLine(fi.absoluteFilePath() + "/comm");
        if (cmdLine.open(QIODevice::ReadOnly)) {
            QString comm = QString::fromLatin1(cmdLine.readAll().trimmed());
            if (comm.contains("nsproxy", Qt::CaseInsensitive) || comm.contains("zscaler", Qt::CaseInsensitive) ||
                comm.contains("netskope", Qt::CaseInsensitive) || comm.contains("zsproxy", Qt::CaseInsensitive)) {
                out.append(QStringLiteral("  Found: %1 (PID %2)").arg(comm, fi.fileName()));
                found = true;
            }
        }
    }
    if (!found) out.append(QStringLiteral("  No security proxy process detected"));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = found ? DiagStatus::Pass : DiagStatus::Info;
    r.summary = found ? QStringLiteral("Security proxy detected") : QStringLiteral("No security proxy detected");
    r.durationMs = 0;
    return r;
}

}
