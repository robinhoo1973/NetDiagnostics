// =============================================================================
// DashboardController.h — Dashboard page controller
//
// Provides stats, group info, and result queries for the dashboard UI.
// Accesses AppState for shared diagnostic results.
// =============================================================================
#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class AppState;

class DashboardController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int totalCompleted READ totalCompleted NOTIFY progressChanged)
    Q_PROPERTY(int totalDiags READ totalDiags NOTIFY progressChanged)
    Q_PROPERTY(QVariantList allGroupStats READ allGroupStats NOTIFY progressChanged)
    Q_PROPERTY(QStringList groupLabels READ groupLabels CONSTANT)

public:
    explicit DashboardController(AppState* appState, QObject* parent = nullptr);

    int totalCompleted() const;
    int totalDiags() const;
    QVariantList allGroupStats() const;
    QStringList groupLabels() const;

    Q_INVOKABLE QVariantList resultsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList allDiagsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList allDiagIdsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList visibleGroups() const;
    Q_INVOKABLE QVariantMap groupStats(int groupInt) const;
    Q_INVOKABLE QVariantMap getDetailResult(int diagIdInt) const;
    Q_INVOKABLE void showDetailDialog(int diagIdInt);

signals:
    void progressChanged();

private:
    AppState* m_appState;
};
