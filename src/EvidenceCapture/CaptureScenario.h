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
    QVector<CaptureStep> m_steps;
};

// ═════════════════════════════════════════════════════════════════════════════
// Default scenario: covers all tabs + diagnostic + detail + dashboard + report
// ═════════════════════════════════════════════════════════════════════════════
inline CaptureScenario buildDefaultScenario(const QString& diagUrl) {
    CaptureScenario s;

    s.addSteps({
        // ── Phase 1: All tabs ──────────────────────────────────────────
        {StepAction::Navigate,       "0", "Dashboard",     true,  false},
        {StepAction::WaitPageReady,  "3000", "",            false, false},
        {StepAction::Capture,        "01_dashboard",       "",    false},

        {StepAction::Navigate,       "1", "Diagnostics",   true,  false},
        {StepAction::WaitPageReady,  "3000", "",            false, false},
        {StepAction::Capture,        "02_diagnostic",      "",    false},

        {StepAction::Navigate,       "2", "Config",        true,  false},
        {StepAction::WaitPageReady,  "3000", "",            false, false},
        {StepAction::Capture,        "03_config",          "",    false},

        {StepAction::Navigate,       "3", "Settings",      true,  false},
        {StepAction::WaitPageReady,  "3000", "",            false, false},
        {StepAction::Capture,        "04_settings",        "",    false},

        // ── Phase 2: Diagnostic run ────────────────────────────────────
        {StepAction::Navigate,       "1", "Diagnostics: run", true, false},
        {StepAction::SetUrl,         diagUrl, "",            false, false},
        {StepAction::Capture,        "05_pre_diag",         "",    false},
        {StepAction::RunDiagnostic,  "", "",                 false, false},
        {StepAction::WaitDiagComplete,"120000", "",          false, false},
        {StepAction::Capture,        "06_diag_complete",    "",    false},

        // ── Phase 3: InternetConnectivity detail ───────────────────────
        {StepAction::OpenDetail,
         QString::number(static_cast<int>(DiagId::G3InternetConnectivity)),
         "InternetConnectivity", true, false},
        {StepAction::WaitPageReady,  "2000", "",            false, false},
        {StepAction::Capture,        "07_connectivity_detail","",   false},
        // Scroll during recording only — captures the full detail content
        {StepAction::Scroll,         "3000", "Connectivity scroll", false, true},
        {StepAction::Capture,        "08_connectivity_end", "",    false},

        // ── Phase 4: Dashboard ─────────────────────────────────────────
        {StepAction::Navigate,       "0", "Dashboard",     true,  false},
        {StepAction::WaitPageReady,  "3000", "",            false, false},
        {StepAction::Capture,        "09_dashboard",       "",    false},
        // Scroll during recording — captures dashboard scroll content
        {StepAction::Scroll,         "5000", "Dashboard scroll", false, true},
        {StepAction::Capture,        "10_dashboard_end",   "",    false},

        // ── Phase 5: Report preview ────────────────────────────────────
        {StepAction::OpenReport,     "", "Report Preview", true,  false},
        {StepAction::WaitPageReady,  "3000", "",            false, false},
        {StepAction::Capture,        "11_report",          "",    false},
    });

    return s;
}
