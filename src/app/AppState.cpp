// =============================================================================
// AppState.cpp
// =============================================================================
#include "app/AppState.h"
#include "engine/diagnostics/G5/G5WebsiteUrl.h"
#include "engine/task/TaskFactory.h"
#include "util/DebugSwitch.h"
#include "util/Logger.h"
#include <cstdio>
#include <chrono>
#include <QTimer>
#include <QUrl>
#include <QCoreApplication>
#include <QTextDocument>
#include <QPdfWriter>
#include <QFont>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QUrl>
#include <QUrlQuery>
#include <QFileInfo>
#include <QProcess>
#include <QDesktopServices>
#include <QSettings>
#include "platform/PlatformShare.h"
#if defined(PLATFORM_IOS)
#include "platform/PlatformStore.h"
#endif
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QDialog>
#endif
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QFileDialog>
#endif

AppState::AppState(QObject* parent) : QObject(parent) {
    m_targetScheme = QStringLiteral("https"); // default protocol
    // G1-G3 active by default (G4/G5 auto-managed via setTarget)
    m_activeGroups = {0, 1, 2};
    // Forward service signals to QML
    connect(&m_reportEngine, &ReportEngine::savePathPicked,
            this, &AppState::savePathPicked);
    // Forward PremiumStore signals to QML
    connect(&m_premium, &PremiumStore::premiumChanged,
            this, &AppState::premiumChanged);
    connect(&m_premium, &PremiumStore::purchaseInProgressChanged,
            this, &AppState::purchaseInProgressChanged);
    connect(&m_premium, &PremiumStore::premiumRequired,
            this, &AppState::premiumRequired);
    connect(&m_premium, &PremiumStore::restoreCompleted,
            this, &AppState::restoreCompleted);

    // Enable G1-G3 by default; G4/G5 are auto-managed based on target
    m_config.enableDefaultGroups();

    // NOTE: iOS-unavailable tests (TCP settings, ARP table) are NOT hidden here.
    // They stay enabled and report DiagStatus::Skipped so the UI shows a skip
    // icon (like Active Connections). Default gateway / routing table / DHCP now
    // have working iOS implementations and return real data.

    // Restore persisted settings (language, active groups, enabled diags).
    // Must be called AFTER enableDefaultGroups so we override defaults with
    // saved values. A first-run user has no saved state and keeps defaults.
    loadSettings();
}

AppState::~AppState() {
    if (m_runStatus == RunStatus::Running) {
        m_runStatus = RunStatus::Cancelled;
    }
}

// ── App version / edition / build number ─────────────────────────────────
QString AppState::appVersion() const {
    const QString v = QCoreApplication::applicationVersion();
    return v.isEmpty() ? QStringLiteral("0.0.1") : v;
}

QString AppState::appEdition() const {
#ifdef APP_EDITION
    return QStringLiteral(APP_EDITION);
#else
    return QString();
#endif
}

QString AppState::buildNumber() const {
#if defined(__APPLE__)
    // Read CFBundleVersion from the app bundle Info.plist (CI sets this to the
    // GitHub run number, e.g. "379"). CoreFoundation is a C API — usable here
    // without Objective-C.
    if (CFBundleRef bundle = CFBundleGetMainBundle()) {
        CFTypeRef v = CFBundleGetValueForInfoDictionaryKey(bundle, kCFBundleVersionKey);
        if (v && CFGetTypeID(v) == CFStringGetTypeID()) {
            char buf[64] = {0};
            if (CFStringGetCString((CFStringRef)v, buf, sizeof(buf), kCFStringEncodingUTF8))
                return QString::fromUtf8(buf);
        }
    }
#endif
#ifdef ND_BUILD_NUMBER
    return QStringLiteral(ND_BUILD_NUMBER);
#else
    return QString();
#endif
}

// ── State version — called at end of every mutation method ──────────────
void AppState::bumpVersion() {
    m_stateGeneration.fetch_add(1, std::memory_order_release);
    emit stateVersionChanged();
}

// ── Language switching ──────────────────────────────────────────────────
// 0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW,7=ES,8=PT
void AppState::setLanguage(int index) {
    if (index < 0 || index > 8) return;
    m_languageIndex = index;
    emit languageChanged();
    saveSettings();
    bumpVersion();
    TRACE(" Language set to index %d\n", index);
}

// ── Theme mode persistence ─────────────────────────────────────────────
// 0=system, 1=light, 2=dark (matches ThemeEngine.sysMode/litMode/drkMode)
void AppState::setThemeMode(int mode) {
    if (mode < 0 || mode > 2) return;
    if (m_themeMode == mode) return;
    m_themeMode = mode;
    emit themeChanged();
    saveSettings();
    bumpVersion();
    TRACE(" Theme mode set to %d\n", mode);
}

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

// ── Supported URL schemes (must be lowercase) ────────────────────────────
// Delegates to G5WebsiteUrl::knownSchemes() — single source of truth.
static const QStringList& supportedSchemes() {
    static const QStringList s = G5WebsiteUrl::knownSchemes();
    return s;
}

QStringList AppState::supportedSchemes() const {
    return ::supportedSchemes();
}

// ── Structured target field accessors ──────────────────────────────────────
QString AppState::targetScheme() const { return m_targetScheme; }
void AppState::setTargetScheme(const QString& s) {
    if (m_targetScheme != s) {
        m_targetScheme = s;
        assembleTargetUrl();
        emit targetChanged(); bumpVersion();
    }
}

QString AppState::targetHost() const { return m_targetHost; }
void AppState::setTargetHost(const QString& h) {
    if (m_targetHost != h) {
        m_targetHost = h;
        assembleTargetUrl();
        emit targetChanged(); bumpVersion();
    }
}

int AppState::targetPort() const { return m_targetPort; }
void AppState::setTargetPort(int p) {
    if (m_targetPort != p) {
        m_targetPort = p;
        assembleTargetUrl();
        emit targetChanged(); bumpVersion();
    }
}

QString AppState::targetUsername() const { return m_targetUsername; }
void AppState::setTargetUsername(const QString& u) {
    if (m_targetUsername != u) {
        m_targetUsername = u;
        assembleTargetUrl();
        emit targetChanged(); bumpVersion();
    }
}

QString AppState::targetPassword() const { return m_targetPassword; }
void AppState::setTargetPassword(const QString& p) {
    if (m_targetPassword != p) {
        m_targetPassword = p;
        assembleTargetUrl();
        emit targetChanged(); bumpVersion();
    }
}

QString AppState::targetPath() const { return m_targetPath; }
void AppState::setTargetPath(const QString& p) {
    if (m_targetPath != p) {
        m_targetPath = p;
        assembleTargetUrl();
        emit targetChanged(); bumpVersion();
    }
}

int AppState::defaultPortForScheme() const {
    return G5WebsiteUrl::defaultPortForScheme(m_targetScheme);
}

// ── Build m_target from structured fields ──────────────────────────────────
void AppState::assembleTargetUrl() {
    if (m_assembling) return;          // prevent re-entrant loops
    if (m_targetHost.isEmpty()) return; // require at least a hostname

    const QString scheme = m_targetScheme.isEmpty() ? QStringLiteral("https") : m_targetScheme;
    const int defaultPort = G5WebsiteUrl::defaultPortForScheme(scheme);

    // Build authority: [user[:pass]@]host[:port]
    QString authority;
    if (!m_targetUsername.isEmpty()) {
        authority += QString::fromUtf8(QUrl::toPercentEncoding(m_targetUsername));
        if (!m_targetPassword.isEmpty()) {
            authority += QLatin1Char(':') + QString::fromUtf8(QUrl::toPercentEncoding(m_targetPassword));
        }
        authority += QLatin1Char('@');
    }
    authority += m_targetHost;
    if (m_targetPort > 0 && m_targetPort != defaultPort) {
        authority += QLatin1Char(':') + QString::number(m_targetPort);
    }

    const QString url = scheme + QStringLiteral("://") + authority + m_targetPath;

    m_assembling = true;
    setTarget(url);
    m_assembling = false;
}

// ── Parse m_target → populate structured fields ────────────────────────────
void AppState::syncFieldsFromTarget() {
    if (m_assembling) return;

    const QString trimmed = m_target.trimmed();
    if (trimmed.isEmpty()) {
        m_targetScheme.clear();
        m_targetHost.clear();
        m_targetPort = -1;
        m_targetUsername.clear();
        m_targetPassword.clear();
        m_targetPath.clear();
        return;
    }

    if (!trimmed.contains(QStringLiteral("://"))) {
        // Bare hostname/IP — default to https
        m_targetScheme = QStringLiteral("https");
        m_targetHost = trimmed;
        m_targetPort = -1;
        m_targetUsername.clear();
        m_targetPassword.clear();
        m_targetPath.clear();
        return;
    }

    QUrl u(trimmed, QUrl::TolerantMode);
    if (u.isValid() && !u.scheme().isEmpty()) {
        m_targetScheme = u.scheme().toLower();
        m_targetHost = u.host();
        m_targetPort = u.port() > 0 ? u.port() : -1;
        m_targetUsername = u.userName();
        m_targetPassword = u.password();

        // Manual credential fallback: QUrl::TolerantMode often drops userinfo
        // for non-HTTP schemes (mysql://, postgresql://, etc.)
        if (m_targetUsername.isEmpty() && trimmed.contains('@')) {
            QString userinfo = trimmed.section(QStringLiteral("://"), 1)
                                     .section('@', 0, 0);
            if (userinfo.contains(':')) {
                m_targetUsername = userinfo.section(':', 0, 0);
                m_targetPassword = userinfo.section(':', 1);
            } else {
                m_targetUsername = userinfo;
            }
        }
        // Manual port fallback: QUrl::TolerantMode may return -1 for non-standard schemes
        if (m_targetPort <= 0) {
            // Parse port from raw authority: extract host[:port] after optional userinfo@
            QString authority = trimmed.section(QStringLiteral("://"), 1);
            if (authority.contains('@'))
                authority = authority.section('@', 1);
            // Strip path/query/fragment
            for (auto ch : {'/', '?', '#'}) {
                int pos = authority.indexOf(ch);
                if (pos >= 0) authority = authority.left(pos);
            }
            if (authority.contains(':')) {
                QString portPart = authority.section(':', -1);
                bool ok = false;
                int p = portPart.toInt(&ok);
                if (ok && p > 0 && p <= 65535)
                    m_targetPort = p;
            }
        }

        // Keep path + query + fragment
        QString fullPath = u.path();
        if (u.hasQuery()) fullPath += QLatin1Char('?') + u.query();
        if (u.hasFragment()) fullPath += QLatin1Char('#') + u.fragment();
        m_targetPath = fullPath;
    } else {
        // Fallback: keep existing fields, just store host
        m_targetHost = trimmed;
    }
}

// ── QML-invokable: parse pasted URL into fields ────────────────────────────
void AppState::parseUrlIntoFields(const QString& urlString) {
    if (urlString.trimmed().isEmpty()) return;

    const QString trimmed = urlString.trimmed();
    if (!trimmed.contains(QStringLiteral("://"))) {
        // Bare hostname — treat as host, keep current scheme
        m_targetHost = trimmed;
        assembleTargetUrl();
        emit targetChanged();
        return;
    }

    QUrl u(trimmed, QUrl::TolerantMode);
    if (u.isValid() && !u.scheme().isEmpty()) {
        const QString scheme = u.scheme().toLower();
        // Accept any scheme QUrl can parse; fall back to https if not in our list
        m_targetScheme = ::supportedSchemes().contains(scheme) ? scheme : QStringLiteral("https");
        m_targetHost = u.host();
        m_targetPort = u.port() > 0 ? u.port() : -1;
        m_targetUsername = u.userName();
        m_targetPassword = u.password();
        QString fullPath = u.path();
        if (u.hasQuery()) fullPath += QLatin1Char('?') + u.query();
        if (u.hasFragment()) fullPath += QLatin1Char('#') + u.fragment();
        m_targetPath = fullPath;

        // Push canonical string through setTarget so all side effects fire:
        // URL validation, G4/G5 enable+activate, m_targetError clear, bumpVersion
        setTarget(trimmed);
    }
}

static bool isValidHostname(const QString& host) {
    if (host.isEmpty() || host.size() > 253) return false;
    if (host.contains("..") || host == ".") return false;
    if (looksLikeIPv6(host)) return true;  // accept all IPv6
    if (isValidIPv4(host)) return true;
    const auto labels = host.split('.');
    for (const auto& label : labels) {
        if (!isValidHostLabel(label)) return false;
    }
    return true;
}

// ── Full URL validation ──────────────────────────────────────────────────
static QString validateUrl(const QString& trimmed) {
    // 1. Parse scheme
    auto schemeEnd = trimmed.indexOf("://");
    if (schemeEnd < 0) return QString();  // no scheme → not a URL

    QString scheme = trimmed.left(schemeEnd).toLower();
    if (scheme.isEmpty()) return QStringLiteral("Empty URL scheme");
    if (!supportedSchemes().contains(scheme))
        return QStringLiteral("Unsupported protocol: %1:// — supported schemes: %2")
            .arg(scheme, supportedSchemes().join(", "));

    // 2. Parse authority (host[:port])
    QString afterScheme = trimmed.mid(schemeEnd + 3);
    auto pathStart = afterScheme.indexOf('/');
    auto queryStart = afterScheme.indexOf('?');
    auto fragStart = afterScheme.indexOf('#');

    auto authorityEnd = afterScheme.size();
    if (pathStart >= 0) authorityEnd = std::min(authorityEnd, pathStart);
    if (queryStart >= 0) authorityEnd = std::min(authorityEnd, queryStart);
    if (fragStart >= 0) authorityEnd = std::min(authorityEnd, fragStart);

    QString authority = afterScheme.left(authorityEnd);
    if (authority.isEmpty()) return QStringLiteral("URL has no hostname");

    // Strip userinfo (user:password@) before hostname validation
    if (authority.contains('@')) {
        auto atPos = authority.lastIndexOf('@');
        authority = authority.mid(atPos + 1);  // keep only host[:port]
        if (authority.isEmpty()) return QStringLiteral("URL has no hostname after userinfo");
    }

    // 3. Separate host and port
    QString host, portStr;
    auto portColon = authority.lastIndexOf(':');
    // Check for IPv6 bracket notation [::1]:port
    if (authority.startsWith('[')) {
        auto closing = authority.indexOf(']');
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

    // 4. Validate hostname
    if (host.isEmpty()) return QStringLiteral("URL has no hostname");
    if (!isValidHostname(host)) {
        if (host.contains("..")) return QStringLiteral("Invalid hostname: consecutive dots");
        return QStringLiteral("Hostname label must be 1-63 alphanumeric chars (a-z, 0-9, -) and cannot start/end with hyphen");
    }

    // 5. Validate port
    if (!portStr.isEmpty()) {
        bool ok = false;
        int port = portStr.toInt(&ok);
        if (!ok) return QStringLiteral("Port must be a number");
        if (port < 1 || port > 65535)
            return QStringLiteral("Port must be between 1 and 65535 (got %1)").arg(port);
    }

    return QString();  // empty = success
}

// ── Target ─────────────────────────────────────────────────────────────────
void AppState::setTarget(const QString& t) {
    if (m_target != t) {
        m_target = t;
        m_targetError.clear();
        TRACE(" setTarget('%s')\n", m_target.toUtf8().constData());

        // Sync structured fields from canonical target (unless we're assembling)
        syncFieldsFromTarget();

        const QString trimmed = m_target.trimmed();
        bool has = !trimmed.isEmpty();

        // ── Unified validation ──────────────────────────────────────────
        if (has) {
            if (trimmed.contains("://")) {
                // URL path: validate scheme, hostname, optional port
                m_targetError = validateUrl(trimmed);
            } else {
                // Hostname/IP path
                if (!isValidHostname(trimmed)) {
                    if (trimmed.contains("..")) m_targetError = QStringLiteral("Invalid hostname: consecutive dots");
                    else m_targetError = QStringLiteral("Hostname label must be 1-63 alphanumeric chars (a-z, 0-9, -) and cannot start/end with hyphen");
                }
            }
        }

        bool isUrlType = has && trimmed.contains("://");  // any :// → URL type
        bool isHttp    = isUrlType && isTargetHttpUrl();  // only http/https → HTTP-specific routing
        bool isAnyUrl  = isUrlType && isTargetUrl();      // any supported URL scheme

        // Deep diagnostic: why isTargetUrl() might return false
        QString scheme = trimmed.contains("://") ? trimmed.section("://", 0, 0).toLower() : QString();
        TRACE(" setTarget scheme='%s' empty=%d hasScheme=%d isTargetUrl=%d isTargetHttpUrl=%d validateErr='%s'\n",
                scheme.toUtf8().constData(), isTargetEmpty(), trimmed.contains("://"),
                isTargetUrl(), isTargetHttpUrl(), m_targetError.toUtf8().constData());

        // G4: always on when target non-empty (URL or host)
        // G5: any valid URL scheme (all protocol tests: HTTP, FTP, SSH, DB, etc.)
        setGroupEnabled(3, has);               // G4 on if target non-empty
        setGroupEnabled(4, has && isAnyUrl);   // G5 on for any valid URL scheme

        // Activate G4/G5 when target is entered, deactivate when cleared
        bool had3 = m_activeGroups.contains(3);
        bool had4 = m_activeGroups.contains(4);
        if (has) {
            m_activeGroups.insert(3);  // G4 active
            if (isAnyUrl)
                m_activeGroups.insert(4);  // G5 active
        } else {
            m_activeGroups.remove(3);
            m_activeGroups.remove(4);
        }
        if (had3 != m_activeGroups.contains(3) || had4 != m_activeGroups.contains(4))
            emit groupActiveChanged();

        TRACE(" setTarget result: has=%d isUrl=%d isHttp=%d G4=%d G5=%d err='%s'\n",
                has, isAnyUrl, isHttp, has, has && isAnyUrl, m_targetError.toUtf8().constData());
        emit targetChanged();
        bumpVersion();
    }
}

// ── Group labels ───────────────────────────────────────────────────────────
QStringList AppState::groupLabels() const { return DiagnosticConfig::groupLabels(); }

// ── Test enable/disable — delegated to DiagnosticConfig ──────────────
bool AppState::isDiagEnabled(int diagIdInt) const { return m_config.isDiagEnabled(diagIdInt); }
void AppState::setDiagEnabled(int diagIdInt, bool enabled) { m_config.setDiagEnabled(diagIdInt, enabled); saveSettings(); bumpVersion(); }
void AppState::setGroupEnabled(int groupInt, bool enabled) { m_config.setGroupEnabled(groupInt, enabled); saveSettings(); bumpVersion(); }
bool AppState::isGroupAllEnabled(int groupInt) const { return m_config.isGroupAllEnabled(groupInt); }
bool AppState::isGroupAnyEnabled(int groupInt) const { return m_config.isGroupAnyEnabled(groupInt); }

// ── Group activation (separate from enable/disable) ─────────────────
void AppState::setGroupActive(int groupInt, bool active) {
    if (groupInt < 0 || groupInt >= 5) return;
    bool changed = active ? m_activeGroups.contains(groupInt) == false
                          : m_activeGroups.contains(groupInt) == true;
    if (active)
        m_activeGroups.insert(groupInt);
    else
        m_activeGroups.remove(groupInt);
    if (changed) {
        saveSettings();
        emit groupActiveChanged();
        bumpVersion();
    }
}

bool AppState::isGroupActive(int groupInt) const {
    return m_activeGroups.contains(groupInt);
}

// ── G5 scheme-to-DiagId matching — single source of truth ─────────────────
// Returns true if `id` is applicable for the given URL scheme.
// Used by both runDiagnostics (scheduling) and allDiagIdsForGroup (Config display)
// so the two views can never diverge.
static bool g5DiagMatchesScheme(DiagId id, const QString& schemeLower) {
    bool isGeneric = (id == DiagId::G5UrlParsing || id == DiagId::G5TcpConnect
                   || id == DiagId::G5ServiceBanner);
    if (isGeneric) return true;

    bool isHttp = (schemeLower == "http" || schemeLower == "https");
    bool isFtp  = (schemeLower == "ftp" || schemeLower == "ftps");
    bool isSsh  = (schemeLower == "ssh" || schemeLower == "sftp");

    if (isHttp) {
        return (id == DiagId::G5CurlVerbose || id == DiagId::G5HttpHeaders
             || id == DiagId::G5SecurityHeaders || id == DiagId::G5SslCertificate
             || id == DiagId::G5HttpRedirect || id == DiagId::G5HttpCompression
             || id == DiagId::G5HttpTiming);
    }
    if (isFtp)  return id == DiagId::G5FtpDiagnostics;
    if (isSsh)  return id == DiagId::G5SshDiagnostics;

    if (schemeLower == "mysql")      return id == DiagId::G5Mysql;
    if (schemeLower == "postgresql") return id == DiagId::G5Postgres;
    if (schemeLower == "redis")      return id == DiagId::G5Redis;
    if (schemeLower == "mongodb")    return id == DiagId::G5Mongodb;
    if (schemeLower == "ldap")       return id == DiagId::G5Ldap;
    if (schemeLower == "mqtt")       return id == DiagId::G5Mqtt;
    if (schemeLower == "telnet")     return id == DiagId::G5Telnet;
    if (schemeLower == "smtp" || schemeLower == "smtps"
     || schemeLower == "imap" || schemeLower == "imaps"
     || schemeLower == "pop3" || schemeLower == "pop3s")
        return id == DiagId::G5EmailDiagnostics;

    return false;
}

// ── Run diagnostics ────────────────────────────────────────────────────────
void AppState::runDiagnostics() {
    // Force-reset if stuck from a previous run
    if (m_runStatus == RunStatus::Running) {
        Logger::instance().event("Force-resetting stuck run");
        cancel();
        m_runStatus = RunStatus::Idle;
        m_runGeneration.fetch_add(1, std::memory_order_release);
    }
    TRACE(" runDiagnostics start target='%s'\n", m_target.toUtf8().constData());

    // Reset state before each run (clears previous results, error messages, etc.)
    reset();

    // Flutter behaviour: G1-G3 always run (local-only); G4 requires target; G5 requires URL.
    // The group-level filter below (hasTarget/isUrl) handles G4/G5 exclusion automatically.
    // No blanket error on empty target — only block if NO groups would run at all.

    bool hasTarget = !isTargetEmpty();
    m_runStatus = RunStatus::Running;
    m_runGeneration.fetch_add(1, std::memory_order_release); // invalidate stale callbacks
    TRACE(" status=Running generation=%d, building pending tests\n", (int)m_runGeneration.load());
    m_totalCompleted = 0;
    m_totalDiags = 0;
    m_results.clear();
    m_completedPerGroup.clear();
    m_totalPerGroup.clear();
    m_currentDiagName.clear();
    m_currentGroup.clear();

    // Build groups: group tests by DiagGroup (G1→G5 order)
    m_pendingGroups.clear();
    TRACE(" runDiagnostics: enabledTests=%d hasTarget=%d\n",
            (int)m_config.enabledDiags().size(), hasTarget);
    // Per-group enabled counts (verify checkbox state)
    for (int g = 0; g < 5; ++g) {
        int enabledInGroup = 0;
        int totalInGroup = 0;
        auto group = static_cast<DiagGroup>(g);
        for (auto id : DiagnosticConfig::diagIdsForGroup(group)) {
            totalInGroup++;
            if (m_config.enabledDiags().contains(id)) enabledInGroup++;
        }
        TRACE("   G%d: %d/%d enabled\n", g+1, enabledInGroup, totalInGroup);
    }
    for (int g = 0; g < 5; ++g) {
        // Skip inactive groups (user toggled them off via G1-G5 buttons)
        if (!m_activeGroups.contains(g)) continue;
        GroupTask gt;
        gt.group = static_cast<DiagGroup>(g);
        for (auto id : DiagnosticConfig::diagIdsForGroup(gt.group)) {
            if (!m_config.enabledDiags().contains(id)) continue;
            if (gt.group == DiagGroup::G4 && !hasTarget) continue;
            if (gt.group == DiagGroup::G5 && !hasTarget) continue;
            // G5: filter by scheme — only schedule tests matching the target's protocol
            if (gt.group == DiagGroup::G5 && hasTarget) {
                QString scheme = m_targetScheme.isEmpty()
                    ? QStringLiteral("https") : m_targetScheme.toLower();
                if (!g5DiagMatchesScheme(id, scheme)) continue;
            }
            gt.diagIds.append(id);
            m_totalPerGroup[gt.group]++;
        }
        if (!gt.diagIds.isEmpty()) {
            // Limit per group via env var
            QByteArray maxEnv = qgetenv("ND_MAX_TESTS");
            if (!maxEnv.isEmpty()) {
                int max = maxEnv.toInt();
                if (max > 0 && gt.diagIds.size() > max) gt.diagIds = gt.diagIds.mid(0, max);
            }
            m_pendingGroups.append(gt);
            m_totalDiags += gt.diagIds.size();
        }
    }
    TRACE(" %d groups, %d total tests\n", (int)m_pendingGroups.size(), m_totalDiags);

    if (m_pendingGroups.isEmpty()) {
        m_errorMessage = hasTarget
            ? QStringLiteral("No diagnostic tests are enabled. Check Config.")
            : QStringLiteral("No target specified and no local tests enabled. Enter a target or enable tests in Config.");
        m_runStatus = RunStatus::Error;
        TRACE(" runDiagnostics blocked: no enabled tests\n");
        emit runStatusChanged();
        return;
    }

    emit runStatusChanged();
    emit progressChanged();
    emit resultsReset();
    bumpVersion();

    Logger::instance().event(QStringLiteral("Starting diagnostic run: %1 tests in %2 groups")
                              .arg(m_totalDiags).arg(m_pendingGroups.size()));

    startNextGroup();
}

void AppState::startNextGroup() {
    if (m_runStatus != RunStatus::Running) return;
    if (m_currentGroupIdx >= m_pendingGroups.size()) {
        TRACE(" All groups complete. Setting runStatus=Completed.\n");
        m_runStatus = RunStatus::Completed;
        m_currentDiagName.clear();
        m_currentGroup.clear();
        emit runStatusChanged();
        emit progressChanged();
        bumpVersion();
        Logger::instance().event(QStringLiteral("Diagnostic run complete"));
        return;
    }

    auto& gt = m_pendingGroups[m_currentGroupIdx];
    m_currentGroup = diagGroupLabel(gt.group);
    m_activeGroupDone.store(0);
    bumpVersion();
    TRACE(" startGroup %s (%d tests)\n", m_currentGroup.toUtf8().constData(), (int)gt.diagIds.size());

    for (int i = 0; i < gt.diagIds.size(); ++i) {
        runDiagInGroup(m_currentGroupIdx, i);
    }
}

void AppState::runDiagInGroup(int groupIdx, int diagIdx) {
    if (m_runStatus != RunStatus::Running) return;
    if (groupIdx >= m_pendingGroups.size()) return;
    auto& gt = m_pendingGroups[groupIdx];
    if (diagIdx >= gt.diagIds.size()) return;

    DiagId id = gt.diagIds[diagIdx];
    m_currentDiagName = staticDiagDisplayName(id);
    emit currentDiagChanged();
    emit groupChanged();
    bumpVersion();

    TRACE(" runDiag id=%d name='%s' group=%d\n", (int)id, m_currentDiagName.toUtf8().constData(), groupIdx);

    // Create task via factory — each task handles its own timeout internally
    int runGen = m_runGeneration.load(std::memory_order_acquire);
    auto task = TaskFactory::createTask(id, m_target);
    if (!task) {
        onDiagFinished(id, DiagnosticResult::error(id, QStringLiteral("Unknown DiagId")));
        return;
    }

    // When task completes (or times out), route to onDiagFinished
    connect(task.get(), &DiagnosticTask::finished, this,
        [this, id, groupIdx, runGen](const DiagnosticResult& result) {
            if (m_runGeneration.load(std::memory_order_acquire) != runGen) return;
            if (m_results.contains(id)) return;
            onDiagFinished(id, result);
            int done = m_activeGroupDone.fetch_add(1) + 1;
            auto& gt = m_pendingGroups[groupIdx];
            if (done >= gt.diagIds.size()) {
                m_currentGroupIdx++;
                QTimer::singleShot(0, this, &AppState::startNextGroup);
            }
        });

    // The task owns its own lifetime: it self-deletes (via deleteLater) only
    // after its worker run() has returned — see DiagnosticTask::onFutureFinished.
    // Deleting it here on the first finished() signal would be unsafe, because a
    // watchdog timeout emits finished() while run() may still be executing.
    task.release()->start(); // transfer ownership to Qt parent/event loop
}

void AppState::onDiagFinished(DiagId id, DiagnosticResult result) {
    TRACE(" onDiagFinished id=%d status=%d\n", (int)id, (int)result.status);
    // Suppress stale results after cancel/reset
    if (m_runStatus != RunStatus::Running) return;
    if (m_results.contains(id)) return; // already handled by timeout/cancel
    DiagGroup g = DiagnosticConfig::diagGroup(id);
    m_results[id] = result;
    m_totalCompleted++;
    m_completedPerGroup[g]++;
    m_resultsVersion++;

    emit progressChanged();
    emit diagCompleted(static_cast<int>(id));
    bumpVersion();
}

void AppState::cancel() {
    if (m_runStatus != RunStatus::Running) return;
    m_runStatus = RunStatus::Cancelled;
    m_currentDiagName.clear();

    // DiagnosticTask::cancel() stops the per-task watchdog and suppresses
    // the finished() signal via the atomic cancelled flag. Tasks in-flight
    // will complete silently in the background. No explicit result marking
    // is needed — the UI shows pending tests as cancelled via runStatus.
    emit runStatusChanged();
    emit progressChanged();
    bumpVersion();
    Logger::instance().event("Diagnostic run cancelled");
}

void AppState::reset() {
    cancel();
    m_runStatus = RunStatus::Idle;
    m_totalCompleted = 0; m_totalDiags = 0;
    m_results.clear();
    m_completedPerGroup.clear(); m_totalPerGroup.clear();
    m_errorMessage.clear();
    m_pendingGroups.clear();
    m_currentGroupIdx = 0;
    m_activeGroupDone.store(0);
    m_resultsVersion = 0;
    emit runStatusChanged();
    emit progressChanged();
    emit resultsReset();
    bumpVersion();
}

// ── Results for QML ────────────────────────────────────────────────────────
QVariantList AppState::resultsForGroup(int groupInt) const {
    QVariantList list;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : DiagnosticConfig::diagIdsForGroup(g)) {
        if (m_results.contains(id)) {
            const auto& r = m_results[id];
            // Skip platform-unavailable / irrelevant protocol tests
            if (r.status == DiagStatus::Skipped)
                continue;
            QVariantMap m;
            m["id"] = static_cast<int>(r.id);
            m["diagId"] = static_cast<int>(r.id);
            m["displayName"] = r.displayName.isEmpty() ? staticDiagDisplayName(r.id) : r.displayName;
            m["status"] = static_cast<int>(r.status);
            m["statusIcon"] = r.statusIcon();
            m["summary"] = r.summary;
            m["details"] = r.details;
            m["durationMs"] = r.durationMs;
            m["isDone"] = true;
            m["isPending"] = false;
            m["isRunning"] = false;
            list.append(m);
        }
    }
    return list;
}

QVariantList AppState::allDiagIdsForGroup(int groupInt) const {
    QVariantList list;
    if (!DiagnosticConfig::isValidGroup(groupInt)) return list;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : DiagnosticConfig::diagIdsForGroup(g)) {
        // G5: filter tests by target scheme so Config shows only relevant protocol tests
        if (g == DiagGroup::G5 && !isTargetEmpty() && hasUrlScheme()) {
            QString scheme = m_targetScheme.isEmpty()
                ? QStringLiteral("https") : m_targetScheme.toLower();
            if (!g5DiagMatchesScheme(id, scheme)) continue;
        }
        list.append(static_cast<int>(id));
    }
    return list;
}

QVariantList AppState::visibleGroups() const {
    QVariantList list;
    for (int i = 0; i < 5; ++i) {
        QVariantMap s = groupStats(i);
        if (s["enabled"].toInt() > 0 || s["total"].toInt() > 0)
            list.append(i);
    }
    return list;
}

QVariantList AppState::allDiagsForGroup(int groupInt) const {
    QVariantList list;
    if (!DiagnosticConfig::isValidGroup(groupInt)) return list;
    auto g = static_cast<DiagGroup>(groupInt);
    
    for (auto id : DiagnosticConfig::diagIdsForGroup(g)) {
        if (!m_config.enabledDiags().contains(id)) continue;
        
        // G5: skip cancelled/irrelevant protocol tests (e.g. FTP in an HTTP run)
        // Also skip platform-unavailable tests for G1-G4 (e.g. iOS-sandboxed tests)
        if (m_results.contains(id)
            && m_results[id].status == DiagStatus::Skipped)
            continue;

        if (m_results.contains(id)) {
            // Completed test
            const auto& r = m_results[id];
            QVariantMap m;
            m["id"] = static_cast<int>(r.id);
            m["diagId"] = static_cast<int>(r.id);  // alias for QML access
            m["displayName"] = r.displayName.isEmpty() ? staticDiagDisplayName(r.id) : r.displayName;
            m["status"] = static_cast<int>(r.status);
            m["statusIcon"] = r.statusIcon();
            m["summary"] = r.summary;
            m["details"] = r.details;
            m["durationMs"] = r.durationMs;
            QVariantList props;
            for (auto& p : r.properties) {
                QVariantMap pm;
                pm["label"] = p.label;
                pm["value"] = p.value;
                props.append(pm);
            }
            m["properties"] = props;
            m["isDone"] = true;
            m["isPending"] = false;
            m["isRunning"] = false;
            list.append(m);
        } else {
            // G5: hide pending tests that don't match the current URL scheme.
            // These would never be scheduled by runDiagnostics, so showing
            // them as "pending" is misleading — they'll never run.
            if (g == DiagGroup::G5 && !isTargetEmpty() && hasUrlScheme()) {
                QString scheme = m_targetScheme.isEmpty()
                    ? QStringLiteral("https") : m_targetScheme.toLower();
                if (!g5DiagMatchesScheme(id, scheme)) continue;
            }
            // Pending test — mark as running if its group is currently executing
            bool isRunning = (m_runStatus == RunStatus::Running)
                          && (m_currentGroupIdx < m_pendingGroups.size())
                          && (m_pendingGroups[m_currentGroupIdx].group == g);
            QVariantMap m;
            m["id"] = static_cast<int>(id);
            m["diagId"] = static_cast<int>(id);
            m["displayName"] = staticDiagDisplayName(id);
            m["status"] = -1;
            m["statusIcon"] = QStringLiteral("badge-skip");
            m["summary"] = QString(isRunning ? "Running..." : "");
            m["details"] = QString();
            m["durationMs"] = 0;
            m["isDone"] = false;
            m["isPending"] = true;
            m["isRunning"] = isRunning;
            list.append(m);
        }
    }
    return list;
}

QVariantMap AppState::groupStats(int groupInt) const {
    QVariantMap stats;
    // ── Aggregate all groups when groupInt == -1 ──────────────────────
    if (groupInt < 0) {
        int total = 0, pass = 0, warn = 0, fail = 0, skip = 0, info = 0, error = 0, completed = 0;
        for (int g = 0; g < 5; ++g) {
            QVariantMap gs = groupStats(g);
            total     += gs["total"].toInt();
            pass      += gs["pass"].toInt();
            warn      += gs["warn"].toInt();
            fail      += gs["fail"].toInt();
            skip      += gs["skip"].toInt();
            info      += gs["info"].toInt();
            error     += gs["error"].toInt();
            completed += gs["completed"].toInt();
        }
        stats["pass"] = pass; stats["warn"] = warn;
        stats["fail"] = fail; stats["skip"] = skip;
        stats["info"] = info; stats["error"] = error;
        stats["completed"] = completed; stats["total"] = total;
        stats["enabled"] = total;
        return stats;
    }
    auto g = static_cast<DiagGroup>(groupInt);
    // total = tests actually scheduled for this group (m_totalPerGroup).
    // Before any run this is 0 — all counts show 0.
    int total = m_totalPerGroup.value(g, 0);
    int pass = 0, warn = 0, fail = 0, skip = 0, info = 0, error = 0, completed = 0;
    for (auto id : DiagnosticConfig::diagIdsForGroup(g)) {
        if (!m_results.contains(id)) continue;
        completed++;
        switch (m_results[id].status) {
            case DiagStatus::Pass:    pass++; break;
            case DiagStatus::Warning: warn++; break;
            case DiagStatus::Fail:    fail++; break;
            case DiagStatus::Skipped: skip++; break;
            case DiagStatus::Info:    info++; break;
            case DiagStatus::Error:   error++; break;
            default: break;
        }
    }
    stats["pass"] = pass; stats["warn"] = warn;
    stats["fail"] = fail; stats["skip"] = skip; stats["info"] = info; stats["error"] = error;
    stats["completed"] = completed; stats["total"] = total;
    stats["enabled"] = total;
    return stats;
}

QString AppState::currentDiagLabel() const {
    if (m_runStatus == RunStatus::Running)
        return QStringLiteral("%1: %2").arg(m_currentGroup, m_currentDiagName);
    return {};
}

QString AppState::diagDisplayName(int diagIdInt) const {
    return staticDiagDisplayName(static_cast<DiagId>(diagIdInt));
}

QString AppState::staticDiagDisplayName(DiagId id) {
    switch (id) {
        case DiagId::G1NetworkAdapters: return QStringLiteral("Network Adapters");
        case DiagId::G1NicAdvanced: return QStringLiteral("NIC Advanced");
        case DiagId::G1WifiDiagnostics: return QStringLiteral("WiFi Information");
        case DiagId::G1WiredDiagnostics: return QStringLiteral("Wired Information");
        case DiagId::G1DhcpStatus: return QStringLiteral("DHCP Status");
        case DiagId::G1IpConfiguration: return QStringLiteral("IP Configuration");
        case DiagId::G1ActiveConnections: return QStringLiteral("Active Connections");
        case DiagId::G1CellularInfo: return QStringLiteral("Cellular Information");
        case DiagId::G2NetworkProfile: return QStringLiteral("Network Profile");
        case DiagId::G2TcpSettings: return QStringLiteral("TCP Settings");
        case DiagId::G2DefaultGateway: return QStringLiteral("Default Gateway");
        case DiagId::G2RoutingTable: return QStringLiteral("Routing Table");
        case DiagId::G2ArpTable: return QStringLiteral("ARP Table");
        case DiagId::G2ProxySettings: return QStringLiteral("Proxy Settings");
        case DiagId::G3NetskopeStatus: return QStringLiteral("Netskope Status");
        case DiagId::G3DnsServers: return QStringLiteral("DNS Servers");
        case DiagId::G3DnsCache: return QStringLiteral("DNS Cache");
        case DiagId::G3DnsPollution: return QStringLiteral("DNS Pollution");
        case DiagId::G3InternetSpeedTest: return QStringLiteral("Internet Connectivity");
        case DiagId::G4DnsResolution: return QStringLiteral("DNS Resolution");
        case DiagId::G4Ping: return QStringLiteral("Ping");
        case DiagId::G4Traceroute: return QStringLiteral("Traceroute");
        case DiagId::G4PathPing: return QStringLiteral("PathPing");
        case DiagId::G4MtuDiscovery: return QStringLiteral("MTU Discovery");
        case DiagId::G5UrlParsing: return QStringLiteral("URL Parsing");
        case DiagId::G5TcpConnect: return QStringLiteral("TCP Connect");
        case DiagId::G5ServiceBanner: return QStringLiteral("Service Banner");
        case DiagId::G5CurlVerbose: return QStringLiteral("HTTP Request");
        case DiagId::G5HttpHeaders: return QStringLiteral("HTTP Headers");
        case DiagId::G5SecurityHeaders: return QStringLiteral("Security Headers");
        case DiagId::G5SslCertificate: return QStringLiteral("SSL Certificate");
        case DiagId::G5HttpRedirect: return QStringLiteral("HTTP Redirect");
        case DiagId::G5HttpCompression: return QStringLiteral("HTTP Compression");
        case DiagId::G5HttpTiming: return QStringLiteral("HTTP Timing");
        case DiagId::G5FtpDiagnostics: return QStringLiteral("FTP");
        case DiagId::G5SshDiagnostics: return QStringLiteral("SSH");
        case DiagId::G5EmailDiagnostics: return QStringLiteral("Email");
        case DiagId::G5Telnet:   return QStringLiteral("Telnet");
        case DiagId::G5Mysql:    return QStringLiteral("MySQL");
        case DiagId::G5Postgres: return QStringLiteral("PostgreSQL");
        case DiagId::G5Redis:    return QStringLiteral("Redis");
        case DiagId::G5Mongodb:  return QStringLiteral("MongoDB");
        case DiagId::G5Ldap:     return QStringLiteral("LDAP");
        case DiagId::G5Mqtt:     return QStringLiteral("MQTT");
    }
    return QStringLiteral("Unknown");
}

QVariantList AppState::allGroupStats() const {
    if (m_cachedStatsVersion == m_resultsVersion && !m_cachedGroupStats.isEmpty())
        return m_cachedGroupStats;
    m_cachedStatsVersion = m_resultsVersion;
    m_cachedGroupStats.clear();
    for (int g = 0; g < 5; ++g)
        m_cachedGroupStats.append(groupStats(g));
    return m_cachedGroupStats;
}


ReportData AppState::buildReportData() const {
    ReportData d;
    d.target = m_target.isEmpty() ? QStringLiteral("(none)") : m_target.toHtmlEscaped();
    d.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    d.appVersion = appVersion();
    d.buildNumber = buildNumber();
    d.groupLabels = groupLabels();
    d.results = m_results;
    d.diagDisplayName = &AppState::staticDiagDisplayName;
    for (int g = 0; g < 5; ++g) {
        d.groupStats[g] = groupStats(g);
        d.diagIdsInGroup[static_cast<DiagGroup>(g)] =
            DiagnosticConfig::diagIdsForGroup(static_cast<DiagGroup>(g));
    }
    return d;
}

QString AppState::buildReportHtml(bool fullDetail, bool darkBackground) const {
    return ReportEngine::buildHtml(buildReportData(), fullDetail, darkBackground);
}

// Full standalone HTML document with a modern dark theme (styled after the
// PowerShell NetDiagnostic report). Rendered by a real browser / mail client —
// NOT by the in-app QML preview, which uses the Qt-subset buildReportHtml().
QString AppState::buildRichHtmlDocument() const {
    return ReportEngine::buildRichDocument(buildReportData());
}
QString AppState::defaultReportPath(const QString& ext) const {
    return ReportEngine::defaultReportPath(ext);
}
QString AppState::exportHtml(const QString& filePath) const {
    return ReportEngine::exportHtml(filePath, buildRichHtmlDocument());
}

QString AppState::exportPdf(const QString& filePath) const {
    return ReportEngine::exportPdf(filePath, buildReportHtml(false));
}

void AppState::requestSavePath(const QString& format) {
    m_reportEngine.requestSavePath(format);
}

void AppState::setPremium(bool v) {
    m_premium.setPremium(v);
}

void AppState::requestSubscription() {
    m_premium.requestSubscription();
}

void AppState::restorePurchases() {
    m_premium.restorePurchases();
}

void AppState::shareReport(const QString& format) {
    if (!m_premium.isPremium()) { emit premiumRequired(); return; }
    const QString ext = (format == QLatin1String("pdf")) ? QStringLiteral("pdf")
                                                         : QStringLiteral("html");
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
    // Generate into a temp file, then present the OS share sheet.
    const QString tmp = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("NetDiagnostics_report.%1").arg(ext));
    const QString saved = (ext == QLatin1String("pdf")) ? exportPdf(tmp) : exportHtml(tmp);
    if (saved.isEmpty()) { emit reportShared(false); return; }
    platformShareFile(saved,
                      ext == QLatin1String("pdf") ? QStringLiteral("application/pdf")
                                                  : QStringLiteral("text/html"),
                      QStringLiteral("Network Diagnostic Report"));
    emit reportShared(true);
#else
    // Desktop: save to Documents and hand off to the default mail client.
    const QString saved = (ext == QLatin1String("pdf")) ? exportPdf(defaultReportPath(ext))
                                                        : exportHtml(defaultReportPath(ext));
    if (saved.isEmpty()) { emit reportShared(false); return; }
    emailReportDesktop(saved);
    emit reportShared(true);
#endif
}

void AppState::emailReportDesktop(const QString& path) {
    ReportEngine::emailReportDesktop(path);
}

void AppState::showDetailDialog(int diagIdInt) {
    if (!DiagnosticConfig::isValidDiagId(diagIdInt)) return;
    auto id = static_cast<DiagId>(diagIdInt);
    if (!m_results.contains(id)) return;
    
    const auto& r = m_results[id];
    
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
    // Use heap-allocated dialog with show() instead of exec()
    // exec() creates a nested event loop that crashes QML on ARM64
    auto* dlg = new QDialog(nullptr, Qt::Dialog | Qt::WindowCloseButtonHint);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(r.displayName);
    dlg->setMinimumSize(450, 350);
    dlg->setModal(true);
    dlg->setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1E1E2E; }"
        "QLabel { color: #E0E0E0; font-family: 'JetBrains Mono'; }"
        "QPushButton { color: #E0E0E0; background-color: #252538; border: 1px solid #3A3A5A; padding: 6px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #0078D4; }"
    ));
    
    auto* layout = new QVBoxLayout(dlg);
    layout->setSpacing(8);
    
    // Status line
    QStringList statusNames = {"Pass", "Warning", "Fail", "Skipped", "Error", "Info"};
    auto* statusLabel = new QLabel(QStringLiteral("Status: %1    Duration: %2ms")
        .arg(statusNames.value(static_cast<int>(r.status), "Unknown"))
        .arg(r.durationMs));
    statusLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #A0A0B8; font-family: 'JetBrains Mono';");
    layout->addWidget(statusLabel);
    
    // Summary
    if (!r.summary.isEmpty()) {
        auto* sumLabel = new QLabel(QStringLiteral("Summary: %1").arg(r.summary));
        sumLabel->setStyleSheet("font-size: 12px; color: #E0E0E0; font-family: 'JetBrains Mono';");
        sumLabel->setWordWrap(true);
        layout->addWidget(sumLabel);
    }
    
    // Properties
    if (!r.properties.isEmpty()) {
        auto* propHeader = new QLabel("Properties:");
        propHeader->setStyleSheet("font-size: 11px; font-weight: bold; color: #A0A0B8; margin-top: 8px; font-family: 'JetBrains Mono';");
        layout->addWidget(propHeader);
        
        for (const auto& p : r.properties) {
            auto* propLabel = new QLabel(QStringLiteral("  %1: %2").arg(p.label, p.value));
            propLabel->setStyleSheet("font-size: 11px; color: #E0E0E0; font-family: 'JetBrains Mono';");
            propLabel->setWordWrap(true);
            layout->addWidget(propLabel);
        }
    }
    
    // Raw output (scrollable)
    if (!r.details.isEmpty()) {
        auto* outHeader = new QLabel("Output:");
        outHeader->setStyleSheet("font-size: 11px; font-weight: bold; color: #A0A0B8; margin-top: 8px; font-family: 'JetBrains Mono';");
        layout->addWidget(outHeader);
        
        auto* scrollArea = new QScrollArea();
        scrollArea->setStyleSheet("QScrollArea { background-color: #252538; border: none; border-radius: 4px; }");
        scrollArea->setMaximumHeight(200);
        
        auto* detailText = new QLabel(r.details);
        detailText->setStyleSheet("font-size: 10px; color: #A0A0B8; padding: 8px; font-family: 'JetBrains Mono';");
        detailText->setWordWrap(true);
        scrollArea->setWidget(detailText);
        layout->addWidget(scrollArea);
    }
    
    // Close button
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    layout->addWidget(btnBox);
    
    dlg->show();
#endif // !PLATFORM_IOS && !PLATFORM_ANDROID
}

QVariantMap AppState::getDetailResult(int diagIdInt) const {
    QVariantMap m;
    if (!DiagnosticConfig::isValidDiagId(diagIdInt)) return m;
    auto id = static_cast<DiagId>(diagIdInt);
    if (!m_results.contains(id)) return m;
    
    const auto& r = m_results[id];
    m["displayName"] = r.displayName;
    m["status"] = static_cast<int>(r.status);
    m["summary"] = r.summary;
    m["details"] = r.details;
    m["durationMs"] = r.durationMs;
    
    QVariantList props;
    for (const auto& p : r.properties) {
        QVariantMap pm;
        pm["label"] = p.label;
        pm["value"] = p.value;
        props.append(pm);
    }
    m["properties"] = props;
    return m;
}

// =============================================================================
// QSettings persistence — survives app restarts
// =============================================================================

static const char* kSettingsGroup = "settings";

void AppState::loadSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));

    // Language index
    int lang = s.value("language", 0).toInt();
    if (lang >= 0 && lang <= 8) {
        m_languageIndex = lang;
        emit languageChanged();
    }

    // Theme mode (0=system, 1=light, 2=dark)
    int theme = s.value("themeMode", 2).toInt();
    if (theme >= 0 && theme <= 2 && theme != m_themeMode) {
        m_themeMode = theme;
        emit themeChanged();
    }

    // Active groups (G1-G5 shown/hidden)
    QVariantList ag = s.value("activeGroups").toList();
    if (!ag.isEmpty()) {
        m_activeGroups.clear();
        for (const auto& v : ag)
            m_activeGroups.insert(v.toInt());
    }

    // Per-test enabled/disabled (Config page checkboxes)
    QStringList enabledStrs = s.value("enabledDiags").toStringList();
    if (!enabledStrs.isEmpty()) {
        // Reset all to disabled, then re-enable saved ones
        int diagCount = DiagnosticConfig::allDiagIds().size();
        for (int i = 0; i < diagCount; ++i)
            m_config.setDiagEnabled(i, false);
        for (const auto& str : enabledStrs) {
            bool ok = false;
            int id = str.toInt(&ok);
            if (ok)
                m_config.setDiagEnabled(id, true);
        }
    }

    s.endGroup();
    s.sync();  // flush to disk immediately (critical on mobile)
}

void AppState::saveSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));

    s.setValue("language", m_languageIndex);
    s.setValue("themeMode", m_themeMode);

    QVariantList ag;
    for (int g : m_activeGroups) ag.append(g);
    s.setValue("activeGroups", ag);

    QStringList enabledStrs;
    for (auto id : m_config.enabledDiags())
        enabledStrs.append(QString::number(static_cast<int>(id)));
    s.setValue("enabledDiags", enabledStrs);

    s.endGroup();
    s.sync();  // flush to disk immediately (critical on mobile)
}