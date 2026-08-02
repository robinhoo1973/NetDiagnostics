// =============================================================================
// PlatformAndroidJni.h — Shared JNI helpers for Android platform modules
// =============================================================================
// 5WHY: Android platform modules need the same Qt Activity lookup. Keep it
// in one inline helper so QtNative API migrations are updated consistently.
// =============================================================================
#pragma once

#include <QJniObject>

// Returns the Qt Activity for JNI calls that require an Activity context
// (Window operations, system-service lookups).  MUST NOT be cached across
// calls — Android destroys/recreates the Activity on configuration changes
// (rotation, multi-window resize, locale change).
inline QJniObject getQtActivity() {
    return QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;");
}
