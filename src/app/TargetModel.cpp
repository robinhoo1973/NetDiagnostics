// =============================================================================
// TargetModel.cpp — URL target parsing, assembly, and validation
// =============================================================================
#include "app/TargetModel.h"
#include "Diagnostics/Model/G5/G5WebsiteUrl.h"
#include <QUrl>

// ── RFC 952/1123 hostname label validation ────────────────────────────────
static bool isValidHostLabel(const QString& label) {
    if (label.isEmpty() || label.size() > 63) return false;
    for (int i = 0; i < label.size(); ++i) {
        QChar c = label[i];
        if (!c.isLetterOrNumber() && c != '-') return false;
    }
    if (label.startsWith('-') || label.endsWith('-')) return false;
    return true;
}

static bool isValidIPv4(const QString& host) {
    const auto parts = host.split('.');
    if (parts.size() != 4) return false;
    for (const auto& p : parts) {
        bool ok = false;
        int v = p.toInt(&ok);
        if (!ok || v < 0 || v > 255) return false;
        if (p.size() > 1 && p.startsWith('0')) return false;
    }
    return true;
}

static bool looksLikeIPv6(const QString& host) {
    return host.contains(':');
}

static bool isValidHostname(const QString& host) {
    if (host.isEmpty() || host.size() > 253) return false;
    if (host.contains("..") || host == ".") return false;
    if (looksLikeIPv6(host)) return true;
    if (isValidIPv4(host)) return true;
    const auto labels = host.split('.');
    for (const auto& label : labels) {
        if (!isValidHostLabel(label)) return false;
    }
    return true;
}

static const QStringList& supportedSchemes() {
    static const QStringList s = G5WebsiteUrl::knownSchemes();
    return s;
}

static QString validateUrl(const QString& trimmed) {
    auto schemeEnd = trimmed.indexOf("://");
    if (schemeEnd < 0) return QString();

    QString scheme = trimmed.left(schemeEnd).toLower();
    if (scheme.isEmpty()) return QStringLiteral("Empty URL scheme");
    if (!supportedSchemes().contains(scheme))
        return QStringLiteral("Unsupported protocol: %1:// — supported schemes: %2")
            .arg(scheme, supportedSchemes().join(", "));

    QString afterScheme = trimmed.mid(schemeEnd + 3);
    int pathStart = afterScheme.indexOf('/');
    int queryStart = afterScheme.indexOf('?');
    int fragStart = afterScheme.indexOf('#');

    int authorityEnd = afterScheme.size();
    if (pathStart >= 0) authorityEnd = std::min(authorityEnd, pathStart);
    if (queryStart >= 0) authorityEnd = std::min(authorityEnd, queryStart);
    if (fragStart >= 0) authorityEnd = std::min(authorityEnd, fragStart);

    QString authority = afterScheme.left(authorityEnd);
    if (authority.isEmpty()) return QStringLiteral("URL has no hostname");

    if (authority.contains('@')) {
        int atPos = authority.lastIndexOf('@');
        authority = authority.mid(atPos + 1);
        if (authority.isEmpty()) return QStringLiteral("URL has no hostname after userinfo");
    }

    QString host, portStr;
    int portColon = authority.lastIndexOf(':');
    if (authority.startsWith('[')) {
        int closing = authority.indexOf(']');
        if (closing < 0) return QStringLiteral("Invalid IPv6 bracket notation");
        host = authority.mid(1, closing - 1);
        if (closing + 1 < authority.size()) {
            if (authority[closing + 1] != ':') return QStringLiteral("Expected colon after IPv6 bracket");
            portStr = authority.mid(closing + 2);
        }
    } else if (portColon > 0) {
        host = authority.left(portColon);
        portStr = authority.mid(portColon + 1);
    } else {
        host = authority;
    }

    if (host.isEmpty()) return QStringLiteral("URL has no hostname");
    if (!isValidHostname(host)) {
        if (host.contains("..")) return QStringLiteral("Invalid hostname: consecutive dots");
        return QStringLiteral("Hostname label must be 1-63 alphanumeric chars (a-z, 0-9, -) and cannot start/end with hyphen");
    }

    if (!portStr.isEmpty()) {
        bool ok = false;
        int port = portStr.toInt(&ok);
        if (!ok) return QStringLiteral("Port must be a number");
        if (port < 1 || port > 65535)
            return QStringLiteral("Port must be between 1 and 65535 (got %1)").arg(port);
    }

    return QString();
}

// =============================================================================
// TargetModel implementation
// =============================================================================

TargetModel::TargetModel(QObject* parent) : QObject(parent) {
    m_scheme = QStringLiteral("https");
}

QStringList TargetModel::supportedSchemes() const { return ::supportedSchemes(); }
int TargetModel::defaultPort() const { return G5WebsiteUrl::defaultPortForScheme(m_scheme); }

bool TargetModel::isHttpUrl() const {
    const QString t = m_target.trimmed();
    if (!t.contains("://")) return false;
    const QString sch = t.section("://", 0, 0).toLower();
    return (sch == "http" || sch == "https") && !isEmpty();
}

// ── Structured field setters ────────────────────────────────────────────
void TargetModel::setScheme(const QString& s) {
    if (m_scheme != s) { m_scheme = s; assembleTargetUrl(); } // setTarget() emits
}
void TargetModel::setHost(const QString& h) {
    if (m_host != h) { m_host = h; assembleTargetUrl(); }
}
void TargetModel::setPort(int p) {
    if (m_port != p) { m_port = p; assembleTargetUrl(); }
}
void TargetModel::setUsername(const QString& u) {
    if (m_username != u) { m_username = u; assembleTargetUrl(); }
}
void TargetModel::setPassword(const QString& p) {
    if (m_password != p) { m_password = p; assembleTargetUrl(); }
}
void TargetModel::setPath(const QString& p) {
    if (m_path != p) { m_path = p; assembleTargetUrl(); }
}

// ── Canonical target ────────────────────────────────────────────────────
void TargetModel::setTarget(const QString& t) {
    if (m_target != t) {
        m_target = t;
        m_error.clear();
        syncFieldsFromTarget();

        const QString trimmed = m_target.trimmed();
        if (!trimmed.isEmpty()) {
            if (trimmed.contains("://")) {
                m_error = validateUrl(trimmed);
            } else {
                if (!isValidHostname(trimmed)) {
                    m_error = trimmed.contains("..")
                        ? QStringLiteral("Invalid hostname: consecutive dots")
                        : QStringLiteral("Hostname label must be 1-63 alphanumeric chars (a-z, 0-9, -) and cannot start/end with hyphen");
                }
            }
        }
        emit targetChanged();
    }
}

// ── Build m_target from structured fields ───────────────────────────────
void TargetModel::assembleTargetUrl() {
    if (m_assembling) return;
    if (m_host.isEmpty()) return;

    const QString sch = m_scheme.isEmpty() ? QStringLiteral("https") : m_scheme;
    const int defPort = G5WebsiteUrl::defaultPortForScheme(sch);

    QString authority;
    if (!m_username.isEmpty()) {
        authority += QString::fromUtf8(QUrl::toPercentEncoding(m_username));
        if (!m_password.isEmpty())
            authority += QLatin1Char(':') + QString::fromUtf8(QUrl::toPercentEncoding(m_password));
        authority += QLatin1Char('@');
    }
    authority += m_host;
    if (m_port > 0 && m_port != defPort)
        authority += QLatin1Char(':') + QString::number(m_port);

    const QString url = sch + QStringLiteral("://") + authority + m_path;

    m_assembling = true;
    setTarget(url);
    m_assembling = false;
}

// ── Parse m_target → structured fields ─────────────────────────────────
void TargetModel::syncFieldsFromTarget() {
    if (m_assembling) return;

    const QString trimmed = m_target.trimmed();
    if (trimmed.isEmpty()) {
        m_scheme.clear(); m_host.clear(); m_port = -1;
        m_username.clear(); m_password.clear(); m_path.clear();
        return;
    }

    if (!trimmed.contains(QStringLiteral("://"))) {
        m_scheme = QStringLiteral("https"); m_host = trimmed; m_port = -1;
        m_username.clear(); m_password.clear(); m_path.clear();
        return;
    }

    QUrl u(trimmed, QUrl::TolerantMode);
    if (u.isValid() && !u.scheme().isEmpty()) {
        m_scheme = u.scheme().toLower();
        m_host = u.host();
        m_port = u.port() > 0 ? u.port() : -1;
        m_username = u.userName();
        m_password = u.password();

        if (m_username.isEmpty() && trimmed.contains('@')) {
            QString userinfo = trimmed.section(QStringLiteral("://"), 1).section('@', 0, 0);
            if (userinfo.contains(':')) {
                m_username = userinfo.section(':', 0, 0);
                m_password = userinfo.section(':', 1);
            } else {
                m_username = userinfo;
            }
        }
        if (m_port <= 0) {
            QString authority = trimmed.section(QStringLiteral("://"), 1);
            if (authority.contains('@')) authority = authority.section('@', 1);
            for (auto ch : {'/', '?', '#'}) {
                int pos = authority.indexOf(ch);
                if (pos >= 0) authority = authority.left(pos);
            }
            if (authority.contains(':')) {
                bool ok = false;
                int p = authority.section(':', -1).toInt(&ok);
                if (ok && p > 0 && p <= 65535) m_port = p;
            }
        }

        QString fullPath = u.path();
        if (u.hasQuery()) fullPath += QLatin1Char('?') + u.query();
        if (u.hasFragment()) fullPath += QLatin1Char('#') + u.fragment();
        m_path = fullPath;
    } else {
        m_host = trimmed;
    }
}

// ── QML-invokable: parse pasted URL into fields ─────────────────────────
void TargetModel::parseUrlIntoFields(const QString& urlString) {
    if (urlString.trimmed().isEmpty()) return;

    const QString trimmed = urlString.trimmed();
    if (!trimmed.contains(QStringLiteral("://"))) {
        m_host = trimmed;
        assembleTargetUrl(); // setTarget() emits targetChanged
        return;
    }

    QUrl u(trimmed, QUrl::TolerantMode);
    if (u.isValid() && !u.scheme().isEmpty()) {
        const QString sch = u.scheme().toLower();
        m_scheme = ::supportedSchemes().contains(sch) ? sch : QStringLiteral("https");
        m_host = u.host();
        m_port = u.port() > 0 ? u.port() : -1;
        m_username = u.userName();
        m_password = u.password();
        QString fullPath = u.path();
        if (u.hasQuery()) fullPath += QLatin1Char('?') + u.query();
        if (u.hasFragment()) fullPath += QLatin1Char('#') + u.fragment();
        m_path = fullPath;
        setTarget(trimmed);
    }
}
