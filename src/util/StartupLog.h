// =============================================================================
// StartupLog.h — Crash diagnostic logging for Windows WIN32 builds (no console).
//
// Writes timestamped startup events to %TEMP%\NetDiagnostics_startup.log.
// Each run APPENDS to the file, so crash logs persist across launches.
//
// Usage:  STARTUP_LOG("QML engine loaded, rootObjects=%d", engine.rootObjects().size());
// =============================================================================
#pragma once

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

static void startup_log(const char* file, int line, const char* fmt, ...) {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString path = QDir(dir).filePath("NetDiagnostics_startup.log");

    QFile f(path);
    // Append mode — crash logs survive across launches
    f.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text);
    QTextStream ts(&f);

    QString tsStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    ts << "[" << tsStr << "] ";

    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ts << QString::fromUtf8(buf);
    // Only add file:line for non-trivial entries
    if (file && line > 0)
        ts << "  (" << file << ":" << line << ")";
    ts << "\n";
    ts.flush();
    f.close();

#ifdef _WIN32
    // Also emit to debugger if attached (DebugView / Visual Studio)
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
#endif
}

#define STARTUP_LOG(fmt, ...) startup_log(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define STARTUP_SEPARATOR() startup_log(nullptr, 0, "══════════════════════════════════════════")
