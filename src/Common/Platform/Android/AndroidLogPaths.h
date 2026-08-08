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

// The Qt Android activity (never cache across calls).
static inline QJniObject androidLogActivity() {
    // Qt 6.2+: version-independent accessor — no hardcoded Java class name.
    QJniObject ctx = QNativeInterface::QAndroidApplication::context();
    if (ctx.isValid())
        return ctx;
    // Legacy fallback for older Qt (should not be reached on 6.5.3).
    return QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;");
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
// Returns "<externalFiles>/NetDiagnostics" or falls back to AppDataLocation.
// 5WHY (Android launch crash): this is called from CrashHandler even when the
// crash happens BEFORE QGuiApplication is constructed (e.g. app ctor crash).
// JNI (QNativeInterface::QAndroidApplication::context) requires the Qt Android
// platform plugin, which is only loaded by QGuiApplication — calling it before
// that would crash inside the crash handler (masking the real SIGSEGV).  Guard:
// before the app object exists, fall back to the pure-Qt TempLocation path.
static inline QString androidUserVisibleLogDir() {
    if (QCoreApplication::instance() == nullptr)
        return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
               + QStringLiteral("/NetDiagnostics");
    QString base = androidExternalFilesDir();
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("NetDiagnostics"));
}

#endif // PLATFORM_ANDROID
