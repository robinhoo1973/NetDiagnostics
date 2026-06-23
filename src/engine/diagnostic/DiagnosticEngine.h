// =============================================================================
// DiagnosticEngine.h — Dispatches tests to pure C++ group handlers
// =============================================================================
#pragma once

#include <QObject>
#include <QFuture>
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
    DiagnosticResult runG1(DiagId id);
    DiagnosticResult runG2(DiagId id);
    DiagnosticResult runG3(DiagId id);
    DiagnosticResult runG4(DiagId id, const QString& target, int fromPort, int toPort, bool useCommonPorts);
    DiagnosticResult runG5(DiagId id, const QString& target);

    std::atomic<bool> m_destroying{false};
};