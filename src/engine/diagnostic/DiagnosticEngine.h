// =============================================================================
// DiagnosticEngine.h — Orchestrator: dispatches tests, native→Qt fallback
// =============================================================================
#pragma once

#include <QObject>
#include <QFuture>
#include <memory>
#include <atomic>
#include "models/DiagId.h"
#include "models/DiagnosticResult.h"

class DiagnosticEngine : public QObject {
    Q_OBJECT
public:
    explicit DiagnosticEngine(QObject* parent = nullptr);
    ~DiagnosticEngine() override;

    /// Run a single diagnostic test. Port params only used for G4PortScan.
    QFuture<DiagnosticResult> runDiag(DiagId id, const QString& target = {},
                                       int fromPort = 0, int toPort = 0,
                                       bool useCommonPorts = true);

private:
    DiagnosticResult runG1(DiagId id, const QString& target);
    DiagnosticResult runG2(DiagId id, const QString& target);
    DiagnosticResult runG3(DiagId id, const QString& target);
    DiagnosticResult runG4(DiagId id, const QString& target, int fromPort, int toPort, bool useCommonPorts);
    DiagnosticResult runG5(DiagId id, const QString& target);

    /// Try native plugin first; if unavailable/unsupported, returns nullopt.
    std::optional<DiagnosticResult> tryNative(DiagId id, const QString& target,
                                               int fromPort = 0, int toPort = 0);

    std::atomic<bool> m_destroying{false};
};