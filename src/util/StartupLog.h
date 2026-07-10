// =============================================================================
// StartupLog.h — Crash diagnostic logging (DEBUG builds only).
//
// Controlled by CMake option ND_DEBUG (default OFF).  When enabled, writes
// timestamped startup events to %TEMP%\NetDiagnostics_startup.log.
// When disabled, all macros compile to no-ops — zero runtime overhead.
//
// Enable:  cmake -DND_DEBUG=ON ...
// Usage:   STARTUP_LOG("QML loaded, rootObjects=%d", count);
// =============================================================================
#pragma once

// 5WHY: test.yml sets ND_TESTING=ON but ND_DEBUG=OFF, making STARTUP_LOG a
// no-op. If the binary crashes before --test mode begins (QML load failure),
// there is zero diagnostic output. Enable logging whenever ND_TESTING is on.
#if defined(ND_DEBUG) || defined(ND_TESTING)

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
    if (file && line > 0)
        ts << "  (" << file << ":" << line << ")";
    ts << "\n";
    ts.flush();
    f.close();

#ifdef _WIN32
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
#endif
}

#define STARTUP_LOG(fmt, ...) startup_log(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define STARTUP_SEPARATOR()  startup_log(nullptr, 0, "══════════════════════════════════════════")

#else  // neither ND_DEBUG nor ND_TESTING — compile to nothing

#define STARTUP_LOG(fmt, ...) ((void)0)
#define STARTUP_SEPARATOR()  ((void)0)

#endif
