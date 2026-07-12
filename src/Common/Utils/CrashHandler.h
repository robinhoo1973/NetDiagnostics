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
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <exception>
#include <typeinfo>

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
// ── Forward declarations (file scope) ──────────────────────────────────
// 5WHY: The extern declaration inside CrashHandler::checkForPreviousCrash()
// scopes startup_log to CrashHandler::startup_log, but the actual definition
// (in StartupLog.h, included by main.cpp) is ::startup_log at global scope.
// Adding a file-scope forward declaration makes ::startup_log visible to the
// compiler's name lookup, so the linker resolves the correct global symbol.
// Without this, iOS builds fail with: "Undefined symbols: CrashHandler::startup_log"
// because the block-scope extern inside the namespace shadows the global one.
#if defined(ND_DEBUG) || defined(ND_TESTING) || defined(PLATFORM_IOS)
void startup_log(const char* file, int line, const char* fmt, ...);
#endif

// ═══════════════════════════════════════════════════════════════════════
// CrashHandler namespace
// ═══════════════════════════════════════════════════════════════════════
namespace CrashHandler {

static constexpr const char* kCrashFileName = "NetDiagnostics_crash.log";

// 5WHY: Set once a qFatal/qCritical/terminate crash report has been written,
// so the trailing abort()→SIGABRT does not overwrite the real root cause.
static bool g_messageCrashWritten = false;

// ── Helper: crash log path ─────────────────────────────────────────────
static QString crashLogPath() {
    // 5WHY: On iOS the temp dir is sandboxed and invisible to the user.
    // Write the crash log to Documents so it is retrievable via Files.app
    // (requires UIFileSharingEnabled + LSSupportsOpeningDocumentsInPlace).
#if defined(PLATFORM_IOS)
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath(QString::fromLatin1(kCrashFileName));
#else
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QString::fromLatin1(kCrashFileName));
#endif
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
    // 5WHY: If a qFatal/qCritical/terminate report was already written for
    // this crash, the ensuing abort() → SIGABRT must NOT overwrite it with a
    // useless "abort → pthread_kill" backtrace.  Append the signal instead so
    // the real root cause (the Qt message / exception) is preserved.
    if (g_messageCrashWritten) {
        QFile af(path);
        if (af.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream ats(&af);
            ats << "--- Subsequent signal: " << signalName
                << " (" << signalNum << ") ---\n";
            ats.flush();
            af.close();
        }
        return;
    }
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
    // 5WHY: Use the project's own PLATFORM_IOS define rather than Apple's
    // TARGET_OS_IOS, which requires <TargetConditionals.h> to be included;
    // without it the macro is undefined and iOS would be mislabelled macOS.
#if defined(_WIN32)
    ts << "Platform:  Windows\n";
#elif defined(PLATFORM_IOS)
    ts << "Platform:  iOS\n";
#elif defined(__APPLE__)
    ts << "Platform:  macOS\n";
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

// ── Append extra context to an existing crash report ───────────────────
// 5WHY: qFatal() and uncaught C++ exceptions are the way Qt/QML startup
// failures actually abort on iOS (e.g. engine.load() → qFatal → abort).
// A pure signal handler only sees the resulting SIGABRT with a useless
// backtrace (abort → pthread_kill).  Capturing the Qt message / exception
// text BEFORE abort() runs preserves the real root cause for upload.
static void writeMessageCrashReport(const char* kind, const QString& text) {
    QString path = crashLogPath();
    QFile f(path);
    f.remove();
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream ts(&f);
    ts << "=== NetDiagnostics Crash Report ===\n";
    ts << "Timestamp: " << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << "\n";
    ts << "Kind:      " << kind << "\n";
#if defined(PROJECT_VERSION)
    ts << "Version:   " << PROJECT_VERSION << "\n";
#endif
#if defined(ND_BUILD_NUMBER)
    ts << "Build:     " << ND_BUILD_NUMBER << "\n";
#endif
#if defined(APP_EDITION)
    ts << "Edition:   " << APP_EDITION << "\n";
#endif
#if defined(_WIN32)
    ts << "Platform:  Windows\n";
#elif defined(PLATFORM_IOS)
    ts << "Platform:  iOS\n";
#elif defined(__APPLE__)
    ts << "Platform:  macOS\n";
#elif defined(__ANDROID__)
    ts << "Platform:  Android\n";
#elif defined(__linux__)
    ts << "Platform:  Linux\n";
#endif
    ts << "Message:\n" << text << "\n";
    writeBacktrace(ts);
    ts << "====================================\n";
    ts.flush();
    f.close();
    g_messageCrashWritten = true;
}

// ── Qt message handler — captures qFatal into the crash report ─────────
// 5WHY: The QML startup-failure path in main.cpp uses qCritical()/qFatal()
// style diagnostics.  Without a message handler a genuine qFatal() aborts
// with only a useless "abort" backtrace, and its text goes only to stderr
// (iOS syslog), unreachable without a Mac.  This handler writes qFatal text
// to the crash log in Documents so it can be retrieved via Files.app.
//
// 5WHY (review fix): Only QtFatalMsg is treated as a crash.  qCritical is
// used throughout the app for RECOVERABLE errors (network failures, etc.);
// treating it as a crash produced false "previous crash" banners on every
// normal error.  Controlled QML-load failures are already captured by
// STARTUP_LOG to Documents, so qCritical does not belong in the crash log.
//
// 5WHY (review fix 2): qInstallMessageHandler returns nullptr when Qt's
// built-in default handler was active.  If we only forward when non-null,
// all qDebug/qWarning/qCritical console output is silently suppressed.
// When there is no previous handler we replicate the default by writing the
// formatted message to stderr, preserving normal logging.
static QtMessageHandler g_prevMsgHandler = nullptr;

static void qtMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    if (type == QtFatalMsg) {
        writeMessageCrashReport("qFatal", msg);
    }
    if (g_prevMsgHandler) {
        g_prevMsgHandler(type, ctx, msg);
    } else {
        // No previous handler (Qt default was active): preserve output so we
        // don't suppress logging by installing this handler.
        const QByteArray line = qFormatLogMessage(type, ctx, msg).toLocal8Bit();
        fprintf(stderr, "%s\n", line.constData());
        fflush(stderr);
    }
}

// ── std::terminate handler — captures uncaught C++ exceptions ──────────
// 5WHY: An uncaught C++ exception (e.g. std::bad_alloc, or a Qt exception)
// calls std::terminate() → abort() → SIGABRT.  By the time the signal
// handler runs, the stack is unwound and the exception type is lost.  This
// terminate handler runs while the exception is still active, so it records
// the exception's demangled type name and what() text for the crash report.
static std::terminate_handler g_prevTerminate = nullptr;

static void terminateHandler() {
    QString detail = QStringLiteral("Unhandled C++ exception (std::terminate)");
    if (std::exception_ptr ep = std::current_exception()) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            const char* tname = typeid(e).name();
#if !defined(_WIN32)
            int status = 0;
            char* demangled = abi::__cxa_demangle(tname, nullptr, nullptr, &status);
            const char* shown = (status == 0 && demangled) ? demangled : tname;
            detail = QStringLiteral("Unhandled exception: %1\nwhat(): %2")
                         .arg(QString::fromUtf8(shown), QString::fromUtf8(e.what()));
            if (demangled) free(demangled);
#else
            detail = QStringLiteral("Unhandled exception: %1\nwhat(): %2")
                         .arg(QString::fromUtf8(tname), QString::fromUtf8(e.what()));
#endif
        } catch (...) {
            detail = QStringLiteral("Unhandled non-std C++ exception");
        }
    }
    writeMessageCrashReport("std::terminate", detail);
    if (g_prevTerminate)
        g_prevTerminate();
    else
        std::abort();
}

// ── Install error-capture handlers (message + terminate) ───────────────
// Called from both the Windows and POSIX install() paths.
static void installErrorCapture() {
    g_prevMsgHandler = qInstallMessageHandler(qtMessageHandler);
    g_prevTerminate  = std::set_terminate(terminateHandler);
}

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
    installErrorCapture();
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
    installErrorCapture();
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
#if defined(ND_DEBUG) || defined(ND_TESTING) || defined(PLATFORM_IOS)
    extern void startup_log(const char*, int, const char*, ...);
    // 5WHY: Use global-scope call. The extern declaration inside the
    // CrashHandler namespace introduces CrashHandler::startup_log, but
    // the actual function is ::startup_log (global scope, defined in
    // StartupLog.h).  Explicit :: qualification avoids linker errors
    // on iOS where the optimizer may inline the namespace-qualified call.
    ::startup_log(nullptr, 0, "=== PREVIOUS CRASH DETECTED ===");
    ::startup_log(nullptr, 0, "%s", content.toUtf8().constData());
    ::startup_log(nullptr, 0, "=== END CRASH REPORT ===");
    ::startup_log(nullptr, 0, "Crash log retained at: %s", path.toUtf8().constData());
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
