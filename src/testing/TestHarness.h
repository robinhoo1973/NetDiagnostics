// =============================================================================
// TestHarness.h — Headless automated testing framework (ND_TESTING only)
// =============================================================================
// Compile-time guarded: zero code emitted when ND_TESTING is not defined.
// All test code is in #ifdef ND_TESTING blocks — completely isolated from
// production builds.
// =============================================================================
#pragma once

#ifdef ND_TESTING

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QElapsedTimer>
#include <QVector>
#include <QMap>
#include <functional>

#include "models/DiagId.h"
#include "models/DiagnosticResult.h"

// ═════════════════════════════════════════════════════════════════════════════
// TestStep — records one simulated user action + before/after values
// ═════════════════════════════════════════════════════════════════════════════
struct TestStep {
    QString     description;       // what the user did
    QString     elementBefore;     // UI element state before
    QString     elementAfter;      // UI element state after
    qint64      durationMs = 0;
    bool        passed = true;
    QString     errorMsg;
};

// ═════════════════════════════════════════════════════════════════════════════
// TestCase — one test scenario (e.g., "https://example.com")
// ═════════════════════════════════════════════════════════════════════════════
struct TestCase {
    QString     name;               // test case name
    QString     scheme;             // URL scheme (https, ftp, ssh, ...)
    QString     host;               // target hostname
    int         port = -1;          // custom port (-1 = use scheme default)
    QString     username;           // optional username
    QString     password;           // optional password
    QVector<TestStep> steps;        // recorded simulation steps
    QMap<DiagId, DiagnosticResult> results; // diagnostic results
    bool        passed = true;
    QString     summary;
};

// ═════════════════════════════════════════════════════════════════════════════
// TestHarness — main testing engine
// ═════════════════════════════════════════════════════════════════════════════
class TestHarness : public QObject {
    Q_OBJECT
public:
    static TestHarness& instance();

    // ── Configuration ─────────────────────────────────────────────────
    void setLogPath(const QString& path);
    void setScreenshotDir(const QString& dir);
    void setHeadless(bool hl) { m_headless = hl; }
    bool isHeadless() const { return m_headless; }
    bool isEnabled() const { return m_enabled; }

    // ── Logging ───────────────────────────────────────────────────────
    void logInfo(const QString& msg);
    void logError(const QString& msg);
    void logStep(const QString& description, const QString& before, const QString& after);
    void logDiagResult(DiagId id, const DiagnosticResult& r);

    // ── Simulation helpers ────────────────────────────────────────────
    // These are called by test scenarios to simulate user actions.
    // They record before/after state changes.
    void simSetScheme(const QString& scheme);
    void simSetHost(const QString& host);
    void simSetPort(int port);
    void simSetUsername(const QString& user);
    void simSetPassword(const QString& pass);
    void simClickRun();
    void simClickStop();

    // ── Test case execution ───────────────────────────────────────────
    void runTestCase(const TestCase& tc);
    void runAllSchemas();

    // ── Results ───────────────────────────────────────────────────────
    void printSummary();
    bool allPassed() const { return m_failCount == 0; }
    int passCount() const { return m_passCount; }
    int failCount() const { return m_failCount; }

    // ── Screenshots (headless-friendly) ───────────────────────────────
    void takeScreenshot(const QString& label);

private:
    TestHarness() = default;
    void writeLog(const QString& level, const QString& msg);
    QString now() const;
    TestCase buildTestCase(const QString& scheme, const QString& host, int port = -1);

    QFile       m_logFile;
    QTextStream m_logStream;
    QString     m_screenshotDir;
    bool        m_headless = true;
    bool        m_enabled = true;
    int         m_passCount = 0;
    int         m_failCount = 0;
    QVector<TestCase> m_testCases;
    QElapsedTimer m_timer;
};

// ═════════════════════════════════════════════════════════════════════════════
// Convenience macros — no-op when ND_TESTING not defined
// ═════════════════════════════════════════════════════════════════════════════
#define TH_LOG_INFO(msg)    TestHarness::instance().logInfo(msg)
#define TH_LOG_ERROR(msg)   TestHarness::instance().logError(msg)
#define TH_LOG_STEP(desc, before, after) TestHarness::instance().logStep(desc, before, after)

#else  // !ND_TESTING

// No-op stubs — compiler optimizes these away entirely
#define TH_LOG_INFO(msg)    ((void)0)
#define TH_LOG_ERROR(msg)   ((void)0)
#define TH_LOG_STEP(d,b,a)  ((void)0)

#endif // ND_TESTING
