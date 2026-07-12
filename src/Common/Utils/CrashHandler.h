// =============================================================================
// CrashHandler.h — Lightweight crash signal handler + post-mortem log.
//
// Installs POSIX signal / Windows SEH handlers that write a crash report
// to %TEMP%\NetDiagnostics_crash.log before the OS default handler runs.
//
// On next startup, checkForPreviousCrash() detects leftover crash logs and
// includes them in the startup log so the user/app can submit the report.
//
// Compile: #include "CrashHandler.h" in main.cpp.
// Register: CrashHandler::install();  (before QApplication creation)
// Check:    if (CrashHandler::checkForPreviousCrash()) { ... }
//
// Platform support:
//   POSIX (Linux/macOS/iOS/Android): sigaction + backtrace + dladdr
//   Windows: SetUnhandledExceptionFilter + CaptureStackBackTrace
// =============================================================================
#pragma once

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <cstdio>
#include <cstdlib>
#include <csignal>

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <unistd.h>
#endif

// ═══════════════════════════════════════════════════════════════════════
// CrashHandler namespace
// ═══════════════════════════════════════════════════════════════════════
namespace CrashHandler {

static constexpr const char* kCrashFileName = "NetDiagnostics_crash.log";

// ── Helper: crash log path ─────────────────────────────────────────────
static QString crashLogPath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QString::fromLatin1(kCrashFileName));
}

// ── Helper: write backtrace to file ────────────────────────────────────
static void writeBacktrace(QTextStream& ts) {
#if defined(_WIN32)
    void* stack[64];
    USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);
    ts << "  Backtrace (" << frames << " frames):\n";
    for (USHORT i = 0; i < frames; ++i) {
        ts << "    [" << i << "] " << reinterpret_cast<uintptr_t>(stack[i]) << "\n";
    }
#elif (defined(__APPLE__) || defined(__linux__)) && !defined(__ANDROID__)
    // 5WHY: Android IS Linux (__linux__ is defined), but Bionic libc
    // lacks backtrace()/backtrace_symbols() (glibc extensions).
    // Explicitly exclude __ANDROID__ so we don't call these on Android.
    void* stack[64];
    int frames = backtrace(stack, 64);
    char** symbols = backtrace_symbols(stack, frames);
    ts << "  Backtrace (" << frames << " frames):\n";
    for (int i = 0; i < frames; ++i) {
        ts << "    [" << i << "] ";
        if (symbols) {
            ts << symbols[i];
        } else {
            ts << reinterpret_cast<uintptr_t>(stack[i]);
        }
        ts << "\n";
    }
    if (symbols) free(symbols);
#endif
}

// ── Helper: write crash report ─────────────────────────────────────────
static void writeCrashReport(const char* signalName, int signalNum) {
    QString path = crashLogPath();
    QFile f(path);
    // Remove previous crash log if exists, start fresh
    f.remove();
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream ts(&f);
    ts << "=== NetDiagnostics Crash Report ===\n";
    ts << "Timestamp: " << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << "\n";
    ts << "Signal:    " << signalName << " (" << signalNum << ")\n";

    // Build info
#if defined(PROJECT_VERSION)
    ts << "Version:   " << PROJECT_VERSION << "\n";
#endif
#if defined(ND_BUILD_NUMBER)
    ts << "Build:     " << ND_BUILD_NUMBER << "\n";
#endif
#if defined(APP_EDITION)
    ts << "Edition:   " << APP_EDITION << "\n";
#endif

    // Platform info
#if defined(_WIN32)
    ts << "Platform:  Windows\n";
#elif defined(__APPLE__)
    #if TARGET_OS_IOS
    ts << "Platform:  iOS\n";
    #else
    ts << "Platform:  macOS\n";
    #endif
#elif defined(__ANDROID__)
    ts << "Platform:  Android\n";
#elif defined(__linux__)
    ts << "Platform:  Linux\n";
#endif

    writeBacktrace(ts);
    ts << "====================================\n";
    ts.flush();
    f.close();
}

// ── POSIX signal handler ───────────────────────────────────────────────
#if defined(_WIN32)

static LONG WINAPI windowsExceptionHandler(EXCEPTION_POINTERS* exInfo) {
    DWORD code = exInfo->ExceptionRecord->ExceptionCode;
    const char* name = "UNKNOWN";
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:    name = "SIGSEGV (Access Violation)"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:  name = "SIGFPE  (Divide by Zero)";   break;
        case EXCEPTION_ILLEGAL_INSTRUCTION: name = "SIGILL (Illegal Instruction)";break;
        case EXCEPTION_STACK_OVERFLOW:      name = "Stack Overflow";              break;
        default: break;
    }
    writeCrashReport(name, static_cast<int>(code));
    return EXCEPTION_EXECUTE_HANDLER; // Let OS handler run
}

static void install() {
    SetUnhandledExceptionFilter(windowsExceptionHandler);
}

#else // POSIX

static void posixSignalHandler(int sig) {
    const char* name = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: name = "SIGSEGV"; break;
        case SIGABRT: name = "SIGABRT"; break;
        case SIGFPE:  name = "SIGFPE";  break;
        case SIGILL:  name = "SIGILL";  break;
        case SIGBUS:  name = "SIGBUS";  break;
        default: break;
    }
    writeCrashReport(name, sig);
    // Reset to default and re-raise so OS crash reporting still works
    signal(sig, SIG_DFL);
    raise(sig);
}

static void install() {
    struct sigaction sa;
    sa.sa_handler = posixSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND; // One-shot: reset to default after handler runs
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
}

#endif

// ── Crash report recovery ──────────────────────────────────────────────
// Returns true if a previous crash log was found (and content written to
// startup_log if STARTUP_LOG is available).  Call once at startup.
static bool checkForPreviousCrash() {
    QString path = crashLogPath();
    if (!QFile::exists(path))
        return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QString content = QString::fromUtf8(f.readAll());
    f.close();

    // Write to startup log (if enabled)
#if defined(ND_DEBUG) || defined(ND_TESTING)
    extern void startup_log(const char*, int, const char*, ...);
    startup_log(nullptr, 0, "=== PREVIOUS CRASH DETECTED ===");
    startup_log(nullptr, 0, "%s", content.toUtf8().constData());
    startup_log(nullptr, 0, "=== END CRASH REPORT ===");
    startup_log(nullptr, 0, "Crash log retained at: %s", path.toUtf8().constData());
#endif

    // Also write to stderr so it appears in iOS syslog / Android logcat
    fprintf(stderr, "\n=== PREVIOUS CRASH DETECTED ===\n%s\n=== END CRASH REPORT ===\n",
            content.toUtf8().constData());
    fflush(stderr);

    // Don't delete — keep for user retrieval
    return true;
}

// ── Get crash log path for user-facing display ─────────────────────────
static QString crashReportPath() {
    QString path = crashLogPath();
    return QFile::exists(path) ? path : QString();
}

} // namespace CrashHandler
