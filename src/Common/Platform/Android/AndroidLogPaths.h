// =============================================================================
// AndroidLogPaths.h — Android user-visible diagnostic log directory
// =============================================================================
// 5WHY (Android b21294): startup/crash logs were written to
// QStandardPaths::TempLocation, which on Android resolves to the app's PRIVATE
// cache dir (/data/user/0/<pkg>/cache) — invisible to the user and impossible
// to retrieve without root/adb.  On a launch crash there was no way for a
// non-technical user to collect the log.
//
// This helper returns a USER-VISIBLE directory on Android:
//   getExternalFilesDir(null) → /storage/emulated/0/Android/data/<pkg>/files
// which needs NO storage permission (app-scoped external dir, Android 5+),
// is reachable via USB/MTP and most file managers, and survives the app's
// cache being cleared.  Falls back to the internal AppDataLocation if the
// JNI call fails (covers very early crash paths before Qt initializes).
// =============================================================================
#pragma once

#if defined(PLATFORM_ANDROID)
#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QJniObject>

// 5WHY (Android launch crash): The old androidLogActivity() hardcoded the Qt
// Java class "org/qtproject/qt/android/QtNative" and called it from
// CrashHandler::checkForPreviousCrash() BEFORE QGuiApplication existed.
// (a) The Qt Android platform plugin — which initializes the JNI environment —
//     is only loaded when QGuiApplication is constructed; calling QJniObject
//     earlier crashed at startup.
// (b) Qt 6 moved the Java classes to the org.qtproject.qt6.android package,
//     so the hardcoded Qt5-era class name is not stable across Qt versions.
// Fix: (a) callers now invoke this only after QGuiApplication exists; (b) use
// the version-independent QNativeInterface::QAndroidApplication::context()
// accessor (Qt 6.2+, same pattern as PlatformShare.cpp), which resolves the
// Activity without hardcoding a Java class name.

// Explicit JNI-readiness flag, set by main() immediately AFTER QGuiApplication
// constructs (see main.cpp).
// 5WHY (Android launch crash, b21294 class): QCoreApplication::instance()
// becomes non-null inside QGuiApplication's CONSTRUCTOR, while the Qt Android
// platform plugin — which initializes the JNI environment — is only loaded
// near the END of that constructor.  A crash in that window (e.g. platform
// plugin init failure) would route the crash handler through JNI with an
// uninitialized VM: a second fault inside the crash handler, masking the real
// error and producing zero log.  Guard on this EXPLICIT flag instead.
// C++17 inline variable: ONE process-global object shared by every TU that
// includes this header (a `static inline` helper would duplicate per TU).
inline bool androidJniReadyFlag = false;
static inline bool androidJniReady() { return androidJniReadyFlag; }
static inline void setAndroidJniReady(bool v) { androidJniReadyFlag = v; }

// The Qt Android activity (never cache across calls).
static inline QJniObject androidLogActivity() {
    // Qt 6.2+: version-independent accessor — no hardcoded Java class name.
    // 5WHY: the legacy "org/qtproject/qt/android/QtNative" fallback was a
    // Qt5-era package that does not exist on Qt 6.5.3 — if it ever fired it
    // left a pending NoClassDefFoundError on the JNI stack that would abort
    // on the NEXT JNI call (SIGABRT). context() is valid post-JNI on every
    // Qt 6.x Android build we ship; fail empty (callers already tolerate it).
    return QNativeInterface::QAndroidApplication::context();
}

// Returns the app-scoped EXTERNAL files dir (/storage/emulated/0/Android/data/<pkg>/files).
// Empty if JNI unavailable (pre-Qt-init crash paths).
static inline QString androidExternalFilesDir() {
    QJniObject activity = androidLogActivity();
    if (!activity.isValid())
        return QString();
    QJniObject dir = activity.callObjectMethod(
        "getExternalFilesDir",
        "(Ljava/lang/String;)Ljava/io/File;",
        nullptr);
    if (!dir.isValid())
        return QString();
    QJniObject abs = dir.callObjectMethod<jstring>("getAbsolutePath");
    return abs.toString();
}

// User-visible base directory for NetDiagnostics logs on Android.
// Returns "<externalFiles>/NetDiagnostics" or a deterministic fallback.
// 5WHY (Android launch crash): this is called from CrashHandler even when the
// crash happens BEFORE/INSIDE QGuiApplication construction (e.g. app ctor
// crash, platform-plugin init failure).  JNI
// (QNativeInterface::QAndroidApplication::context) requires the Qt Android
// platform plugin, which is only loaded near the END of QGuiApplication's
// constructor — calling it before that would crash inside the crash handler
// (masking the real SIGSEGV).  Guard: before the explicit JNI-ready flag is
// set (and whenever the JNI call fails), fall back to the deterministic
// app-scoped external dir built from the package name
// (ND_ANDROID_PACKAGE, injected by CMake from AndroidManifest.xml.in).  It is
// user-visible via USB/MTP and needs no storage permission — unlike the old
// TempLocation fallback (private cache, invisible), which silently hid every
// early native startup log.
static inline QString androidUserVisibleLogDir() {
    if (!androidJniReady()) {
        // Pre-JNI crash path (before/inside QGuiApplication ctor): no JNI
        // available.  The app-scoped external dir is deterministic from the
        // package name and writable without JNI.
        return QStringLiteral("/storage/emulated/0/Android/data/")
               + QStringLiteral(ND_ANDROID_PACKAGE)
               + QStringLiteral("/files/NetDiagnostics");
    }
    QString base = androidExternalFilesDir();
    if (base.isEmpty()) {
        // JNI lookup failed even though the app exists — last-resort fallback
        // to the same deterministic user-visible path.
        base = QStringLiteral("/storage/emulated/0/Android/data/")
               + QStringLiteral(ND_ANDROID_PACKAGE)
               + QStringLiteral("/files");
    }
    return QDir(base).filePath(QStringLiteral("NetDiagnostics"));
}

#endif // PLATFORM_ANDROID
