// =============================================================================
// PlatformCredentialStore.h — 平台安全凭证存储抽象
//
// 5WHY (H2 凭证明文): m_targetPassword 曾以明文存入 QSettings——任何有文件
// 读取权限的进程均可获取 SSH/FTP/MySQL 凭证。本模块封装平台原生安全存储：
//   · Windows — DPAPI (CryptProtectData/CryptUnprotectData, crypt32.dll 已链接)
//   · macOS/iOS — Keychain (Security.framework)，实现于 PlatformCredentialStore.mm
//     （Objective-C++；PlatformCredentialStore.cpp 在 Apple 平台编译为空 TU）
//   · Linux — 文件存储 + 机器绑定密钥 (gethostname + getuid 派生)，0600 权限
//   · Android — 当前走 Linux 分支（EncryptedSharedPreferences 为未来扩展）
//
// 接口风格与 PlatformStore.h 一致：C-linkage 自由函数，各平台独立实现。
// =============================================================================
#pragma once

#include <QString>

// 保存凭证到平台安全存储。
// key: 存储键名（如 "targetPassword"）
// value: 要保存的秘密值
// 返回: 成功返回 true
bool platformCredentialSave(const QString& key, const QString& value);

// 从平台安全存储读取凭证。
// key: 存储键名
// 返回: 读取到的值；失败返回空串
QString platformCredentialLoad(const QString& key);

// 从平台安全存储删除凭证。
// key: 存储键名
// 返回: 成功返回 true
bool platformCredentialRemove(const QString& key);
