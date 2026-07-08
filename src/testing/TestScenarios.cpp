// =============================================================================
// TestScenarios.cpp — Concrete test scenario definitions
// =============================================================================
// Target selection principles:
// 1. China-accessible: no google.com, no gmail, no services blocked by GFW
// 2. Auth-aware: only test with user/pass when the protocol genuinely needs it
// 3. CI-safe: all core targets reachable from GitHub Actions runners
// 4. Public services: nobody's private servers; no credentials exposed
// =============================================================================
#ifdef ND_TESTING

#include "testing/TestScenarios.h"
#include "testing/TestHarness.h"

#include <QProcessEnvironment>

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
// Core scenarios — always run, fast, global + China-accessible
// ═════════════════════════════════════════════════════════════════════════════
QVector<TestCase> coreTargets() {
    return {
        // HTTP/HTTPS — httpbin.org (Cloudflare CDN, globally accessible)
        make("HTTPS httpbin",       "https",    "httpbin.org",           443),
        make("HTTP httpbin",        "http",     "httpbin.org",           80),

        // SSH — GitHub (globally accessible, returns SSH banner)
        // No auth: SSH banner reading works without credentials
        make("SSH GitHub",          "ssh",      "github.com",            22),

        // HTTPS — Cloudflare (1.1.1.1, globally accessible DNS)
        make("HTTPS Cloudflare",    "https",    "one.one.one.one",       443),

        // HTTPS — Baidu (China-accessible, reliable CDN)
        make("HTTPS Baidu",         "https",    "www.baidu.com",         443),
    };
}

// ═════════════════════════════════════════════════════════════════════════════
// Extended scenarios — run in CI, skip localhost
// ═════════════════════════════════════════════════════════════════════════════
QVector<TestCase> extendedTargets() {
    return {
        // FTP — anonymous login (no auth needed)
        // ftp.gnu.org has been slow; speedtest.tele2.net is a public FTP test
        make("FTP Tele2 (anon)",         "ftp",      "speedtest.tele2.net",  21),

        // SMTP — smtp.qq.com (China-accessible, no auth for HELO/EHLO banner)
        // Banner grab works without credentials — just opens TCP, reads greeting
        make("SMTP QQ",                  "smtp",     "smtp.qq.com",          25),

        // Telnet — Star Wars ASCII art (globally accessible, no auth)
        make("Telnet Towel",             "telnet",   "towel.blinkenlights.nl", 23),

        // MQTT — public test broker by Eclipse Mosquitto project
        // No auth required for connection
        make("MQTT Mosquitto",           "mqtt",     "test.mosquitto.org",   1883),

        // LDAP — public test server from forumsys.com
        // Accepts anonymous BIND for read-only; no auth needed for banner
        make("LDAP ForumSys",            "ldap",     "ldap.forumsys.com",     389),

        // RDP — test target on common port (banner/pre-connect detection)
        // No auth: just checks if RDP service responds on TCP 3389
        // Note: most public RDP servers reject unauthenticated connections;
        // this test verifies reachability, not login
        make("RDP (reachability)",       "rdp",      "localhost",            3389),
    };
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

        // Secure variants — localhost only (need self-signed certs)
        make("FTPS local",           "ftps",         "localhost",   990),
        make("SFTP local",           "sftp",         "localhost",    22),
        make("SCP local",            "scp",          "localhost",    22),
        make("IMAPS local",          "imaps",        "localhost",   993),
        make("POP3S local",          "pop3s",        "localhost",   995),
        make("SMTP TLS local",       "smtps",        "localhost",   465),
        make("LDAPS local",          "ldaps",        "localhost",   636),
        make("MQTTS local",          "mqtts",        "localhost",  8883),
    };
}

// ═════════════════════════════════════════════════════════════════════════════
// Full schema sweep
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

    // Test 3: Scheme change triggers field visibility (no auth → auth protocol)
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

    // localTargets() never runs in CI — requires actual services

    return tests;
}

} // namespace TestScenarios

#endif // ND_TESTING
