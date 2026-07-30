// =============================================================================
// DiagnosticsController.h — Diagnostics page controller
//
// Forwards diagnostic execution state from AppState to QML as a proper MVC
// controller.  QML binds to these properties instead of reaching into AppState
// directly.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>

class AppState;

class DiagnosticsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int runStatus READ runStatus NOTIFY runStatusChanged)
    Q_PROPERTY(int totalCompleted READ totalCompleted NOTIFY progressChanged)
    Q_PROPERTY(QString currentDiagLabel READ currentDiagLabel NOTIFY currentDiagChanged)
    Q_PROPERTY(QString currentGroup READ currentGroup NOTIFY groupChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(bool cellularWarnVisible READ cellularWarnVisible NOTIFY cellularWarnChanged)

public:
    explicit DiagnosticsController(AppState* appState, QObject* parent = nullptr);

    Q_INVOKABLE void runDiagnostics();
    Q_INVOKABLE void cancel();

    int runStatus() const;
    int totalCompleted() const;
    QString currentDiagLabel() const;
    QString currentGroup() const;
    QString errorMessage() const;
    bool cellularWarnVisible() const;

signals:
    void runStatusChanged();
    void progressChanged();
    void currentDiagChanged();
    void groupChanged();
    void errorChanged();
    void cellularWarnChanged();

private:
    AppState* m_appState;
};
