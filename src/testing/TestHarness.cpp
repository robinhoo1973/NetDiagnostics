// =============================================================================
// TestHarness.cpp — Headless automated testing framework implementation
// =============================================================================
#ifdef ND_TESTING

#include "testing/TestHarness.h"
#include "testing/TestScenarios.h"
#include "app/AppState.h"
#include "models/DiagId.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QEventLoop>

// ── Singleton ──────────────────────────────────────────────────────────────
TestHarness& TestHarness::instance() {
    static TestHarness s;
    return s;
}

// ── Configuration ──────────────────────────────────────────────────────────
void TestHarness::setLogPath(const QString& path) {
    m_logFile.setFileName(path);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logStream.setDevice(&m_logFile);
        m_logStream << "\n=== NetDiagnostics Test Harness ===\n";
        m_logStream << "Started: " << now() << "\n\n";
    }
}

void TestHarness::setScreenshotDir(const QString& dir) {
    m_screenshotDir = dir;
    QDir().mkpath(dir);
}

QString TestHarness::now() const {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

// ── Logging ────────────────────────────────────────────────────────────────
void TestHarness::writeLog(const QString& level, const QString& msg) {
    QString line = QStringLiteral("[%1] [%2] %3").arg(now(), level, msg);
    if (m_logStream.device()) {
        m_logStream << line << "\n";
        m_logStream.flush();
    }
    // Also print to stdout for CI visibility
    fprintf(stdout, "%s\n", line.toUtf8().constData());
    fflush(stdout);
}

void TestHarness::logInfo(const QString& msg)  { writeLog("INFO", msg); }
void TestHarness::logError(const QString& msg) { writeLog("ERROR", msg); }

void TestHarness::logStep(const QString& description, const QString& before, const QString& after) {
    writeLog("STEP", description);
    if (!before.isEmpty()) writeLog("  BEFORE", before);
    if (!after.isEmpty())  writeLog("  AFTER",  after);
}

void TestHarness::logDiagResult(DiagId id, const DiagnosticResult& r) {
    QString statusStr;
    switch (r.status) {
        case DiagStatus::Pass:    statusStr = "PASS"; break;
        case DiagStatus::Warning: statusStr = "WARN"; break;
        case DiagStatus::Fail:    statusStr = "FAIL"; break;
        case DiagStatus::Skipped: statusStr = "SKIP"; break;
        case DiagStatus::Error:   statusStr = "ERROR"; break;
        case DiagStatus::Info:    statusStr = "INFO"; break;
    }
    writeLog("RESULT", QStringLiteral("  [%1] %2 — %3 (%4ms)")
        .arg(statusStr, -5)
        .arg(r.summary)
        .arg(QString::number(static_cast<int>(id)))
        .arg(r.durationMs));
}

// ── Simulation ─────────────────────────────────────────────────────────────
void TestHarness::simSetScheme(const QString& scheme) {
    logStep("Select scheme: " + scheme, "", "");
    // Directly set AppState properties (headless mode skips QML)
}

void TestHarness::simSetHost(const QString& host) {
    logStep("Enter host: " + host, "", "");
}

void TestHarness::simSetPort(int port) {
    logStep("Enter port: " + QString::number(port), "", "");
}

void TestHarness::simSetUsername(const QString& user) {
    logStep("Enter username: " + user, "", "");
}

void TestHarness::simSetPassword(const QString& pass) {
    logStep("Enter password: " + (pass.isEmpty() ? "(empty)" : "***"), "", "");
}

void TestHarness::simClickRun() {
    logStep("Click RUN button", "", "");
}

void TestHarness::simClickStop() {
    logStep("Click STOP button", "", "");
}

// ── Test case execution ────────────────────────────────────────────────────
void TestHarness::runTestCase(const TestCase& tc) {
    m_timer.start();
    logInfo(QStringLiteral("══════ Test Case: %1 ══════").arg(tc.name));
    logInfo(QStringLiteral("  Scheme: %1://  Host: %2  Port: %3")
        .arg(tc.scheme, tc.host).arg(tc.port > 0 ? QString::number(tc.port) : "default"));

    // Simulate user actions
    simSetScheme(tc.scheme);
    simSetHost(tc.host);
    if (tc.port > 0) simSetPort(tc.port);
    if (!tc.username.isEmpty()) simSetUsername(tc.username);
    if (!tc.password.isEmpty()) simSetPassword(tc.password);

    // Construct the target URL
    QString target;
    if (tc.port > 0)
        target = QStringLiteral("%1://%2:%3").arg(tc.scheme, tc.host).arg(tc.port);
    else
        target = QStringLiteral("%1://%2").arg(tc.scheme, tc.host);

    logInfo("Target: " + target);

    // Headless: directly set AppState target and run diagnostics
    AppState* appState = qobject_cast<AppState*>(
        QCoreApplication::instance()->findChild<AppState*>());
    if (!appState) {
        logError("AppState not found — cannot run diagnostics headless");
        m_failCount++;
        return;
    }

    simClickRun();
    appState->setTarget(target);
    appState->runDiagnostics();

    // Wait for diagnostics to complete (headless mode — no event loop needed)
    // The runDiagnostics call is synchronous for the launch; results come via signals.
    // For CI, we process events until completion.
    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(appState, &AppState::runStatusChanged, [&]() {
        if (appState->runStatusInt() != 1) loop.quit(); // not Running → done
    });
    watchdog.start(120000); // 2-minute max per test case
    loop.exec();

    // Collect results
    for (DiagId id : DiagnosticConfig::allDiagIds()) {
        auto r = appState->getDetailResult(static_cast<int>(id));
        if (r.has_value()) {
            logDiagResult(id, r.value());
        }
    }

    qint64 elapsed = m_timer.elapsed();
    logInfo(QStringLiteral("  Test case completed in %1ms").arg(elapsed));

    if (allPassed()) m_passCount++; else m_failCount++;
}

void TestHarness::runAllSchemas() {
    logInfo("══════ Running All Schema Tests ══════");

    // Define targets per scheme — use reliable, stable hosts for CI
    struct SchemaTarget { QString scheme; QString host; int port; };
    const QVector<SchemaTarget> targets = {
        {"https",    "httpbin.org",            443},
        {"http",     "httpbin.org",            80},
        {"ssh",      "github.com",             22},
        {"ftp",      "ftp.gnu.org",            21},
        {"smtp",     "gmail-smtp-in.l.google.com", 25},
        {"imap",     "imap.gmail.com",         993},
        {"pop3",     "pop.gmail.com",          995},
        {"mysql",    "localhost",              3306},
        {"redis",    "localhost",              6379},
        {"telnet",   "towel.blinkenlights.nl", 23},
        {"ldap",     "ldap.forumsys.com",      389},
        {"mqtt",     "test.mosquitto.org",     1883},
        {"postgresql","localhost",             5432},
        {"mongodb",  "localhost",              27017},
        {"rdp",      "localhost",              3389},
    };

    for (const auto& st : targets) {
        TestCase tc;
        tc.name   = st.scheme + "://" + st.host;
        tc.scheme = st.scheme;
        tc.host   = st.host;
        tc.port   = st.port;
        runTestCase(tc);
    }

    printSummary();
}

void TestHarness::printSummary() {
    writeLog("SUMMARY", QStringLiteral("══════════════════════════════"));
    writeLog("SUMMARY", QStringLiteral("  Passed: %1  Failed: %2  Total: %3")
        .arg(m_passCount).arg(m_failCount).arg(m_passCount + m_failCount));
    writeLog("SUMMARY", QStringLiteral("  All passed: %1")
        .arg(allPassed() ? "YES" : "NO"));
}

void TestHarness::takeScreenshot(const QString& label) {
    if (m_screenshotDir.isEmpty()) return;
    // Headless: skip actual screenshot capture; log the attempt
    logInfo("Screenshot: " + label + " (headless — skipped)");
}

#endif // ND_TESTING
