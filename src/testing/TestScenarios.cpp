// =============================================================================
// TestScenarios.cpp — Region-aware test scenarios with geo-detection
// =============================================================================
// Target selection principles:
// 1. Region-aware: auto-detect China vs Global, use appropriate targets
//    China → Alibaba (aliyun), Tencent (qq), DNSPod, 114DNS
//    Global → Google, Cloudflare, GitHub, httpbin.org
// 2. Auth-aware: only test with user/pass when the protocol genuinely needs it
// 3. CI-safe: all core targets reachable from GitHub Actions runners
// 4. Public services: nobody's private servers; no credentials exposed
// =============================================================================
#ifdef ND_TESTING

#include "testing/TestScenarios.h"
#include "testing/TestHarness.h"

#include <QProcessEnvironment>
#include <QTcpSocket>
#include <QHostInfo>
#include <QThread>
#include <QEventLoop>
#include <QTimer>

namespace TestScenarios {

// ── Helpers ────────────────────────────────────────────────────────────────
static TestCase make(const QString& name, const QString& scheme, const QString& host, int port = -1) {
    TestCase tc;
    tc.name   = name;
    tc.scheme = scheme;
    tc.host   = host;
    tc.port   = port;
    return tc;
}

static TestCase makeWithAuth(const QString& name, const QString& scheme, const QString& host,
                              int port, const QString& user, const QString& pass) {
    TestCase tc = make(name, scheme, host, port);
    tc.username = user;
    tc.password = pass;
    return tc;
}

static bool isCI() {
    return QProcessEnvironment::systemEnvironment().contains("CI")
        || QProcessEnvironment::systemEnvironment().contains("GITHUB_ACTIONS");
}

// ═════════════════════════════════════════════════════════════════════════════
// Geo-detection: fast TCP connect to China-specific host
// Returns true if we appear to be inside China (aliyun.com reachable quickly)
// ═════════════════════════════════════════════════════════════════════════════
static bool s_regionDetected = false;
static bool s_isChina = false;

static bool detectIsChina() {
    if (s_regionDetected) return s_isChina;

    // Fast check: try TCP connect to Alibaba (China CDN edge)
    QTcpSocket sock;
    sock.connectToHost(QStringLiteral("www.aliyun.com"), 443);
    bool connected = sock.waitForConnected(2000);
    sock.disconnectFromHost();

    s_isChina = connected;
    s_regionDetected = true;

    // Fallback: if CI, default to global (GitHub Actions runners are in US/EU)
    if (!connected && isCI()) s_isChina = false;

    TH_LOG_INFO(QStringLiteral("Region detected: %1").arg(s_isChina ? "China" : "Global"));
    return s_isChina;
}

// ═════════════════════════════════════════════════════════════════════════════
// Core targets — region-aware, always run
// ═════════════════════════════════════════════════════════════════════════════
static QVector<TestCase> chinaCoreTargets() {
    return {
        // Alibaba Cloud (aliyun.com) — China's largest cloud provider
        make("HTTPS Aliyun",         "https",    "www.aliyun.com",       443),
        make("HTTP Aliyun",          "http",     "www.aliyun.com",       80),

        // Tencent (qq.com) — China's largest internet company
        make("HTTPS Tencent",        "https",    "www.qq.com",           443),

        // SSH — GitHub (globally accessible even from China)
        make("SSH GitHub",           "ssh",      "github.com",            22),

        // httpbin.org — Cloudflare CDN, accessible globally including China
        make("HTTPS httpbin",        "https",    "httpbin.org",           443),
    };
}

static QVector<TestCase> globalCoreTargets() {
    return {
        // HTTP/HTTPS — httpbin.org (Cloudflare CDN, globally accessible)
        make("HTTPS httpbin",        "https",    "httpbin.org",           443),
        make("HTTP httpbin",         "http",     "httpbin.org",           80),

        // Google (globally accessible outside China)
        make("HTTPS Google",         "https",    "www.google.com",        443),

        // Cloudflare (1.1.1.1)
        make("HTTPS Cloudflare",     "https",    "one.one.one.one",       443),

        // SSH — GitHub (globally accessible)
        make("SSH GitHub",           "ssh",      "github.com",            22),
    };
}

QVector<TestCase> coreTargets() {
    return detectIsChina() ? chinaCoreTargets() : globalCoreTargets();
}

// ═════════════════════════════════════════════════════════════════════════════
// Extended targets — region-aware, run in CI
// ═════════════════════════════════════════════════════════════════════════════
static QVector<TestCase> chinaExtendedTargets() {
    return {
        // SMTP — QQ Mail (Tencent, China-accessible, banner grab no auth)
        make("SMTP QQ",              "smtp",     "smtp.qq.com",          25),

        // FTP — Alibaba open mirror (if available) or use speedtest
        make("FTP Tele2 (anon)",     "ftp",      "speedtest.tele2.net",  21),

        // Telnet — Star Wars ASCII art (globally accessible)
        make("Telnet Towel",         "telnet",   "towel.blinkenlights.nl", 23),

        // MQTT — public test broker
        make("MQTT Mosquitto",       "mqtt",     "test.mosquitto.org",   1883),

        // DNS — Alibaba DNS (223.5.5.5) connectivity + 114DNS (114.114.114.114)
        make("HTTPS AliDNS",         "https",    "alidns.com",            443),

        // LDAP — public test server
        make("LDAP ForumSys",        "ldap",     "ldap.forumsys.com",     389),
    };
}

static QVector<TestCase> globalExtendedTargets() {
    return {
        // SMTP — Google (globally accessible outside China, banner grab)
        make("SMTP Google",          "smtp",     "gmail-smtp-in.l.google.com", 25),

        // IMAP — Google (globally accessible outside China)
        // Note: Google requires OAuth2 for actual login; this tests reachability only
        make("IMAP Gmail",           "imap",     "imap.gmail.com",        993),

        // FTP — public anonymous
        make("FTP Tele2 (anon)",     "ftp",      "speedtest.tele2.net",  21),

        // Telnet — Star Wars ASCII art
        make("Telnet Towel",         "telnet",   "towel.blinkenlights.nl", 23),

        // MQTT — public test broker
        make("MQTT Mosquitto",       "mqtt",     "test.mosquitto.org",   1883),

        // LDAP — public test server
        make("LDAP ForumSys",        "ldap",     "ldap.forumsys.com",     389),

        // DNS test — Google DNS
        make("HTTPS Google DNS",     "https",    "dns.google",            443),
    };
}

QVector<TestCase> extendedTargets() {
    return detectIsChina() ? chinaExtendedTargets() : globalExtendedTargets();
}

// ═════════════════════════════════════════════════════════════════════════════
// Local-only targets — require running services, skipped in CI
// ═════════════════════════════════════════════════════════════════════════════
QVector<TestCase> localTargets() {
    return {
        // Database protocols — all need running local service
        make("MySQL local",          "mysql",        "localhost",  3306),
        make("PostgreSQL local",     "postgresql",   "localhost",  5432),
        make("Redis local",          "redis",        "localhost",  6379),
        make("MongoDB local",        "mongodb",      "localhost",  27017),
        make("MSSQL local",          "mssql",        "localhost",  1433),

        // Secure variants — localhost only
        make("FTPS local",           "ftps",         "localhost",   990),
        make("SFTP local",           "sftp",         "localhost",    22),
        make("SCP local",            "scp",          "localhost",    22),
        make("IMAPS local",          "imaps",        "localhost",   993),
        make("POP3S local",          "pop3s",        "localhost",   995),
        make("SMTP TLS local",       "smtps",        "localhost",   465),
        make("LDAPS local",          "ldaps",        "localhost",   636),
        make("MQTTS local",          "mqtts",        "localhost",  8883),

        // RDP — localhost reachability
        make("RDP local",            "rdp",          "localhost",  3389),
    };
}

// ═════════════════════════════════════════════════════════════════════════════
// Full schema sweep — all targets
// ═════════════════════════════════════════════════════════════════════════════
QVector<TestCase> allSchemas() {
    QVector<TestCase> all = coreTargets();
    all << extendedTargets();
    all << localTargets();
    return all;
}

// ═════════════════════════════════════════════════════════════════════════════
// UI simulation tests — AppState-only, no QML needed
// ═════════════════════════════════════════════════════════════════════════════
QVector<TestCase> uiSimulation() {
    QVector<TestCase> tests;

    // Test 1: Basic HTTPS entry
    TestCase t1 = make("UI: HTTPS entry", "https", "example.com");
    t1.steps.append({"Type hostname", "", "example.com"});
    t1.steps.append({"Select scheme", "http (default)", "https"});
    tests << t1;

    // Test 2: Paste URL parsing
    TestCase t2 = make("UI: Paste URL", "https", "ftp.example.com");
    t2.steps.append({"Paste full URL", "", "ftp://user:pass@ftp.example.com:2121"});
    t2.steps.append({"Verify scheme parsed", "", "ftp"});
    t2.steps.append({"Verify port parsed", "", "2121"});
    t2.steps.append({"Verify user parsed", "", "user"});
    tests << t2;

    // Test 3: Scheme change triggers field visibility
    TestCase t3 = make("UI: Schema fields", "https", "example.com");
    t3.steps.append({"Open advanced", "", "gear clicked"});
    t3.steps.append({"Only port visible (https)", "_showUser=false _showPass=false", ""});
    t3.steps.append({"Switch to ftp", "https", "ftp"});
    t3.steps.append({"User+Pass+Port visible (ftp)", "_showUser=true _showPass=true", ""});
    t3.steps.append({"Switch to ssh", "ftp", "ssh"});
    t3.steps.append({"User visible, Pass hidden (ssh)", "_showUser=true _showPass=false", ""});
    tests << t3;

    // Test 4: Port default changes with scheme
    TestCase t4 = make("UI: Custom port", "https", "example.com");
    t4.steps.append({"Set custom port 8443", "", "8443"});
    t4.steps.append({"Switch to MySQL", "https", "mysql"});
    t4.steps.append({"Port default updates", "443", "3306"});
    tests << t4;

    // Test 5: Auth credentials for database protocol
    TestCase t5 = makeWithAuth("UI: DB auth", "mysql", "localhost", 3306, "root", "secret");
    t5.steps.append({"Scheme=mysql → user+pass fields visible", "", ""});
    t5.steps.append({"Username set", "", "root"});
    t5.steps.append({"Password set", "", "***"});
    tests << t5;

    return tests;
}

// ═════════════════════════════════════════════════════════════════════════════
// CI entry point — picks appropriate scenarios based on environment
// ═════════════════════════════════════════════════════════════════════════════
QVector<TestCase> ciScenarios() {
    QVector<TestCase> tests = coreTargets();

    if (isCI()) {
        tests << extendedTargets();
        tests << uiSimulation();
    }

    return tests;
}

} // namespace TestScenarios

#endif // ND_TESTING
