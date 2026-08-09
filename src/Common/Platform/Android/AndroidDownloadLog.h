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
#include <QCoreApplication>
#include <QJniObject>

// Mirror one already-formatted log line to Download/NetDiagnostics/
// NetDiagnostics_startup.log (public, user-visible, Android 11+).
// 5WHY (pre-Qt download gap): the first 3-8 STARTUP_LOG lines run BEFORE
// QGuiApplication constructs (main entry → CrashHandler check → AppState ctor).
// androidMirrorLineToDownloads() used to return immediately for those, so the
// Download mirror only covered post-Qt lines — exactly the WRONG half of the
// startup sequence for diagnosing a launch crash.  Fix: add a native-file-IO
// fallback that writes directly to /sdcard/Download/NetDiagnostics/ BEFORE
// JNI is available (works on Android ≤10; on 11+ scoped storage may reject it
// but the attempt is harmless — logcat + app-scoped file still hold the data).
static inline void androidWriteLineToDownloadNative(const QString& line) {
    // Best-effort direct write — silently skip if Download is unwritable.
    static const char* kPath = "/sdcard/Download/NetDiagnostics/"
                               "NetDiagnostics_startup.log";
    // 5WHY: mkdir the parent dir on every write, same pattern as StartupLog.h.
    // On a fresh install the NetDiagnostics/ dir doesn't exist yet.
    QDir().mkpath(QStringLiteral("/sdcard/Download/NetDiagnostics"));
    QFile f(QString::fromLatin1(kPath));
    if (f.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(line.toUtf8());
        f.close();
    }
}

static inline void androidMirrorLineToDownloads(const QString& line) {
    // 5WHY: Always try the native fallback FIRST — it covers the pre-Qt window
    // (before QGuiApplication exists) and works on Android ≤10 without JNI.
    androidWriteLineToDownloadNative(line);

    // 5WHY (b21294): QJniObject needs the Qt Android platform plugin, loaded
    // only when QGuiApplication is constructed.  The JNI path (MediaStore) is
    // the canonical one for Android 11+ scoped storage; skip it pre-Qt.
    if (QCoreApplication::instance() == nullptr)
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
