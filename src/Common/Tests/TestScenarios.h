// =============================================================================
// TestScenarios.h — Predefined test scenarios for automated CI testing
// =============================================================================
#pragma once

#if defined(ND_TESTING)

#include <QVector>
#include <QString>

struct TestCase;

namespace TestScenarios {

// ── Core scenarios (always run, fast) ─────────────────────────────────
QVector<TestCase> coreTargets();       // httpbin.org (https+http), github.com (ssh)

// ── Extended scenarios (slower, CI-conditional) ───────────────────────
QVector<TestCase> extendedTargets();   // FTP, SMTP, IMAP, Redis, Telnet, MQTT

// ── Full schema sweep ─────────────────────────────────────────────────
QVector<TestCase> allSchemas();        // All 23 supported schemes

// ── UI simulation tests ───────────────────────────────────────────────
QVector<TestCase> uiSimulation();      // Simulate clicks, text input, scheme changes

// ── CI entry point — picks appropriate scenarios based on env ─────────
QVector<TestCase> ciScenarios();       // Core + detects CI env for extended

} // namespace TestScenarios

#endif // ND_TESTING
