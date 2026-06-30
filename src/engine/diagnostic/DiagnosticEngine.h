// =============================================================================
// DiagnosticEngine.h — Dispatches tests to pure C++ group handlers
// =============================================================================
#pragma once

#include <QObject>
#include <QFuture>
#include <atomic>
#include <memory>
#include "models/DiagId.h"
#include "models/DiagnosticResult.h"

class INetworkController;
class IHttpClient;
class INetworkProbe;

class DiagnosticEngine : public QObject {
    Q_OBJECT
public:
    explicit DiagnosticEngine(QObject* parent = nullptr);
    ~DiagnosticEngine() override;

    /// Run a single diagnostic test. Port params only used for G4PortScan.
    QFuture<DiagnosticResult> runDiag(DiagId id, const QString& target = {},
                                       int fromPort = 0, int toPort = 0,
                                       bool useCommonPorts = true);
    /// Synchronous variant — use from worker threads to avoid QThreadPool deadlock.
    DiagnosticResult runDiagSync(DiagId id, const QString& target = {},
                                  int fromPort = 0, int toPort = 0,
                                  bool useCommonPorts = true);

private:
    DiagnosticResult runG1(DiagId id);
    DiagnosticResult runG2(DiagId id);
    DiagnosticResult runG3(DiagId id);
    DiagnosticResult runG4(DiagId id, const QString& target, int fromPort, int toPort, bool useCommonPorts);
    DiagnosticResult runG5(DiagId id, const QString& target);

    // MVC controllers — platform-specific via ControllerFactory
    std::unique_ptr<INetworkController> m_networkCtrl;
    std::unique_ptr<IHttpClient> m_httpClient;
    std::unique_ptr<INetworkProbe> m_networkProbe;

    std::atomic<bool> m_destroying{false};
};