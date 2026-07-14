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
//
// 5WHY (iOS): TestFlight Release builds define neither ND_DEBUG nor
// ND_TESTING, so on the exact platform where startup failures are hardest
// to diagnose (no console access without a Mac), STARTUP_LOG compiled to
// nothing.  Always enable it on iOS so the app writes a startup log the
// user can retrieve via Files.app (see DocumentsLocation routing below).
#if defined(ND_DEBUG) || defined(ND_TESTING) || defined(PLATFORM_IOS)

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#endif

static void startup_log(const char* file, int line, const char* fmt, ...) {
    // 5WHY: On iOS the temp dir is inside the sandbox and not visible to the
    // user.  Write to the app's Documents directory instead — with
    // UIFileSharingEnabled + LSSupportsOpeningDocumentsInPlace in Info.plist,
    // this file is directly accessible in Files.app / Finder, so users can
    // retrieve the startup log without Xcode or a Mac.
#if defined(PLATFORM_IOS)
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#endif
    QString path = QDir(dir).filePath("NetDiagnostics_startup.log");

    QFile f(path);
    // 5WHY: QFile::open() is [[nodiscard]] in Qt 6 — ignoring the return
    // value generates a compiler warning. Check and silently skip logging
    // if the temp directory is unwritable (better than crashing).
    if (!f.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text))
        return;
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

#if defined(_WIN32)
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
#endif
}

#define STARTUP_LOG(fmt, ...) startup_log(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define STARTUP_SEPARATOR()  startup_log(nullptr, 0, "══════════════════════════════════════════")

// 5WHY: The startup log is only useful for diagnosing launch crashes.
// Once the app starts successfully (QML loaded + window shown), the log
// from the previous run is stale.  Delete it so stale crash-debug logs
// don't accumulate across successful launches.
static inline void startup_log_cleanup() {
#if defined(PLATFORM_IOS)
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#endif
    QString path = QDir(dir).filePath("NetDiagnostics_startup.log");
    QFile::remove(path);
}
#define STARTUP_CLEANUP() startup_log_cleanup()

#else  // neither ND_DEBUG nor ND_TESTING nor iOS — compile to nothing

#define STARTUP_LOG(fmt, ...) ((void)0)
#define STARTUP_SEPARATOR()  ((void)0)
#define STARTUP_CLEANUP()    ((void)0)

#endif

// ── Debug-mode console trace — always active in Debug builds ──────────
// 5WHY: iOS startup crashes leave no diagnostic trail.  STARTUP_TRACE
// outputs via qDebug() which on iOS appears in Console.app / Xcode
// device logs.  Independent of ND_DEBUG — works in any Debug build
// where NDEBUG is not defined (CMAKE_BUILD_TYPE=Debug).
#if !defined(NDEBUG)
#include <QDebug>
#define STARTUP_TRACE(fmt, ...) qDebug("[STARTUP] " fmt, ##__VA_ARGS__)
#else
#define STARTUP_TRACE(fmt, ...) ((void)0)
#endif
