// =============================================================================
// ServerDbUpdater.h — Auto-update server database from remote manifest
//
// Manifest format (e.g. latest-sites.txt):
//   # comments ignored
//   <URL> <SHA256_HEX>
//
// Flow:
//   1. Fetch manifest from configured URL
//   2. Parse URL + expected hash
//   3. Compare with locally stored hash (.hash file)
//   4. If identical → skip download
//   5. If different → download, verify hash, atomically replace DB
// =============================================================================
#pragma once
#include <QString>
#include <QObject>
#include <QNetworkAccessManager>
#include <functional>

class ServerDbUpdater : public QObject {
    Q_OBJECT
public:
    // Callback: (success, message)
    using Callback = std::function<void(bool, const QString&)>;

    explicit ServerDbUpdater(QObject* parent = nullptr);

    // Set the manifest URL (e.g. "https://github.com/.../latest-sites.txt")
    void setManifestUrl(const QString& url);

    // Set target database file path (e.g. ".../G3ServerDb.inc")
    void setTargetPath(const QString& path);

    // Check for updates (async).  Calls callback when done.
    // If force=true, skip hash comparison and always download.
    void checkForUpdates(bool force = false, Callback callback = nullptr);

    // Synchronous version — returns true if update was applied
    bool checkForUpdatesSync(bool force = false);

    // Get stored hash (empty if never updated)
    QString storedHash() const;

signals:
    void updateAvailable(const QString& newHash);
    void updateApplied(const QString& path);
    void updateFailed(const QString& error);

private:
    QString m_manifestUrl;
    QString m_targetPath;
    QString m_hashPath;   // m_targetPath + ".hash"

    QString computeHash(const QByteArray& data) const;
    bool writeFile(const QString& path, const QByteArray& data);
};
