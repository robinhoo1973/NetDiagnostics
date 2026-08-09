// =============================================================================
// PlatformAndroidJni.h — Shared JNI helpers for Android platform modules
// =============================================================================
// 5WHY: Android platform modules need the same Qt Activity lookup. Keep it
// in one inline helper so QtNative API migrations are updated consistently.
// =============================================================================
#pragma once

#include <QCoreApplication>
#include <QJniObject>
#include "Common/Platform/Android/AndroidLogPaths.h"

// Returns the Qt Activity for JNI calls that require an Activity context
// (Window operations, system-service lookups).  MUST NOT be cached across
// calls — Android destroys/recreates the Activity on configuration changes
// (rotation, multi-window resize, locale change).
// 5WHY (Android launch crash): the previous implementation hardcoded the
// Qt5-era Java class "org/qtproject/qt/android/QtNative", which is not stable
// across Qt 6 versions (Qt 6 moved Java classes to org.qtproject.qt6.android).
// Use the version-independent QNativeInterface::QAndroidApplication::context()
// (Qt 6.2+) instead — same pattern as PlatformShare.cpp / AndroidLogPaths.h.
inline QJniObject getQtActivity() {
    // Guard: JNI requires the Qt Android platform plugin, which finishes
    // initializing only when QGuiApplication CONSTRUCTS.  Use the explicit
    // androidJniReady() flag — QCoreApplication::instance() is non-null
    // inside the ctor while JNI is still unavailable.
    if (!androidJniReady())
        return QJniObject();
    QJniObject ctx = QNativeInterface::QAndroidApplication::context();
    if (ctx.isValid())
        return ctx;
    return QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;");
}
