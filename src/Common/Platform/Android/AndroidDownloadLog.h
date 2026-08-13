// =============================================================================
// AndroidDownloadLog.h — Mirror startup log to the public Download folder
// =============================================================================
// 5WHY (Android 11+ invisible log): the app-scoped external dir
// (/storage/emulated/0/Android/data/<pkg>/files) is NOT browsable by stock
// file managers on Android 11+ (scoped storage) — the startup log exists but
// a non-technical user can't find it, and has no USB/adb to run logcat.
//
// MediaStore.Downloads (Android 11+, targetSdk 29+) lets an app create a file
// in the PUBLIC Download folder with NO storage permission — exactly where the
// user can see it.  The MediaStore plumbing lives in the Java helper EarlyLog
// (single source of truth, see resources/android/src/.../EarlyLog.java); this
// header is a thin JNI bridge so native startup_log() lines land in the SAME
// Download file as the Java early markers.
//
// Best-effort by design: every failure path no-ops — the app-scoped file and
// the logcat mirror still work.  Logging must never crash app startup.
// =============================================================================
#pragma once

#if defined(PLATFORM_ANDROID)
#include <QString>
#include <QDir>
#include <QFile>
#include <QJniObject>
#include "Common/Platform/Android/AndroidLogPaths.h"

// Mirror one already-formatted log line to Download/NetDiagnostics/
// NetDiagnostics_startup.log (public, user-visible).
// 5WHY (pre-JNI download gap): only the FIRST STARTUP_LOG line ("main()
// entered", main.cpp) actually runs before QGuiApplication; checkForPrevious
// Crash and all AppState markers run after it.  androidMirrorLineToDownloads()
// used to skip the pre-Qt line entirely, so the Download mirror missed exactly
// the line most useful for diagnosing a launch crash.  Fix: a native-file-IO
// fallback writes directly to /sdcard/Download/NetDiagnostics/ BEFORE JNI is
// available (works on API ≤28 with the WRITE_EXTERNAL_STORAGE grant; on API
// 29+ scoped storage rejects it but the attempt is harmless — logcat +
// app-scoped file still hold the data).
static inline void androidWriteLineToDownloadNative(const QString& line) {
    // Best-effort direct write — silently skip if Download is unwritable.
    static const char* kPath = "/sdcard/Download/NetDiagnostics/"
                               "NetDiagnostics_startup.log";
    // 5WHY: avoid per-line mkpath syscalls — open first; only on failure
    // (fresh-install dir gap) create the parent and retry ONCE.  After a
    // doomed attempt (API 29+ scoped storage, no grant) give up for the rest
    // of the process instead of failing the same syscalls on every line.
    static bool sNativeDisabled = false;
    if (sNativeDisabled)
        return;
    QFile f(QString::fromLatin1(kPath));
    if (!f.open(QIODevice::Append | QIODevice::WriteOnly)) {
        QDir().mkpath(QStringLiteral("/sdcard/Download/NetDiagnostics"));
        if (!f.open(QIODevice::Append | QIODevice::WriteOnly)) {
            sNativeDisabled = true;
            return;
        }
    }
    f.write(line.toUtf8());
    f.close();
}

static inline void androidMirrorLineToDownloads(const QString& line) {
    // 5WHY: Always try the native fallback FIRST — it covers the pre-JNI
    // window (the "main() entered" line and any QGuiApplication-ctor crash
    // path) and works on API ≤28 with the storage grant.
    androidWriteLineToDownloadNative(line);

    // 5WHY (b21294): QJniObject needs the Qt Android platform plugin, which
    // only finishes loading when QGuiApplication CONSTRUCTS.  Use the explicit
    // androidJniReady() flag — QCoreApplication::instance() is already non-null
    // inside the ctor while JNI is still uninitialized.  The JNI path
    // (MediaStore) is the canonical one for API 29+ scoped storage.
    if (!androidJniReady())
        return;
    // EarlyLog ships in every NetDiagnostics APK (resources/android/src).
    QJniObject jline = QJniObject::fromString(line);
    QJniObject::callStaticMethod<void>(
        "com/netdiagnostic/app/EarlyLog",
        "appendToDownloads",
        "(Ljava/lang/String;)V",
        jline.object<jstring>());
}

#endif // PLATFORM_ANDROID
