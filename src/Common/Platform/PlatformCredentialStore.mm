// =============================================================================
// PlatformCredentialStore.mm — 平台安全凭证存储实现（Apple: macOS/iOS）
//
// 5WHY (2026-09-04 修正复核): Keychain API（SecItemAdd/SecItemCopyMatching）
// 与字典字面量（@{} / @"..." / @YES / __bridge）是 Objective-C 语法，只能
// 以 Objective-C++（.mm）编译——曾置于 .cpp 中导致 Apple 平台编译失败。
// 本文件独立为 .mm，与 PlatformShare.mm / PlatformStore.mm 同约定；
// ARC 由 netdiag-target.cmake 的 -fobjc-arc 管理。
//
// 5WHY (2026-09-04 TestFlight CI 失败轮): Security.h 只前置声明 NSDictionary/
// NSNumber 而无类定义——Objective-C 字典/数字字面量要求完整类定义可见，
// CI 真机 SDK（Xcode 26.6）下编译失败（本地 stub 验证掩盖了该缺口）：
//   "definition of class NSDictionary must be available to use
//    Objective-C dictionary literals"
// 且 CFTypeRef→CFDataRef 的 __bridge cast 违反 clang 语义（bridge 仅用于
// ObjC↔CF 所有权转换，CF 指针间 cast 应直接使用 C++ cast）。
// 修正（业界最佳实践）：按 Apple GenericKeychain 官方示例改用纯 Core
// Foundation 字典 API——CFDictionaryCreateMutable + CFDictionarySetValue +
// kCFBooleanTrue + CFSTR，从构造上消除 ObjC 字面量/类定义依赖，
// __bridge 全部消失。
// 守卫用编译器内建 __APPLE__（qglobal.h include 之前即有定义，
// 与 PlatformStore.mm 的 macOS CI b21294 链接失败教训同源）。
// =============================================================================
#include "Common/Platform/PlatformCredentialStore.h"

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <QDebug>

namespace {

CFDataRef toCFData(const QByteArray& a) {
    return CFDataCreate(kCFAllocatorDefault,
                        reinterpret_cast<const UInt8*>(a.constData()),
                        static_cast<CFIndex>(a.size()));
}

QByteArray fromCFData(CFDataRef data) {
    if (!data) return {};
    const auto p = CFDataGetBytePtr(data);
    const auto n = CFDataGetLength(data);
    return QByteArray(reinterpret_cast<const char*>(p), n);
}

// 查询字典骨架：class=GenericPassword, service=NetDiagnostics, account=key。
// 调用方补 kSecReturnData / kSecMatchLimit / kSecValueData 等键后自行释放。
CFMutableDictionaryRef makeBaseQuery(const QString& key) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, CFSTR("NetDiagnostics"));
    CFStringRef account = key.toCFString();
    CFDictionarySetValue(query, kSecAttrAccount, account);
    CFRelease(account);
    return query;
}

} // namespace

bool platformCredentialSave(const QString& key, const QString& value) {
    if (key.isEmpty()) return false;
    const QByteArray plain = value.toUtf8();
    CFDataRef cfData = toCFData(plain);
    if (!cfData) return false;

    // 5WHY (H2 复核 2026-09-04): 原实现"先 Delete 再 Add"——Keychain 锁定时
    // Delete 成功、Add 失败（errSecInteractionNotAllowed），旧凭证已被删除、
    // 新凭证未写入：密码永久丢失。改为 SecItemUpdate 优先（无破坏窗口），
    // 条目不存在时回退 SecItemAdd。
    CFMutableDictionaryRef query = makeBaseQuery(key);
    CFMutableDictionaryRef updates = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(updates, kSecValueData, cfData);
    CFDictionarySetValue(updates, kSecAttrAccessible,
                         kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly);
    OSStatus status = SecItemUpdate(query, updates);
    CFRelease(updates);
    if (status == errSecItemNotFound) {
        CFDictionarySetValue(query, kSecValueData, cfData);
        CFDictionarySetValue(query, kSecAttrAccessible,
                             kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly);
        status = SecItemAdd(query, nullptr);
    }
    CFRelease(query);
    CFRelease(cfData);
    if (status != errSecSuccess)
        qWarning("platformCredentialSave: Keychain write failed (status %d)",
                 static_cast<int>(status));
    return status == errSecSuccess;
}

QString platformCredentialLoad(const QString& key) {
    if (key.isEmpty()) return {};

    CFMutableDictionaryRef query = makeBaseQuery(key);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    if (status != errSecSuccess || !result) {
        // 5WHY (H2 复核): 仅"条目不存在"是正常空态；其余（Keychain 锁定、
        // 首次解锁前读取 errSecInteractionNotAllowed）是真实失败——静默返回
        // 空串会让密码表现为"被清除"，至少留日志供诊断。
        if (status != errSecItemNotFound)
            qWarning("platformCredentialLoad: Keychain read failed (status %d)",
                     static_cast<int>(status));
        return {};
    }
    QByteArray plain = fromCFData(static_cast<CFDataRef>(result));
    CFRelease(result);
    return QString::fromUtf8(plain);
}

bool platformCredentialRemove(const QString& key) {
    if (key.isEmpty()) return false;

    CFMutableDictionaryRef query = makeBaseQuery(key);
    OSStatus status = SecItemDelete(query);
    CFRelease(query);
    // 条目本就不存在 = 已删除，视为成功（清除语义幂等）
    return status == errSecSuccess || status == errSecItemNotFound;
}

#endif // defined(__APPLE__)
