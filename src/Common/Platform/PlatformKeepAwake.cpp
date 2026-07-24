// =============================================================================
// PlatformKeepAwake.cpp — Desktop implementation (Linux D-Bus / fallback)
// =============================================================================
#include "Common/Platform/PlatformKeepAwake.h"
#include <QProcess>
#include <QDebug>
#include <atomic>

#if defined(__linux__) && !defined(PLATFORM_ANDROID)
// Use D-Bus via command-line to inhibit screensaver (no QtDBus dependency).
// Stores the inhibit cookie so we can uninhibit on disable.
static std::atomic<bool> s_keepAwake{false};
static quint32 s_inhibitCookie = 0;

void platformSetKeepAwake(bool enable) {
    if (enable == s_keepAwake.load()) return;

    if (enable) {
        // Try org.freedesktop.ScreenSaver.Inhibit (works with GNOME, KDE, XFCE)
        QProcess proc;
        proc.start(QStringLiteral("dbus-send"), {
            QStringLiteral("--session"),
            QStringLiteral("--dest=org.freedesktop.ScreenSaver"),
            QStringLiteral("--type=method_call"),
            QStringLiteral("--print-reply"),
            QStringLiteral("/org/freedesktop/ScreenSaver"),
            QStringLiteral("org.freedesktop.ScreenSaver.Inhibit"),
            QStringLiteral("string:NetDiagnostics"),
            QStringLiteral("string:Automated screen capture in progress")
        });
        proc.waitForFinished(3000);
        QByteArray output = proc.readAllStandardOutput();
        // Parse cookie from reply: "uint32 N"
        int idx = output.indexOf("uint32 ");
        if (idx >= 0) {
            s_inhibitCookie = output.mid(idx + 7).trimmed().toUInt();
        }
        s_keepAwake = true;
        qDebug() << "[PlatformKeepAwake] Inhibit cookie:" << s_inhibitCookie;
    } else {
        if (s_inhibitCookie > 0) {
            QProcess proc;
            proc.start(QStringLiteral("dbus-send"), {
                QStringLiteral("--session"),
                QStringLiteral("--dest=org.freedesktop.ScreenSaver"),
                QStringLiteral("--type=method_call"),
                QStringLiteral("/org/freedesktop/ScreenSaver"),
                QStringLiteral("org.freedesktop.ScreenSaver.UnInhibit"),
                QStringLiteral("uint32:") + QString::number(s_inhibitCookie)
            });
            proc.waitForFinished(3000);
            s_inhibitCookie = 0;
        }
        s_keepAwake = false;
        qDebug() << "[PlatformKeepAwake] UnInhibited";
    }
}

bool platformIsKeepAwake() {
    return s_keepAwake.load();
}

#elif defined(_WIN32)
// Windows: SetThreadExecutionState (needs windows.h)
#include <windows.h>
static std::atomic<bool> s_keepAwake{false};

void platformSetKeepAwake(bool enable) {
    if (enable == s_keepAwake.load()) return;
    if (enable) {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED
                                | ES_DISPLAY_REQUIRED | ES_AWAYMODE_REQUIRED);
    } else {
        SetThreadExecutionState(ES_CONTINUOUS);
    }
    s_keepAwake = enable;
}

bool platformIsKeepAwake() {
    return s_keepAwake.load();
}

#elif defined(__APPLE__)
// macOS: IOKit IOPMAssertionCreateWithName — see .mm implementation
// For now, stub: macOS doesn't auto-lock during active input anyway.
static std::atomic<bool> s_keepAwake{false};

void platformSetKeepAwake(bool enable) {
    s_keepAwake = enable;
    // Full implementation in PlatformKeepAwake.mm (IOKit)
}

bool platformIsKeepAwake() {
    return s_keepAwake.load();
}

#else
// Generic fallback: no-op
static bool s_keepAwake = false;

void platformSetKeepAwake(bool enable) { s_keepAwake = enable; }
bool platformIsKeepAwake() { return s_keepAwake; }
#endif
