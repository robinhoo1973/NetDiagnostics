// =============================================================================
// NavigationAdapter.h — C++ bridge to QML AppContent navigation
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §4.4
//
// Wraps QMetaObject::invokeMethod calls to AppContent.switchToTab() so the
// C++ CaptureOrchestrator can drive page navigation without QML coupling.
// =============================================================================
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>

class AppState;
class QTimer;

class NavigationAdapter : public QObject {
    Q_OBJECT

public:
    explicit NavigationAdapter(QObject* appContent, AppState* appState, QObject* parent = nullptr);

    // Inject AppContent QML object reference (can be updated at runtime).
    void setAppContent(QObject* appContent);

    // Switch to tab: 0=dashboard, 1=diagnostic, 2=config, 3=settings.
    // Returns immediately; call waitForPageReady() to synchronize.
    Q_INVOKABLE void switchToTab(int index);

    // Block until the target page objectName is found in the StackView,
    // or timeoutMs elapses.  Calls onReady callback when done.
    // Uses an event-loop-friendly polling approach (not nested exec).
    Q_INVOKABLE void waitForPageReady(int tabIndex, int timeoutMs,
                                       std::function<void(bool ok)> onReady);

    // Open the diagnostic detail overlay for a given DiagId.
    Q_INVOKABLE void openDiagnosticDetail(int diagIdInt);

    // Open report preview.  Emits reportPreviewReady(bool ok) when the
    // async operation completes (success or failure), or never if the
    // initial switchToTab fails synchronously (in which case the caller's
    // timeout should advance the scenario).
    Q_INVOKABLE void openReportPreview();

    // Get the current tab index (-1 if unknown).
    Q_INVOKABLE int currentTabIndex() const;

    // Page object names indexed by tab.
    static QString pageObjectName(int tabIndex);

signals:
    void pageReady(int tabIndex);
    void tabSwitchFailed(int tabIndex);
    void detailOpened();
    void reportPreviewReady(bool ok);

private:
    void doOpenReportPreview(); // deferred body of openReportPreview (500ms timer)
    QPointer<QObject> m_appContent = nullptr;
    AppState* m_appState = nullptr;
    QPointer<QTimer> m_pageReadyTimer = nullptr;  // active waitForPageReady poll timer

    static const QStringList kPageNames; // ["dashboard","diagnostic","config","settings"]
};
