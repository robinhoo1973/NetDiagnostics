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
    const QString t = m_target; // pre-trimmed in setTarget()
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
    // 5WHY: m_target was stored untrimmed, forcing 7 Q_PROPERTY readers
    // (isEmpty, hasUrlScheme, isUrl, isHost, isHttpUrl, syncFieldsFromTarget,
    // and the validation block below) to each call m_target.trimmed() on every
    // targetChanged signal — up to 7 redundant QString allocations per keystroke.
    // Trim once here so all consumers use the pre-trimmed canonical value.
    const QString trimmed = t.trimmed();
    if (m_target != trimmed) {
        m_target = trimmed;
        m_error.clear();
        syncFieldsFromTarget();

        if (!trimmed.isEmpty()) {
            if (trimmed.contains("://")) {
                m_error = validateUrl(trimmed);
            } else {
                if (!isValidHostname(m_host)) {
                    m_error = m_host.contains("..")
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
    // 5WHY: When host becomes empty (e.g. QML writes targetHost=""),
    // the old code returned early without emitting targetChanged,
    // leaving QML bindings stale.  Clear the canonical target and
    // notify so consumers see the empty state.
    if (m_host.isEmpty()) {
        if (!m_target.isEmpty()) {
            m_target.clear();
            m_error.clear();
            emit targetChanged();
        }
        return;
    }

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

// ── Shared helper: split bare host input (no ://) into m_host / m_path ──
// Called by both syncFieldsFromTarget (setTarget path) and parseUrlIntoFields
// (QML direct-call path) to ensure consistent host/path splitting.
//
// 5WHY (round 5): applyBareHost was also resetting m_scheme to "https",
// m_port to -1, and clearing m_username/m_password unconditionally.  When
// parseUrlIntoFields is re-entered via the QML TextField binding round-trip
// (e.g. user typed "http://example.com:8080/path" → binding produces
// "example.com/path" → parseUrlIntoFields treats it as fresh bare input),
// the scheme, port, and credentials from the original URL were silently
// destroyed.  Now applyBareHost only splits host/path; callers apply
// their own defaults for scheme/port/credentials.
void TargetModel::applyBareHost(const QString& trimmed) {
    int slash = trimmed.indexOf(QLatin1Char('/'));
    if (slash >= 0) {
        m_host = trimmed.left(slash);
        m_path = trimmed.mid(slash);
    } else {
        m_host = trimmed;
        m_path.clear();
    }
}

// ── Reset all structured fields to sane defaults ─────────────────────────
// Shared by syncFieldsFromTarget and parseUrlIntoFields to avoid duplicate
// 6-field reset blocks that drift apart when new fields are added.
void TargetModel::clearFieldsToDefault() {
    m_scheme = QStringLiteral("https"); m_host.clear(); m_port = -1;
    m_username.clear(); m_password.clear(); m_path.clear();
}

// ── Parse m_target → structured fields ─────────────────────────────────
void TargetModel::syncFieldsFromTarget() {
    if (m_assembling) return;

    const QString trimmed = m_target; // pre-trimmed in setTarget()
    if (trimmed.isEmpty()) {
        // 5WHY (round 4): parseUrlIntoFields("") sets m_scheme = "https" so
        // the scheme dropdown shows a sane default after clearing.  Align
        // syncFieldsFromTarget (reached via setTarget("")) for consistency.
        clearFieldsToDefault();
        return;
    }

    if (!trimmed.contains(QStringLiteral("://"))) {
        // 5WHY: syncFieldsFromTarget must split host/path the same way
        // parseUrlIntoFields does, otherwise setTarget("host/path") lumps
        // the path into m_host and DNS resolution fails.
        applyBareHost(trimmed);
        // 5WHY (round 5): applyBareHost no longer resets scheme/port/creds.
        // syncFieldsFromTarget processes fresh input from setTarget(), so
        // apply the defaults here: scheme=https, port=-1, no credentials.
        m_scheme = QStringLiteral("https");
        m_port = -1;
        m_username.clear();
        m_password.clear();
        return;
    }

    QUrl u(trimmed, QUrl::TolerantMode);
    if (u.isValid() && !u.scheme().isEmpty()) {
        const QString sch = u.scheme().toLower();
        // 5WHY: syncFieldsFromTarget set m_scheme directly from QUrl without
        // the supportedSchemes gate that parseUrlIntoFields has.  An unsupported
        // scheme was stored in m_scheme (inconsistent internal state) even though
        // validateUrl() in setTarget() would correctly flag the error.
        m_scheme = ::supportedSchemes().contains(sch) ? sch : QStringLiteral("https");
        m_host = u.host();
        m_port = u.port() > 0 ? u.port() : -1;
        m_username = u.userName();
        m_password = u.password();

        // 5WHY: .section('@',0,0) splits on the FIRST '@', but RFC 3986
        // separates userinfo from host at the LAST '@' in the authority.
        // validateUrl() and extractHostname() both use lastIndexOf('@').
        if (m_username.isEmpty() && trimmed.contains('@')) {
            QString afterScheme = trimmed.section(QStringLiteral("://"), 1);
            int lastAt = afterScheme.lastIndexOf('@');
            if (lastAt >= 0) {
                QString userinfo = afterScheme.left(lastAt);
                if (userinfo.contains(':')) {
                    m_username = userinfo.section(':', 0, 0);
                    m_password = userinfo.section(':', 1);
                } else {
                    m_username = userinfo;
                }
            }
        }
        if (m_port <= 0) {
            QString authority = trimmed.section(QStringLiteral("://"), 1);
            if (authority.contains('@')) {
                int lastAt = authority.lastIndexOf('@');
                authority = authority.mid(lastAt + 1);
            }
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
    const QString trimmed = urlString.trimmed();
    // 5WHY: Returning early on empty input left stale m_host/m_path values.
    // When the user clears the QML TextField, onTextChanged fires with text="",
    // but parseUrlIntoFields("") returned immediately without clearing fields.
    // Clear all structured fields and emit targetChanged so QML bindings update.
    if (trimmed.isEmpty()) {
        clearFieldsToDefault();
        m_target.clear(); m_error.clear();
        emit targetChanged();
        return;
    }

    if (!trimmed.contains(QStringLiteral("://"))) {
        // 5WHY: Bare-domain input (no ://) was setting m_host/m_path but NOT
        // clearing m_scheme/m_port/m_username/m_password.  If the user previously
        // parsed a URL with credentials and then types a bare host/path, stale
        // credentials from the previous URL are silently preserved.
        // 5WHY (round 5): applyBareHost no longer resets scheme/port/creds.
        // This is correct for parseUrlIntoFields — the QML binding round-trip
        // produces bare host+path text, and we must preserve the scheme/port
        // that the user originally specified via a full URL paste.
        applyBareHost(trimmed);
        if (!m_host.isEmpty()) {
            assembleTargetUrl(); // setTarget() emits targetChanged
        } else {
            // 5WHY (round 5): path-only input (e.g. "/path") produces an
            // empty host — assembleTargetUrl would return early without
            // emitting targetChanged.  Emit directly so QML bindings know
            // the structured fields changed.
            m_target.clear();
            emit targetChanged();
        }
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

        // Userinfo fallback — handle @ in authority when QUrl misses it.
        // Mirrors syncFieldsFromTarget's fallback for cross-path consistency.
        if (m_username.isEmpty() && trimmed.contains('@')) {
            QString afterScheme = trimmed.section(QStringLiteral("://"), 1);
            int lastAt = afterScheme.lastIndexOf('@');
            if (lastAt >= 0) {
                QString userinfo = afterScheme.left(lastAt);
                if (userinfo.contains(':')) {
                    m_username = userinfo.section(':', 0, 0);
                    m_password = userinfo.section(':', 1);
                } else {
                    m_username = userinfo;
                }
            }
        }
        // Port fallback — handle explicit port when QUrl::port() returns -1.
        // Mirrors syncFieldsFromTarget's fallback for cross-path consistency.
        if (m_port <= 0) {
            QString authority = trimmed.section(QStringLiteral("://"), 1);
            if (authority.contains('@')) {
                int lastAt = authority.lastIndexOf('@');
                authority = authority.mid(lastAt + 1);
            }
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
        // 5WHY: Calling setTarget(trimmed) without the m_assembling guard
        // caused syncFieldsFromTarget() to re-parse the same raw URL via QUrl,
        // overwriting every field we just set — the scheme fallback (line 301)
        // was silently defeated and QUrl was constructed twice per keystroke.
        // Use assembleTargetUrl() which sets m_assembling=true before calling
        // setTarget(), so syncFieldsFromTarget returns immediately.
        // 5WHY (round 4): If QUrl returns an empty host (e.g. "http:///path"),
        // assembleTargetUrl() returns early at its isEmpty guard — no
        // targetChanged is emitted and m_target is never updated.  Fall
        // through to setTarget() so the raw input is stored and validated.
        if (!m_host.isEmpty()) {
            assembleTargetUrl(); // sets m_assembling → syncFieldsFromTarget is skipped
        } else {
            setTarget(trimmed);
        }
    } else {
        // QUrl couldn't parse — fall through to setTarget so the raw input
        // is stored and validated (validation error shown to user).
        setTarget(trimmed);
    }
}
