// =============================================================================
// PlatformAndroidJni.h — Shared JNI helpers for Android platform modules
// =============================================================================
// 5WHY: getQtActivity() was defined identically in both PlatformFocus.cpp
// and PlatformRecording.cpp (as getActivity()).  Duplication means a fix
// in one copy (Qt version migration, new QtNative class name) would be
// missed in the other.  Extract once to a shared inline header.
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
