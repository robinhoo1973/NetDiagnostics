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
#include <QStandardPaths>

#include "Common/Utils/AtomicWriteFile.h"

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
    // 5WHY (simplify 2026-09-04): 目录一次性创建——原实现每次
    // load/save/remove 都重跑 writableLocation + mkpath（stat + 目录
    // 遍历）。凭证目录会话内不变，函数局部 static 初始化一次。
    static const QString dir = [] {
        const QString d = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QStringLiteral("credentials"));
        QDir().mkpath(d);
        return d;
    }();
    return dir;
}

QString credPath(const QString& key) {
    return credDir() + QLatin1Char('/') + key + QStringLiteral(".bin");
}

} // namespace

bool platformCredentialSave(const QString& key, const QString& value) {
    if (key.isEmpty()) return false;
    // 5WHY (2026-09-05 魔数未被加密 = 校验失效): 曾把 "NDC1" 明文前缀在
    // 密文上——XOR/DPAPI 换错密钥（主机名变更）时密文变乱码而前缀原样
    // 可读，完整性检查恒通过，乱码密码被静默用于认证（正是校验本要防的
    // 场景）。修正：魔数纳入加密域（NDC1+明文 一起加密），外层 "NDC2"
    // 仅作格式版本嗅探。旧 "NDC1" 明文前缀文件按 legacy 路径解密兼容。
    const QByteArray plain = QByteArrayLiteral("NDC1") + value.toUtf8();
    QByteArray cipher;

#if defined(Q_OS_WIN32)
    if (!protectData(plain, &cipher)) return false;
#else
    cipher = xorCrypt(plain, machineKey());
#endif

    const QByteArray blob = QByteArrayLiteral("NDC2") + cipher;

    // 5WHY (2026-09-04 修正复核): QSaveFile 原子写——旧密文在 commit 成功
    // 前保持完好：写失败/断电不会先毁掉已存凭证再报错（open+Truncate
    // 方案短写失败时旧文已被截断，密码两边落空）。commit 内含 flush/fsync。
    // 5WHY (simplify 2026-09-04): 序列收敛到 Common/Utils/AtomicWriteFile.h
    // （与 persistResults 同一助手），0600 收紧 fail-closed 一并收口。
    if (!atomicWriteFile(credPath(key), blob, /*restrictOwner=*/true)) {
        qWarning("platformCredentialSave: atomic write failed for %s",
                 qPrintable(credPath(key)));
        return false;
    }
    return true;
}

QString platformCredentialLoad(const QString& key) {
    if (key.isEmpty()) return {};

    QFile f(credPath(key));
    if (!f.open(QIODevice::ReadOnly)) return {};   // 条目不存在 = 正常空态
    const QByteArray blob = f.readAll();
    static const QByteArray kMagicV2 = QByteArrayLiteral("NDC2");
    static const QByteArray kMagicV1 = QByteArrayLiteral("NDC1");
    if (!blob.startsWith(kMagicV2) && !blob.startsWith(kMagicV1)) {
        // 未知格式/文件截断/损坏——返回"凭证丢失"而非乱码。
        qWarning("platformCredentialLoad: unrecognized credential format "
                 "(truncated or corrupted file) — treating as lost");
        return {};
    }
#if defined(Q_OS_WIN32)
    QByteArray plain;
    if (!unprotectData(blob.mid(4), &plain)) {
        // 5WHY (H2 复核): 主密钥丢失/用户配置文件迁移导致解密失败时，静默
        // 空串会被当成"密码未设置"——留日志区分真实失败与空态。
        qWarning("platformCredentialLoad: DPAPI unprotect failed for key, "
                 "credential unreadable");
        return {};
    }
#else
    QByteArray plain = xorCrypt(blob.mid(4), machineKey());
#endif
    if (blob.startsWith(kMagicV2)) {
        // v2: 明文 = "NDC1" + password——内部魔数校验密钥正确性（换主机名/
        // 密钥漂移时 XOR 输出乱码、魔数不匹配 → 按凭证丢失处理）。
        if (!plain.startsWith(kMagicV1)) {
            qWarning("platformCredentialLoad: credential integrity check "
                     "failed (hostname change or bit rot) — treating as lost");
            return {};
        }
        return QString::fromUtf8(plain.mid(kMagicV1.size()));
    }
    // v1（legacy，明文前缀魔数）: 无法验证密钥正确性——按可读内容尽力返回，
    // 下次保存即升级为 v2 格式。
    return QString::fromUtf8(plain);
}

bool platformCredentialRemove(const QString& key) {
    if (key.isEmpty()) return false;
    const QString path = credPath(key);
    // 条目不存在 = 已删除，视为成功（清除语义幂等）
    return !QFile::exists(path) || QFile::remove(path);
}

#endif // !defined(Q_OS_MACOS) && !defined(Q_OS_IOS)
