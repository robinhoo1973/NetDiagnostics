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
#include <QCoreApplication>
#include <QJniObject>

// Mirror one already-formatted log line to Download/NetDiagnostics/
// NetDiagnostics_startup.log (public, user-visible, Android 11+).
static inline void androidMirrorLineToDownloads(const QString& line) {
    // 5WHY (b21294): QJniObject / QNativeInterface::QAndroidApplication need
    // the Qt Android platform plugin, loaded only when QGuiApplication is
    // constructed — calling them earlier crashed at startup (see
    // AndroidLogPaths.h).  The first 1-3 native lines (pre-QGuiApplication)
    // therefore skip the Download mirror; they still land in the app-scoped
    // dir + logcat.  Java's EarlyLog already covers the pre-native window.
    if (QCoreApplication::instance() == nullptr)
        return;
    // EarlyLog ships in every NetDiagnostics APK (resources/android/src).
    // If the class/method is ever missing, QJniObject logs a warning and
    // no-ops — it never aborts, so a future refactor can't turn logging
    // into a crash.
    QJniObject jline = QJniObject::fromString(line);
    QJniObject::callStaticMethod<void>(
        "com/netdiagnostic/app/EarlyLog",
        "appendToDownloads",
        "(Ljava/lang/String;)V",
        jline.object<jstring>());
}

#endif // PLATFORM_ANDROID
