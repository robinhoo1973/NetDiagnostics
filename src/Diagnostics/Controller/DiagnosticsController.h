// =============================================================================
// DiagnosticsController.h — Diagnostics page controller (MVC stub)
//
// Currently delegates to AppState. Will be extracted from AppState in batch 3.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>

class AppState;

class DiagnosticsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentDiagLabel READ currentDiagLabel NOTIFY currentDiagChanged)
    Q_PROPERTY(QString currentGroup READ currentGroup NOTIFY groupChanged)

public:
    explicit DiagnosticsController(AppState* appState, QObject* parent = nullptr);

    Q_INVOKABLE void runDiagnostics();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void reset();

    QString currentDiagLabel() const;
    QString currentGroup() const;

signals:
    void currentDiagChanged();
    void groupChanged();
    void runStatusChanged();

private:
    AppState* m_appState;
};
