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
#include <QJniObject>

// The Qt Android activity (never cache across calls).
static inline QJniObject androidLogActivity() {
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
static inline QString androidUserVisibleLogDir() {
    QString base = androidExternalFilesDir();
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("NetDiagnostics"));
}

#endif // PLATFORM_ANDROID
