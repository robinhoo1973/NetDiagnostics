// =============================================================================
// CaptureScenario.h — Declarative step definitions for capture workflow
// =============================================================================
// A Scenario is a sequence of CaptureStep entries that define what the
// CaptureOrchestrator should do: navigate to pages, set URLs, run diagnostics,
// open detail views, scroll, capture screenshots.
//
// Design ref: docs/AutomatedEvidenceCapture_Design.md §4.3
// =============================================================================
#pragma once

#include <QString>
#include <QVector>
#include <QObject>
#include "Common/Model/DiagId.h"

// ═════════════════════════════════════════════════════════════════════════════
// StepAction — what the orchestrator should do for this step
// ═════════════════════════════════════════════════════════════════════════════
enum class StepAction {
    Navigate,            // param = tab index "0"-"3", desc = page name
    WaitPageReady,       // param = timeout ms, desc = ""
    Capture,             // param = filename label, desc = human description
    Scroll,              // param = duration ms (only in recording mode)
    SetUrl,              // param = diagnostic URL
    RunDiagnostic,       // no param
    WaitDiagComplete,    // param = timeout ms
    OpenDetail,          // param = diagId (int as string)
    OpenReport,          // no param
};

// ═════════════════════════════════════════════════════════════════════════════
// CaptureStep — one step in the scenario
// ═════════════════════════════════════════════════════════════════════════════
struct CaptureStep {
    StepAction action;          // what to do
    QString    param;           // action-specific parameter (tab idx, url, timeout, ...)
    QString    description;     // human-readable label for UI & filename
    bool       captureBefore = false;  // take screenshot BEFORE executing action
    bool       recordingOnly = false;  // only execute in recording/both mode
};

// ═════════════════════════════════════════════════════════════════════════════
// CaptureScenario — container for a sequence of steps
// ═════════════════════════════════════════════════════════════════════════════
class CaptureScenario {
public:
    CaptureScenario() = default;

    // 5WHY: createSession() needs to distinguish scenarios in session
    // directory names (blueprint §11.2).  Without a scenarioId on the
    // container, the naming logic had to guess or hardcode.
    QString scenarioId() const { return m_scenarioId; }
    void setScenarioId(const QString& id) { m_scenarioId = id; }

    void addStep(const CaptureStep& step) { m_steps.append(step); }
    void addSteps(const QVector<CaptureStep>& steps) { m_steps.append(steps); }

    const QVector<CaptureStep>& steps() const { return m_steps; }
    int stepCount() const { return m_steps.size(); }
    const CaptureStep* stepAt(int index) const {
        if (index < 0 || index >= m_steps.size()) return nullptr;
        return &m_steps.at(index);
    }

    void clear() { m_steps.clear(); }

private:
    QString m_scenarioId;
    QVector<CaptureStep> m_steps;
};

// ═════════════════════════════════════════════════════════════════════════════
// Default scenario: covers all tabs + diagnostic + detail + dashboard + report
// ═════════════════════════════════════════════════════════════════════════════
inline CaptureScenario buildDefaultScenario(const QString& diagUrl) {
    CaptureScenario s;

    s.addSteps({
        // ── Phase 1: All tabs ──────────────────────────────────────────
        {StepAction::Navigate,       "0", "000_Dashboard",     true,  false},
        {StepAction::WaitPageReady,  "3000", "",                false, false},
        {StepAction::Capture,        "001_Dashboard_Main",     "",    false},

        {StepAction::Navigate,       "1", "001_Diagnostics",   true,  false},
        {StepAction::WaitPageReady,  "3000", "",                false, false},
        {StepAction::Capture,        "002_Diagnostic",         "",    false},

        {StepAction::Navigate,       "2", "002_Config",        true,  false},
        {StepAction::WaitPageReady,  "3000", "",                false, false},
        {StepAction::Capture,        "003_Config",             "",    false},

        {StepAction::Navigate,       "3", "003_Settings",      true,  false},
        {StepAction::WaitPageReady,  "3000", "",                false, false},
        {StepAction::Capture,        "004_Settings",           "",    false},

        // ── Phase 2: Diagnostic run ────────────────────────────────────
        {StepAction::Navigate,       "1", "004_Diagnostic_PreRun", true, false},
        {StepAction::SetUrl,         diagUrl, "",            false, false},
        {StepAction::Capture,        "005_Diagnostic_Input",  "",    false},
        {StepAction::RunDiagnostic,  "", "",                 false, false},
        {StepAction::WaitDiagComplete,"120000", "",          false, false},
        {StepAction::Capture,        "010_Diagnostic_Result", "",    false},

        // ── Phase 3: InternetConnectivity detail ───────────────────────
        {StepAction::OpenDetail,
         QString::number(static_cast<int>(DiagId::G3InternetConnectivity)),
         "011_InternetConnectivity", true, false},
        {StepAction::WaitPageReady,  "2000", "",            false, false},
        {StepAction::Capture,        "011_InternetConnectivity_Top","",false},
        // Scroll during recording only — captures the full detail content
        {StepAction::Scroll,         "3000", "Connectivity scroll", false, true},
        {StepAction::Capture,        "012_InternetConnectivity_Bottom","",false},

        // ── Phase 4: Dashboard ─────────────────────────────────────────
        {StepAction::Navigate,       "0", "020_Dashboard",     true,  false},
        {StepAction::WaitPageReady,  "3000", "",                false, false},
        {StepAction::Capture,        "020_Dashboard_AfterDiagnostic","",false},
        // Scroll during recording — captures dashboard scroll content
        {StepAction::Scroll,         "5000", "Dashboard scroll", false, true},
        {StepAction::Capture,        "021_Dashboard_End",     "",    false},

        // ── Phase 5: Report preview ────────────────────────────────────
        {StepAction::OpenReport,     "", "030_Report",        true,  false},
        {StepAction::WaitPageReady,  "3000", "",               false, false},
        {StepAction::Capture,        "030_Report_Summary",    "",    false},
    });

    return s;
}
