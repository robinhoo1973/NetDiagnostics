// =============================================================================
// TestScenarios.cpp — Concrete test scenario definitions
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

// ── Core scenarios (always run, fast, public services) ─────────────────────
QVector<TestCase> coreTargets() {
    return {
        make("HTTPS httpbin",       "https",    "httpbin.org",           443),
        make("HTTP httpbin",        "http",     "httpbin.org",           80),
        make("SSH GitHub",          "ssh",      "github.com",            22),
        make("HTTPS google",        "https",    "www.google.com",        443),
        make("DNS test",            "https",    "dns.google",            443),
    };
}

// ── Extended scenarios (slower, may fail in restricted CI) ─────────────────
QVector<TestCase> extendedTargets() {
    return {
        make("FTP GNU",             "ftp",      "ftp.gnu.org",           21),
        make("SMTP Google",         "smtp",     "gmail-smtp-in.l.google.com", 25),
        make("IMAP Gmail",          "imap",     "imap.gmail.com",        993),
        make("POP3 Gmail",          "pop3",     "pop.gmail.com",         995),
        make("Telnet Towel",        "telnet",   "towel.blinkenlights.nl", 23),
        make("MQTT Mosquitto",      "mqtt",     "test.mosquitto.org",    1883),
        make("LDAP ForumSys",       "ldap",     "ldap.forumsys.com",     389),
    };
}

// ── Full schema sweep (all 23 schemes — localhost for DBs) ─────────────────
QVector<TestCase> allSchemas() {
    QVector<TestCase> all = coreTargets();
    all << extendedTargets();
    // Local-only targets (require running services)
    all << make("MySQL local",          "mysql",        "localhost", 3306);
    all << make("PostgreSQL local",     "postgresql",   "localhost", 5432);
    all << make("Redis local",          "redis",        "localhost", 6379);
    all << make("MongoDB local",        "mongodb",      "localhost", 27017);
    all << make("MSSQL local",          "mssql",        "localhost", 1433);
    all << make("RDP local",            "rdp",          "localhost", 3389);
    all << make("LDAPS local",          "ldaps",        "localhost", 636);
    all << make("MQTTS local",          "mqtts",        "localhost", 8883);
    all << make("SMTP TLS local",       "smtps",        "localhost", 465);
    all << make("FTPS local",           "ftps",         "localhost", 990);
    all << make("SFTP local",           "sftp",         "localhost", 22);
    all << make("SCP local",            "scp",          "localhost", 22);
    all << make("IMAPS local",          "imaps",        "localhost", 993);
    all << make("POP3S local",          "pop3s",        "localhost", 995);
    return all;
}

// ── UI simulation tests ────────────────────────────────────────────────────
QVector<TestCase> uiSimulation() {
    QVector<TestCase> tests;

    // Test 1: Basic HTTPS entry
    TestCase t1 = make("UI: HTTPS entry", "https", "example.com");
    t1.steps.append({"Type hostname", "", "example.com"});
    t1.steps.append({"Select scheme", "http", "https"});
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
    t3.steps.append({"Open advanced", "", "⚙ clicked"});
    t3.steps.append({"Only port visible", "_showUser=false", ""});
    t3.steps.append({"Switch to ftp", "https", "ftp"});
    t3.steps.append({"User+Pass+Port visible", "_showUser=true _showPass=true", ""});
    tests << t3;

    // Test 4: Port override
    TestCase t4 = make("UI: Custom port", "https", "example.com");
    t4.steps.append({"Set custom port", "", "8443"});
    t4.steps.append({"Switch to MySQL", "https", "mysql"});
    t4.steps.append({"Port placeholder updates", "443", "3306"});
    tests << t4;

    return tests;
}

// ── CI entry point ─────────────────────────────────────────────────────────
QVector<TestCase> ciScenarios() {
    QVector<TestCase> tests = coreTargets();

    if (isCI()) {
        // GitHub Actions: run extended but skip localhost-only
        tests << extendedTargets();
        // Run UI simulation in CI (headless, AppState-only, no QML)
        tests << uiSimulation();
    }

    return tests;
}

} // namespace TestScenarios

#endif // ND_TESTING
