// =============================================================================
// AtomicWriteFile.h — QSaveFile 原子写公共助手
//
// 5WHY (simplify 2026-09-04): persistResults 与 platformCredentialSave 各自
// 手写 open→write→commit→cancel 序列（~25 行×2，含 mkpath/目录解析/0600
// 收紧的微妙顺序），未来落盘类修正必须两处同步落地。收敛为单一助手：
//   · mkpath 父目录（目录解析惯例也一并收口）
//   · QSaveFile 原子写 + commit（flush/fsync 持久性）
//   · restrictOwner 收紧 0600，chmod 失败删除并失败（fail-closed）
// 失败时旧文件保持完好（QSaveFile 契约），调用方只需按返回值告警。
// =============================================================================
#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

inline bool atomicWriteFile(const QString& path, const QByteArray& data,
                            bool restrictOwner = false)
{
    const QString dir = QFileInfo(path).absolutePath();
    if (!dir.isEmpty() && !QDir().mkpath(dir))
        return false;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    if (f.write(data) != data.size()) {
        f.cancelWriting();   // 移除临时文件，旧文件保持原样
        return false;
    }
    if (!f.commit()) return false;
    if (restrictOwner) {
        // QSaveFile 以默认 umask 创建新文件（0644）——敏感数据须收紧。
        // chmod 失败（FAT/exFAT/NFS 等）即删除并返回 false（fail-closed）。
        QFile chk(path);
        if (!chk.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
            QFile::remove(path);
            return false;
        }
    }
    return true;
}
