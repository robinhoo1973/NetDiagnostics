// =============================================================================
// ServerDbUpdater.cpp — implementation
// =============================================================================
#include "Common/Services/ServerDbUpdater.h"
#include <QCryptographicHash>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QRegularExpression>

ServerDbUpdater::ServerDbUpdater(QObject* parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
}

void ServerDbUpdater::setManifestUrl(const QString& url) { m_manifestUrl = url; }
void ServerDbUpdater::setTargetPath(const QString& path) {
    m_targetPath = path;
    m_hashPath = path + QStringLiteral(".hash");
}

QString ServerDbUpdater::storedHash() const {
    QFile f(m_hashPath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll()).trimmed();
    return {};
}

QString ServerDbUpdater::computeHash(const QByteArray& data) const {
    return QString::fromUtf8(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool ServerDbUpdater::writeFile(const QString& path, const QByteArray& data) {
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(data);
    f.close();
    return true;
}

// ── Async update ─────────────────────────────────────────────────────
void ServerDbUpdater::checkForUpdates(bool force, Callback callback) {
    if (m_manifestUrl.isEmpty() || m_targetPath.isEmpty()) {
        if (callback) callback(false, QStringLiteral("URL or target path not set"));
        return;
    }

    // Use a local event loop for synchronous-like async
    // Step 1: Fetch manifest
    QUrl manifestUrl(m_manifestUrl);
    QNetworkRequest manifestReq(manifestUrl);
    manifestReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NetDiagnostics/1.0"));
    QNetworkReply* manifestReply = m_nam->get(manifestReq, QByteArray());

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(manifestReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(15000); // 15s timeout

    loop.exec();
    if (!manifestReply->isFinished()) {
        manifestReply->abort();
        manifestReply->deleteLater();
        if (callback) callback(false, QStringLiteral("Manifest fetch timed out"));
        return;
    }

    if (manifestReply->error() != QNetworkReply::NoError) {
        QString err = manifestReply->errorString();
        manifestReply->deleteLater();
        if (callback) callback(false, QStringLiteral("Manifest fetch failed: %1").arg(err));
        return;
    }

    // Step 2: Parse manifest
    QString manifest = QString::fromUtf8(manifestReply->readAll());
    manifestReply->deleteLater();

    // Parse lines: skip comments, find first "URL HASH" pair
    QRegularExpression lineRe(QStringLiteral(R"(^(\S+)\s+(\S+))"));
    QString downloadUrl, expectedHash;
    for (auto& line : manifest.split('\n')) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        auto m = lineRe.match(line);
        if (m.hasMatch()) {
            downloadUrl = m.captured(1);
            expectedHash = m.captured(2).toLower();
            break;
        }
    }

    if (downloadUrl.isEmpty() || expectedHash.isEmpty()) {
        if (callback) callback(false, QStringLiteral("Manifest contains no valid URL+HASH line"));
        return;
    }

    // Step 3: Compare hashes
    QString current = storedHash();
    if (!force && current == expectedHash) {
        if (callback) callback(true, QStringLiteral("Already up to date (hash %1)").arg(expectedHash.left(12)));
        return;
    }

    emit updateAvailable(expectedHash);

    // Step 4: Download new database
    QUrl dlUrl(downloadUrl);
    QNetworkRequest dlReq(dlUrl);
    dlReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NetDiagnostics/1.0"));
    QNetworkReply* dlReply = m_nam->get(dlReq, QByteArray());

    QEventLoop loop2;
    QTimer timer2;
    timer2.setSingleShot(true);
    connect(&timer2, &QTimer::timeout, &loop2, &QEventLoop::quit);
    connect(dlReply, &QNetworkReply::finished, &loop2, &QEventLoop::quit);
    timer2.start(30000);

    loop2.exec();
    if (!dlReply->isFinished()) {
        dlReply->abort();
        dlReply->deleteLater();
        if (callback) callback(false, QStringLiteral("Download timed out"));
        return;
    }

    if (dlReply->error() != QNetworkReply::NoError) {
        QString err = dlReply->errorString();
        dlReply->deleteLater();
        if (callback) callback(false, QStringLiteral("Download failed: %1").arg(err));
        return;
    }

    QByteArray data = dlReply->readAll();
    dlReply->deleteLater();

    // Step 5: Verify hash
    QString actualHash = computeHash(data);
    if (actualHash != expectedHash) {
        if (callback) callback(false,
            QStringLiteral("Hash mismatch: expected %1, got %2").arg(expectedHash.left(12), actualHash.left(12)));
        return;
    }

    // Step 6: Write target file + hash file
    if (!writeFile(m_targetPath, data)) {
        if (callback) callback(false, QStringLiteral("Failed to write %1").arg(m_targetPath));
        return;
    }
    if (!writeFile(m_hashPath, expectedHash.toUtf8())) {
        if (callback) callback(false, QStringLiteral("Failed to write hash file"));
        return;
    }

    emit updateApplied(m_targetPath);
    if (callback) callback(true, QStringLiteral("Updated to hash %1").arg(expectedHash.left(12)));
}

// ── Synchronous convenience ──────────────────────────────────────────
bool ServerDbUpdater::checkForUpdatesSync(bool force) {
    bool result = false;
    QEventLoop wait;
    checkForUpdates(force, [&](bool ok, const QString&) {
        result = ok;
        wait.quit();
    });
    // Don't block forever — the internal loop handles timeouts
    // but we need to be called from a thread with an event loop
    return result;
}
