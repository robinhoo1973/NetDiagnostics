// =============================================================================
// TargetModel.h — URL target parsing, assembly, and validation
//
// Extracted from AppState to reduce the God Object.  Owns the canonical target
// string and its structured fields (scheme, host, port, username, password,
// path), URL validation, and assembly/disassembly.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class TargetModel : public QObject {
    Q_OBJECT

    // ── Properties ──────────────────────────────────────────────────────
    Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(QString scheme READ scheme WRITE setScheme NOTIFY targetChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY targetChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY targetChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY targetChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY targetChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY targetChanged)
    Q_PROPERTY(QStringList supportedSchemes READ supportedSchemes CONSTANT)
    Q_PROPERTY(int defaultPort READ defaultPort NOTIFY targetChanged)

    // Derived state (read-only for QML)
    Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY targetChanged)
    Q_PROPERTY(bool hasUrlScheme READ hasUrlScheme NOTIFY targetChanged)
    Q_PROPERTY(bool isHttpUrl READ isHttpUrl NOTIFY targetChanged)
    Q_PROPERTY(bool isUrl READ isUrl NOTIFY targetChanged)
    Q_PROPERTY(bool isHost READ isHost NOTIFY targetChanged)
    Q_PROPERTY(QString validationError READ validationError NOTIFY targetChanged)

public:
    explicit TargetModel(QObject* parent = nullptr);

    // ── Canonical target string ─────────────────────────────────────────
    QString target() const { return m_target; }
    void setTarget(const QString& t);

    // ── Structured field accessors ──────────────────────────────────────
    QString scheme() const { return m_scheme; }
    void setScheme(const QString& s);
    QString host() const { return m_host; }
    void setHost(const QString& h);
    int port() const { return m_port; }
    void setPort(int p);
    QString username() const { return m_username; }
    void setUsername(const QString& u);
    QString password() const { return m_password; }
    void setPassword(const QString& p);
    QString path() const { return m_path; }
    void setPath(const QString& p);

    // ── Scheme helpers ──────────────────────────────────────────────────
    QStringList supportedSchemes() const;
    int defaultPort() const;

    // ── Derived state ───────────────────────────────────────────────────
    bool isEmpty() const { return m_target.trimmed().isEmpty(); }
    bool hasUrlScheme() const { return m_target.contains("://") && !isEmpty(); }
    bool isHttpUrl() const;
    bool isUrl() const { return hasUrlScheme() && !isEmpty(); }
    bool isHost() const { return !isEmpty() && !hasUrlScheme(); }
    QString validationError() const { return m_error; }

    // ── QML helpers ─────────────────────────────────────────────────────
    // NOTE: canRun() is in AppState (needs runStatus + group state).
    Q_INVOKABLE void parseUrlIntoFields(const QString& urlString);

signals:
    void targetChanged();

private:
    void assembleTargetUrl();      // build m_target from structured fields
    void syncFieldsFromTarget();   // parse m_target → structured fields

    // ── Static validation helpers ───────────────────────────────────────
    static bool isValidHostLabel(const QString& label);
    static bool isValidIPv4(const QString& host);
    static bool looksLikeIPv6(const QString& host);
    static bool isValidHostname(const QString& host);
    static QString validateUrl(const QString& trimmed);

    QString m_target;
    QString m_scheme;
    QString m_host;
    int m_port = -1;
    QString m_username;
    QString m_password;
    QString m_path;
    QString m_error;
    bool m_assembling = false;
};
