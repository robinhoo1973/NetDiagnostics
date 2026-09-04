// =============================================================================
// PlatformCredentialStore.mm — 平台安全凭证存储实现（Apple: macOS/iOS）
//
// 5WHY (2026-09-04 修正复核): Keychain API（SecItemAdd/SecItemCopyMatching）
// 与字典字面量（@{} / @"..." / @YES / __bridge）是 Objective-C 语法，只能
// 以 Objective-C++（.mm）编译——曾置于 .cpp 中导致 Apple 平台编译失败。
// 本文件独立为 .mm，与 PlatformShare.mm / PlatformStore.mm 同约定；
// ARC 由 netdiag-target.cmake 的 -fobjc-arc 管理。
// =============================================================================
#include "Common/Platform/PlatformCredentialStore.h"

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)

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

} // namespace

bool platformCredentialSave(const QString& key, const QString& value) {
    if (key.isEmpty()) return false;
    const QByteArray plain = value.toUtf8();

    // 5WHY (2026-09-04 修正复核): __bridge 后必须跟类型（(__bridge id)x），
    // 裸变量名（(__bridge kSecClass)）按 C 语法被当作表达式——clang 报
    // "cannot initialize return object of type 'id'" / "expected ';'"，整包
    // Apple 编译失败。统一 (__bridge id) 转换。
    //
    // 5WHY (H2 复核 2026-09-04): 原实现"先 Delete 再 Add"——Keychain 锁定时
    // Delete 成功、Add 失败（errSecInteractionNotAllowed），旧凭证已被删除、
    // 新凭证未写入：密码永久丢失。改为 SecItemUpdate 优先（无破坏窗口），
    // 条目不存在时回退 SecItemAdd。
    CFDataRef cfData = toCFData(plain);
    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef) @{
        (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: @"NetDiagnostics",
        (__bridge id)kSecAttrAccount: key.toNSString(),
    }, (__bridge CFDictionaryRef) @{
        (__bridge id)kSecValueData:   (__bridge id)cfData,
        (__bridge id)kSecAttrAccessible:
            (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
    });
    if (status == errSecItemNotFound) {
        status = SecItemAdd((__bridge CFDictionaryRef) @{
            (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService: @"NetDiagnostics",
            (__bridge id)kSecAttrAccount: key.toNSString(),
            (__bridge id)kSecValueData:   (__bridge id)cfData,
            (__bridge id)kSecAttrAccessible:
                (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
        }, nullptr);
    }
    CFRelease(cfData);
    if (status != errSecSuccess)
        qWarning("platformCredentialSave: Keychain write failed (status %d)",
                 static_cast<int>(status));
    return status == errSecSuccess;
}

QString platformCredentialLoad(const QString& key) {
    if (key.isEmpty()) return {};

    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef) @{
        (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: @"NetDiagnostics",
        (__bridge id)kSecAttrAccount: key.toNSString(),
        (__bridge id)kSecReturnData:  @YES,
        (__bridge id)kSecMatchLimit:  (__bridge id)kSecMatchLimitOne,
    }, &result);
    if (status != errSecSuccess || !result) {
        // 5WHY (H2 复核): 仅"条目不存在"是正常空态；其余（Keychain 锁定、
        // 首次解锁前读取 errSecInteractionNotAllowed）是真实失败——静默返回
        // 空串会让密码表现为"被清除"，至少留日志供诊断。
        if (status != errSecItemNotFound)
            qWarning("platformCredentialLoad: Keychain read failed (status %d)",
                     static_cast<int>(status));
        return {};
    }
    QByteArray plain = fromCFData((__bridge CFDataRef)result);
    CFRelease(result);
    return QString::fromUtf8(plain);
}

bool platformCredentialRemove(const QString& key) {
    if (key.isEmpty()) return false;

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef) @{
        (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: @"NetDiagnostics",
        (__bridge id)kSecAttrAccount: key.toNSString(),
    });
    // 条目本就不存在 = 已删除，视为成功（清除语义幂等）
    return status == errSecSuccess || status == errSecItemNotFound;
}

#endif // defined(Q_OS_MACOS) || defined(Q_OS_IOS)
