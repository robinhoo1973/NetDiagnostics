// =============================================================================
// SettingsKeys.h — QSettings 键名单一来源
//
// 5WHY (simplify 2026-09-04): "AppSettings" 组名字面量曾散落
// ConfigurationController.cpp（file-local 常量）与 AppState.cpp（5 处
// 硬编码字面量）——两文件对同一组的认知无法共享。提升为共享头，
// 新增引用方直接复用，不再增长字面量副本。
// =============================================================================
#pragma once

inline constexpr const char* kSettingsGroup = "AppSettings";
