// =============================================================================
// PlatformCredentialStore.cpp — 平台安全凭证存储实现（非 Apple 平台）
//
// 5WHY (H2 凭证明文): m_targetPassword 曾以明文存入 QSettings——任何有文件
// 读取权限的进程均可获取 SSH/FTP/MySQL 凭证。本文件封装平台原生安全存储：
//   · Windows — DPAPI (CryptProtectData/CryptUnprotectData)
//   · Linux — 文件存储 + 机器绑定密钥 (XOR obfuscation with hostname+uid)
//
// 5WHY (2026-09-04 修正复核): Apple (macOS/iOS) 的 Keychain 实现含
// Objective-C 字面量（@{} / @"..." / __bridge）——.cpp 以 C++ 编译，Apple
// 平台整包编译失败。Keychain 实现移至 PlatformCredentialStore.mm
// （Objective-C++，与 PlatformShare.mm / PlatformStore.mm 同约定），
// 本 TU 在 Apple 平台不定义任何符号，避免重复定义。
//
// 各平台通过 #if 分支编译，不引入额外链接依赖（Windows crypt32 已链接，
// macOS/iOS Security.framework 已在 netdiag-target.cmake 添加）。
// =============================================================================
#include "Common/Platform/PlatformCredentialStore.h"

#if !defined(Q_OS_MACOS) && !defined(Q_OS_IOS)

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

// ═══════════════════════════════════════════════════════════════════════
// Windows — DPAPI
// ═══════════════════════════════════════════════════════════════════════
#if defined(Q_OS_WIN32)

#include <windows.h>
#include <wincrypt.h>

#pragma comment(lib, "crypt32.lib")

namespace {

bool protectData(const QByteArray& plain, QByteArray* cipher) {
    DATA_BLOB inBlob = { static_cast<DWORD>(plain.size()),
                         reinterpret_cast<BYTE*>(const_cast<char*>(plain.data())) };
    DATA_BLOB outBlob = {};
    if (!CryptProtectData(&inBlob, L"NetDiagnostics", nullptr, nullptr,
                          nullptr, CRYPTPROTECT_LOCAL_MACHINE, &outBlob))
        return false;
    cipher->assign(reinterpret_cast<const char*>(outBlob.pbData), outBlob.cbData);
    LocalFree(outBlob.pbData);
    return true;
}

bool unprotectData(const QByteArray& cipher, QByteArray* plain) {
    DATA_BLOB inBlob = { static_cast<DWORD>(cipher.size()),
                         reinterpret_cast<BYTE*>(const_cast<char*>(cipher.data())) };
    DATA_BLOB outBlob = {};
    if (!CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr,
                            nullptr, 0, &outBlob))
        return false;
    plain->assign(reinterpret_cast<const char*>(outBlob.pbData), outBlob.cbData);
    LocalFree(outBlob.pbData);
    return true;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
// Linux — 文件存储 + 机器绑定混淆
// ═══════════════════════════════════════════════════════════════════════
#else

#include <unistd.h>
#include <sys/utsname.h>
#include <QCryptographicHash>

namespace {

// 5WHY (H2 Linux): Linux 无系统级密钥链（libsecret 需额外依赖）。
// 使用 gethostname()+getuid() 派生 XOR 密钥——同一机器同一用户可解密，
// 换机器/换用户数据不可读。非军事级安全，但远优于明文；文件权限另
// 收紧为 0600（仅属主可读，见 platformCredentialSave）。
QByteArray machineKey() {
    char host[256] = {};
    struct utsname u = {};
    uname(&u);
    snprintf(host, sizeof(host), "%s:%d:%s", u.nodename, static_cast<int>(getuid()), u.sysname);
    return QCryptographicHash::hash(host, QCryptographicHash::Sha256).left(32);
}

QByteArray xorCrypt(const QByteArray& data, const QByteArray& key) {
    QByteArray out(data.size(), '\0');
    for (int i = 0; i < data.size(); ++i)
        out[i] = data[i] ^ key[i % key.size()];
    return out;
}

} // namespace

#endif

// ═══════════════════════════════════════════════════════════════════════
// 公共实现（非 Apple）
// ═══════════════════════════════════════════════════════════════════════
namespace {

QString credDir() {
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("credentials"));
    QDir().mkpath(dir);
    return dir;
}

QString credPath(const QString& key) {
    return credDir() + QLatin1Char('/') + key + QStringLiteral(".bin");
}

} // namespace

bool platformCredentialSave(const QString& key, const QString& value) {
    if (key.isEmpty()) return false;
    const QByteArray plain = value.toUtf8();
    QByteArray cipher;

#if defined(Q_OS_WIN32)
    if (!protectData(plain, &cipher)) return false;
#else
    cipher = xorCrypt(plain, machineKey());
#endif

    // 完整性魔数前缀：Linux 密钥由 hostname 派生，主机名变更/文件截断/
    // 位翻转都会让解密输出变成"乱码密码"（XOR 无 MAC 校验）——乱码会被
    // 静默用于 SSH/FTP 认证。加载端校验魔数，不符即视为凭证丢失并告警。
    const QByteArray blob = QByteArrayLiteral("NDC1") + cipher;

    // 5WHY (2026-09-04 修正复核): QSaveFile 原子写——旧密文在 commit 成功
    // 前保持完好：写失败/断电不会先毁掉已存凭证再报错（open+Truncate
    // 方案短写失败时旧文已被截断，密码两边落空）。commit 内含 flush/fsync，
    // 持久化契约与 persistResults 同标准。
    QSaveFile f(credPath(key));
    if (!f.open(QIODevice::WriteOnly)) return false;
    if (f.write(blob) != blob.size()) {
        f.cancelWriting();   // 移除临时文件，旧密文保持原样
        return false;
    }
    if (!f.commit()) {
        qWarning("platformCredentialSave: atomic commit failed for %s",
                 qPrintable(credPath(key)));
        return false;
    }
    // 5WHY (H2 复核): QSaveFile 以默认 umask 创建新文件（0644）——其他
    // 本地用户可读，而 XOR 密钥可由 hostname+属主 uid 公开推导，凭证
    // 形同虚设。commit 后收紧为 0600；chmod 失败（FAT/exFAT/NFS 等）即
    // 删除并返回 false（fail-closed，调用方显式告警）。
    QFile chk(credPath(key));
    if (!chk.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        qWarning("platformCredentialSave: failed to set 0600 on %s",
                 qPrintable(credPath(key)));
        QFile::remove(credPath(key));
        return false;
    }
    return true;
}

QString platformCredentialLoad(const QString& key) {
    if (key.isEmpty()) return {};

    QFile f(credPath(key));
    if (!f.open(QIODevice::ReadOnly)) return {};   // 条目不存在 = 正常空态
    const QByteArray blob = f.readAll();
    static const QByteArray kMagic = QByteArrayLiteral("NDC1");
    if (!blob.startsWith(kMagic)) {
        // 主机名变更（密钥漂移）/文件截断/损坏——返回"凭证丢失"而非乱码。
        qWarning("platformCredentialLoad: credential integrity check failed "
                 "(hostname change or truncated file) — treating as lost");
        return {};
    }
    const QByteArray cipher = blob.mid(kMagic.size());
#if defined(Q_OS_WIN32)
    QByteArray plain;
    if (!unprotectData(cipher, &plain)) {
        // 5WHY (H2 复核): 主密钥丢失/用户配置文件迁移导致解密失败时，静默
        // 空串会被当成"密码未设置"——留日志区分真实失败与空态。
        qWarning("platformCredentialLoad: DPAPI unprotect failed for key, "
                 "credential unreadable");
        return {};
    }
    return QString::fromUtf8(plain);
#else
    return QString::fromUtf8(xorCrypt(cipher, machineKey()));
#endif
}

bool platformCredentialRemove(const QString& key) {
    if (key.isEmpty()) return false;
    const QString path = credPath(key);
    // 条目不存在 = 已删除，视为成功（清除语义幂等）
    return !QFile::exists(path) || QFile::remove(path);
}

#endif // !defined(Q_OS_MACOS) && !defined(Q_OS_IOS)
