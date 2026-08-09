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
// 5WHY (Android): Same problem — Release builds have no PC connectivity
// for logcat, and crashes during QML startup leave zero diagnostic trail.
// Enable on all mobile platforms unconditionally.
#if defined(ND_DEBUG) || defined(ND_TESTING) || defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(PLATFORM_ANDROID)
#include "Common/Platform/Android/AndroidLogPaths.h"
#include "Common/Platform/Android/AndroidDownloadLog.h"
#include <android/log.h>
#endif

// 5WHY: Choose a log directory the user can actually retrieve.
//   iOS      → Documents (Files.app, UIFileSharingEnabled) — user-visible
//   Android  → getExternalFilesDir (Android/data/<pkg>/files) — user-visible
//              via USB/MTP or file managers, no permission needed (5WHY in
//              AndroidLogPaths.h)
//   Desktop  → TempLocation (OS temp)
static QString startupLogDir() {
#if defined(PLATFORM_IOS)
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
#if defined(PLATFORM_ANDROID)
    return androidUserVisibleLogDir();
#else
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#endif
#endif
}

// 5WHY: startup_log must be `inline` (external linkage), NOT `static`.
// CrashHandler.h forward-declares it as an external function and calls
// ::startup_log in its crash path; a `static` definition only satisfies
// the TU that includes StartupLog.h, so a ND_DEBUG build would fail at
// link time with "undefined reference to startup_log" from CrashHandler.
// inline gives every TU a merged external definition that the declaration
// can bind to.
inline void startup_log(const char* file, int line, const char* fmt, ...) {
    QString dir = startupLogDir();
    // 5WHY (Android no-log bug): the NetDiagnostics/ subdirectory is never
    // created by the platform, and on a FRESH install (or after clear-data)
    // it does not exist yet.  QFile::open() below then fails and returns
    // silently — so the very first launch, which is exactly when a startup
    // crash is being diagnosed, produced NO log at all.  Create the dir
    // before every write.
    QDir().mkpath(dir);
    QString path = QDir(dir).filePath("NetDiagnostics_startup.log");

    QFile f(path);
    // 5WHY: QFile::open() is [[nodiscard]] in Qt 6 — ignoring the return
    // value generates a compiler warning. Check and silently skip logging
    // if the temp directory is unwritable (better than crashing).
    if (!f.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream ts(&f);

    QString tsStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // 5WHY: build the FULL formatted line once and reuse it for every sink
    // (app-scoped file, logcat, public Download mirror) so all three always
    // carry identical content.
    QString fullLine = QStringLiteral("[") + tsStr + QStringLiteral("] ")
                     + QString::fromUtf8(buf);
    if (file && line > 0)
        fullLine += QStringLiteral("  (") + QString::fromUtf8(file)
                  + QStringLiteral(":") + QString::number(line)
                  + QStringLiteral(")");
    fullLine += QLatin1Char('\n');

    ts << fullLine;
    ts.flush();
    f.close();

#if defined(PLATFORM_ANDROID)
    // 5WHY (Android): mirror every startup event to logcat so the trail is
    // available via `adb logcat -s NetDiagnostics` even when the file write
    // fails (e.g. external storage unavailable) or for CI/developer devices.
    __android_log_print(ANDROID_LOG_INFO, "NetDiagnostics", "%s", buf);
    // 5WHY (Android): also mirror to the PUBLIC Download folder so a
    // non-technical user can find the log without USB/adb (see
    // AndroidDownloadLog.h).  Pre-JNI lines take the raw-path fallback;
    // post-JNI lines go via MediaStore (EarlyLog, API 29+) or the raw path
    // with the WRITE_EXTERNAL_STORAGE grant (API ≤28).
    androidMirrorLineToDownloads(fullLine);
#endif

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
    QString path = QDir(startupLogDir()).filePath("NetDiagnostics_startup.log");
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
