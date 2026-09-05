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
    // 5WHY (2026-09-05 QtNative 回退即崩溃源): 曾以硬编码
    // "org/qtproject/qt/android/QtNative" 静态调用兜底——AndroidLogPaths.h
    // 的 5WHY 已归档该类名的确切崩溃模式（Qt 6.5.3 无此类：FindClass 失败
    // 留 NoClassDefFoundError 挂起 → 下一次 JNI 调用 SIGABRT）。Qt 6.2+
    // 的 QNativeInterface 才是版本无关入口；context 无效时返回无效对象，
    // 全部调用方均已判空降级（权限/服务探测返回"不可用"文案）。
    return QNativeInterface::QAndroidApplication::context();
}
