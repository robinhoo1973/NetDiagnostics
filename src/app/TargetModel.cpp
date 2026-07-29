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

// 5WHY: looksLikeIPv6 used host.contains(':') which matches ANY string
// with a colon — including "host:8080" and "user:pass".  Use count(':')>1
// so a single colon (indicating host:port or user:pass) is NOT treated as
// IPv6, while multiple colons (::1, 2001:db8::1, fe80::1) correctly are.
static bool looksLikeIPv6(const QString& host) {
    return host.count(':') > 1;
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
    // m_target is pre-trimmed in setTarget() — no copy needed
    if (!m_target.contains("://")) return false;
    const QString sch = m_target.section("://", 0, 0).toLower();
    return (sch == "http" || sch == "https");  // !isEmpty() redundant: empty target returns above
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
                // 5WHY: looksLikeIPv6 now correctly uses count(':')>1,
                // so bare "host:port" (single colon) is no longer
                // mistaken for IPv6 and reaches hostname-label validation
                // where ':' fails.  Strip userinfo and port before
                // validation — matching what extractHostname() does in
                // G4Common.h so validation stays aligned with the
                // diagnostic engine's capabilities.
                QString hostCheck = m_host;
                int atPos = hostCheck.lastIndexOf('@');
                if (atPos >= 0) hostCheck = hostCheck.mid(atPos + 1);
                // 5WHY (fix): The original count(':')==1 only strips
                // "host:port" (single colon).  IPv6 bracket notation
                // "[::1]:8080" has 3 colons — the port separator is
                // after the closing bracket, not caught by count(':')==1.
                // Strip port from bracket notation first, then fall back
                // to bare host:port for non-bracket hosts.
                if (hostCheck.startsWith(QLatin1Char('['))) {
                    int closing = hostCheck.indexOf(QLatin1Char(']'));
                    if (closing > 0) {
                        if (closing + 1 < hostCheck.size()
                            && hostCheck[closing + 1] == QLatin1Char(':')) {
                            hostCheck = hostCheck.left(closing + 1);
                        }
                        // Strip brackets to align with extractHostname:
                        // "[::1]" → "::1"
                        hostCheck = hostCheck.mid(1, closing - 1);
                    }
                } else {
                    // Single-colon = host:port.  Use dual-indexOf for
                    // early exit (~1.5x scan) instead of count+lastIndexOf
                    // (two full scans) — consistent with extractHostname.
                    auto colon = hostCheck.indexOf(QLatin1Char(':'));
                    if (colon > 0 && hostCheck.indexOf(QLatin1Char(':'), colon + 1) == -1)
                        hostCheck = hostCheck.left(colon);
                }
                if (!isValidHostname(hostCheck)) {
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
    // 5WHY: QUrl::host() strips brackets from IPv6 addresses (e.g. "[::1]" → "::1").
    // assembleTargetUrl must re-wrap bare IPv6 addresses so the reconstructed URL
    // is RFC 3986 compliant.  extractEmbeddedPortAndUserinfo preserves brackets in
    // m_host for the bare-input path, so the startsWith('[') guard prevents
    // double-wrapping.  Use looksLikeIPv6() (file-scope, host.count(':') > 1) for
    // a self-documenting check rather than an inline count.
    if (looksLikeIPv6(m_host) && !m_host.startsWith(QLatin1Char('[')))
        authority += QLatin1Char('[') + m_host + QLatin1Char(']');
    else
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
    // 5WHY (round 8): m_error was not cleared here, relying on callers to
    // do it (setTarget clears it before calling syncFieldsFromTarget, and
    // parseUrlIntoFields clears it after).  If a new code path ever calls
    // clearFieldsToDefault() without a separate m_error.clear(), a stale
    // validation error would survive the reset.  Make the function self-sufficient.
    m_scheme = QStringLiteral("https"); m_host.clear(); m_port = -1;
    m_username.clear(); m_password.clear(); m_path.clear();
    m_error.clear();
}

// Full reset + notify - shared by parseUrlIntoFields exit paths
// 5WHY: The 3-step sequence (clearFieldsToDefault + m_target.clear + emit)
// was duplicated in two branches.  Extracted so adding a field only needs
// to update clearFieldsToDefault, not both branches individually.
void TargetModel::resetAndNotify() {
    clearFieldsToDefault();
    m_target.clear();
    emit targetChanged();
}

// ── Shared: extract authority and parse userinfo/port fallback ───────────
// 5WHY: The authority extraction + userinfo fallback + port fallback block
// (~50 lines) was duplicated verbatim in syncFieldsFromTarget and
// parseUrlIntoFields.  Extract it so fixes apply once.
// Called after QUrl has already set m_host/m_port/m_username/m_password
// from its own parsing; only fills in what QUrl missed.
void TargetModel::parseAuthorityFields(const QString& trimmed) {
    // ── Extract authority from after-scheme (strip path/query/fragment) ──
    QString afterScheme = trimmed.section(QStringLiteral("://"), 1);
    int authorityEnd = afterScheme.size();
    for (auto ch : {'/', '?', '#'}) {
        int pos = afterScheme.indexOf(ch);
        if (pos >= 0 && pos < authorityEnd) authorityEnd = pos;
    }
    QString authority = afterScheme.left(authorityEnd);

    // ── Userinfo fallback — handle @ in authority when QUrl misses it ──
    if (m_username.isEmpty() && authority.contains('@')) {
        int lastAt = authority.lastIndexOf('@');
        QString userinfo = authority.left(lastAt);
        if (userinfo.contains(':')) {
            m_username = userinfo.section(':', 0, 0);
            m_password = userinfo.section(':', 1);
        } else {
            m_username = userinfo;
            // 5WHY: extractEmbeddedPortAndUserinfo() clears m_password in the
            // same scenario — keep both userinfo-fallback paths consistent.
            m_password.clear();
        }
    }

    // Port fallback — handle explicit port when QUrl::port() returns -1
    if (m_port < 0) {
        QString hostPort = authority;
        if (hostPort.contains('@')) {
            int lastAt = hostPort.lastIndexOf('@');
            hostPort = hostPort.mid(lastAt + 1);
        }
        // IPv6 bracket notation: port follows the closing bracket
        if (hostPort.startsWith('[')) {
            int closing = hostPort.indexOf(']');
            if (closing > 0 && closing + 1 < hostPort.size() && hostPort[closing + 1] == ':') {
                bool ok = false;
                int p = hostPort.mid(closing + 2).toInt(&ok);
                if (ok && p > 0 && p <= 65535) m_port = p;
            }
        // 5WHY: !looksLikeIPv6(hostPort) was always false because
        // looksLikeIPv6 is host.contains(':').  The expression reduced
        // to contains(':') && !contains(':') — dead code.  Use count(':')
        // instead: 1 colon = host:port, >1 colons = bare IPv6 address.
        } else if (hostPort.count(':') == 1) {
            // Bare IPv6 addresses (e.g. ::1) contain colons as part of the
            // address, not as a port separator.  Skip the fallback when the
            // string looks like a bare IPv6 address.
            bool ok = false;
            int p = hostPort.section(':', -1).toInt(&ok);
            if (ok && p > 0 && p <= 65535) m_port = p;
        }
    }
}

// ── Shared: extract embedded port + userinfo from bare-input m_host ─────
// 5WHY: The bracket-aware port extraction and userinfo extraction blocks
// (~45 lines total) were copy-pasted verbatim in both syncFieldsFromTarget
// and parseUrlIntoFields.  Extracting them into a shared helper eliminates
// the divergence risk — a fix to one copy was a latent bug in the other.
// Called after applyBareHost() to finalize structured fields for bare input.
void TargetModel::extractEmbeddedPortAndUserinfo() {
    // Extract embedded port from m_host (bracket-aware, userinfo-stripped)
    {
        QString hostPart = m_host;
        int atPos = hostPart.lastIndexOf(QLatin1Char('@'));
        if (atPos >= 0) hostPart = hostPart.mid(atPos + 1);

        if (hostPart.startsWith(QLatin1Char('['))) {
            int closing = hostPart.indexOf(QLatin1Char(']'));
            if (closing > 0 && closing + 1 < hostPart.size()
                && hostPart[closing + 1] == QLatin1Char(':')) {
                bool ok = false;
                int p = hostPart.mid(closing + 2).toInt(&ok);
                if (ok && p > 0 && p <= 65535) {
                    m_port = p;
                    int bracketPos = atPos >= 0 ? atPos + 1 : 0;
                    // Keeps bracket notation in m_host (e.g. "[::1]"
                    // instead of "::1") so assembleTargetUrl's bracket-
                    // wrapping guard (looksLikeIPv6 && !startsWith('['))
                    // sees the brackets and skips re-wrapping — prevents
                    // double-wrapped "[[]:1]".  extractHostname strips
                    // brackets for raw socket use — the two roles are
                    // intentionally different.
                    m_host = m_host.left(bracketPos + closing + 1);
                }
            }
        } else {
            // Single-colon = host:port.  Use dual-indexOf for early
            // exit (~1.5x scan) instead of count+lastIndexOf
            // (two full scans) — consistent with extractHostname.
            auto portSep = hostPart.indexOf(QLatin1Char(':'));
            if (portSep > 0 && hostPart.indexOf(QLatin1Char(':'), portSep + 1) == -1) {
                bool ok = false;
                int p = hostPart.mid(portSep + 1).toInt(&ok);
                if (ok && p > 0 && p <= 65535) {
                    m_port = p;
                    int portInHost = m_host.lastIndexOf(QLatin1Char(':'));
                    if (portInHost > 0) m_host = m_host.left(portInHost);
                }
            }
        }
    }
    // Extract userinfo from m_host (may have been left behind by
    // applyBareHost in bare input like "user:pass@host:9090")
    {
        int atPos = m_host.lastIndexOf(QLatin1Char('@'));
        if (atPos >= 0) {
            const QString userinfo = m_host.left(atPos);
            const int colonPos = userinfo.indexOf(QLatin1Char(':'));
            if (colonPos >= 0) {
                m_username = userinfo.left(colonPos);
                m_password = userinfo.mid(colonPos + 1);
            } else {
                m_username = userinfo;
                // 5WHY: m_password.clear() is defense-in-depth.  In
                // syncFieldsFromTarget, m_password was already cleared
                // (fresh input), so this is a no-op.  In parseUrlIntoFields,
                // a stale password from a previous URL would otherwise
                // silently leak when bare input has no password.
                m_password.clear();
            }
            m_host = m_host.mid(atPos + 1);
        }
    }
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
        // 5WHY (fix): When setTarget receives bare input like "[::1]:8080"
        // or "host:9090", applyBareHost stores the port as part of m_host.
        // Setting m_port=-1 without extracting the embedded port leaves
        // m_host with the port suffix while the structured port field is
        // reset — the QML host field shows "[::1]:8080" instead of "::1",
        // and assembleTargetUrl would later append a second port separator.
        // Delegate to shared helper (also used by parseUrlIntoFields) so
        // both bare-input code paths stay in sync by construction.
        m_scheme = QStringLiteral("https");
        m_port = -1;
        m_username.clear();
        m_password.clear();
        extractEmbeddedPortAndUserinfo();
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

        // ── Authority + userinfo fallback + port fallback (shared helper) ──
        parseAuthorityFields(trimmed);

        QString fullPath = u.path();
        if (u.hasQuery()) fullPath += QLatin1Char('?') + u.query();
        if (u.hasFragment()) fullPath += QLatin1Char('#') + u.fragment();
        m_path = fullPath;
    } else {
        // 5WHY: When QUrl cannot parse the input, setting only m_host = trimmed
        // (the full URL string) while leaving m_scheme/m_port/m_username/m_password/
        // m_path at their previous values creates inconsistent internal state.
        // 5WHY: Use clearFieldsToDefault() instead of manually resetting
        // the same 6 fields — the shared helper is self-sufficient and
        // ensures consistency when new fields are added.
        clearFieldsToDefault();
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
        resetAndNotify();
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
        // 5WHY (fix): When the bare input contains an embedded port or
        // userinfo (e.g. "[::1]:8080", "host:9090", "user:pass@host"),
        // applyBareHost stores them as part of m_host.  Delegate to the
        // shared helper (also used by syncFieldsFromTarget) to extract
        // port and userinfo, update structured fields, and strip suffixes
        // from m_host.  Both code paths stay in sync by construction.
        extractEmbeddedPortAndUserinfo();
        if (!m_host.isEmpty()) {
            assembleTargetUrl(); // setTarget() emits targetChanged
        } else {
            // 5WHY (round 5): path-only input (e.g. "/path") produces an
            // empty host — assembleTargetUrl would return early without
            // emitting targetChanged.  Emit directly so QML bindings know
            // the structured fields changed.
            // 5WHY (round 12): Only m_target and m_error were cleared,
            // leaving m_path='/path', m_scheme, m_port, etc. with stale
            // values from the previous URL.  Call clearFieldsToDefault()
            // to reset all fields — matching the empty-input path above.
            resetAndNotify();
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

        // ── Authority + userinfo fallback + port fallback (shared helper) ──
        parseAuthorityFields(trimmed);

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
