// =============================================================================
// LogPaths.h — 用户可检索日志目录单一来源
//
// 5WHY (Reuse 2026-09-05): "日志放哪个目录用户才能拿到"的三路路由
// （iOS Documents / Android getExternalFilesDir / 桌面 Temp）曾在
// StartupLog.h::startupLogDir 与 CrashHandler.h::crashLogPath 平行维护——
// 平台分支改动须两处同步，漏一处则崩溃日志与启动日志分落两目录，
// 支持脚本与用户检索故事双双断裂。收敛单点。
// =============================================================================
#pragma once

#include <QString>
#include <QDir>
#include <QStandardPaths>

#if defined(PLATFORM_ANDROID)
#include "Common/Platform/Android/AndroidLogPaths.h"
#endif

// 用户可检索的日志目录：
//   iOS      → Documents（Files.app，UIFileSharingEnabled）
//   Android  → getExternalFilesDir（Android/data/<pkg>/files，USB/MTP 可见）
//   Desktop  → TempLocation（OS 临时目录）
inline QString userVisibleLogDir() {
#if defined(PLATFORM_IOS)
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
#if defined(PLATFORM_ANDROID)
    return androidUserVisibleLogDir();
#else
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#endif
#endif
}
