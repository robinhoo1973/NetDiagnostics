// =============================================================================
// NavigationAdapter.cpp — C++ bridge to QML AppContent navigation
// =============================================================================
#include "EvidenceCapture/NavigationAdapter.h"
#include "app/AppState.h"
#include <QMetaObject>
#include <QVariant>
#include <QCoreApplication>
#include <QThread>

const QStringList NavigationAdapter::kPageNames = {
    QStringLiteral("dashboard"),
    QStringLiteral("diagnostic"),
    QStringLiteral("config"),
    QStringLiteral("settings")
};

QString NavigationAdapter::pageObjectName(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= kPageNames.size()) return {};
    return kPageNames.at(tabIndex);
}

NavigationAdapter::NavigationAdapter(QObject* appContent, AppState* appState, QObject* parent)
    : QObject(parent)
    , m_appContent(appContent)
    , m_appState(appState)
{
}

void NavigationAdapter::setAppContent(QObject* appContent) {
    m_appContent = appContent;
}

void NavigationAdapter::switchToTab(int index) {
    if (!m_appContent || index < 0 || index >= kPageNames.size()) {
        emit tabSwitchFailed(index);
        return;
    }

    QMetaObject::invokeMethod(m_appContent, "switchToTab",
                              Q_ARG(int, index));
}

void NavigationAdapter::waitForPageReady(int tabIndex, int timeoutMs,
                                          std::function<void(bool)> onReady) {
    if (!m_appContent || tabIndex < 0 || tabIndex >= kPageNames.size()) {
        if (onReady) onReady(false);
        return;
    }

    QString targetName = kPageNames.at(tabIndex);

    // Poll the StackView for the target page objectName
    // StackView is: m_appContent->stackView
    QObject* stackView = m_appContent->property("stackView").value<QObject*>();
    if (!stackView) {
        if (onReady) onReady(false);
        return;
    }

    // Shared state for the polling lambda
    auto elapsed = std::make_shared<int>(0);
    auto pollTimer = new QTimer(this);
    pollTimer->setInterval(200);

    connect(pollTimer, &QTimer::timeout, this, [=]() mutable {
        *elapsed += 200;

        // Check if the target page is the current item
        QObject* currentItem = stackView->property("currentItem").value<QObject*>();
        bool found = false;
        if (currentItem) {
            QString name = currentItem->property("objectName").toString();
            found = (name == targetName);
        }

        if (found) {
            pollTimer->stop();
            pollTimer->deleteLater();
            emit pageReady(tabIndex);
            if (onReady) onReady(true);
        } else if (*elapsed >= timeoutMs) {
            pollTimer->stop();
            pollTimer->deleteLater();
            emit tabSwitchFailed(tabIndex);
            if (onReady) onReady(false);
        }
    });

    pollTimer->start();
}

int NavigationAdapter::currentTabIndex() const {
    if (!m_appContent) return -1;
    QObject* stackView = m_appContent->property("stackView").value<QObject*>();
    if (!stackView) return -1;
    QObject* currentItem = stackView->property("currentItem").value<QObject*>();
    if (!currentItem) return -1;
    QString name = currentItem->property("objectName").toString();
    return kPageNames.indexOf(name);
}

void NavigationAdapter::openDiagnosticDetail(int diagIdInt) {
    if (!m_appContent) return;
    // 5WHY: m_appContent->property("appState") fails because appState is a
    // QML context property, not a QObject property on AppContent.  Similarly,
    // findChild<AppState*>() fails because AppState is not in AppContent's
    // QObject child hierarchy.  Use the direct m_appState pointer instead.
    if (!m_appState) return;

    // Find the DiagnosticScreen in the StackView
    QObject* stackView = m_appContent->property("stackView").value<QObject*>();
    if (!stackView) return;

    // Find the diagnostic page: check currentItem first (most common case),
    // then iterate all items via the StackView's internal list (limited access).
    QObject* diagScreen = nullptr;

    // Check currentItem — this covers 90% of cases
    QObject* cur = stackView->property("currentItem").value<QObject*>();
    if (cur && cur->property("objectName").toString() == QStringLiteral("diagnostic")) {
        diagScreen = cur;
    }

    // Fallback: iterate depth via contentChildren
    if (!diagScreen) {
        int depth = stackView->property("depth").toInt();
        QObjectList children = stackView->property("contentChildren").value<QObjectList>();
        for (auto* child : children) {
            if (child && child->property("objectName").toString() == QStringLiteral("diagnostic")) {
                diagScreen = child;
                break;
            }
        }
        Q_UNUSED(depth);
    }

    if (!diagScreen) return;

    QVariantMap detail = m_appState->getDetailResult(diagIdInt);
    if (detail.isEmpty()) return;

    // Set overlay properties on DiagnosticScreen
    QObject* overlay = diagScreen->property("detailOverlay").value<QObject*>();
    if (!overlay) return;

    // Populate detail fields
    QString displayName = detail.value("displayName").toString();
    int status = detail.value("status", 0).toInt();
    QStringList statusNames = {"Pass","Warning","Fail","Skipped","Error","Info"};
    QString statusStr = (status >= 0 && status < statusNames.size())
                        ? statusNames.at(status) : QStringLiteral("Unknown");
    qint64 dur = detail.value("durationMs", 0).toLongLong();
    QString summary = detail.value("summary").toString();
    QString details = detail.value("details").toString();

    // Set QML overlay properties via QMetaObject or property system
    QObject* titleLabel = overlay->findChild<QObject*>(QStringLiteral("dtTitle"));
    QObject* statusLabel = overlay->findChild<QObject*>(QStringLiteral("dtStatus"));
    QObject* summaryLabel = overlay->findChild<QObject*>(QStringLiteral("dtSummary"));
    QObject* outputLabel = overlay->findChild<QObject*>(QStringLiteral("dtOutput"));

    if (titleLabel)   titleLabel->setProperty("text", displayName);
    if (statusLabel)  statusLabel->setProperty("text",
        QStringLiteral("Status: %1    Duration: %2ms").arg(statusStr).arg(dur));
    if (summaryLabel) summaryLabel->setProperty("text", summary);
    if (outputLabel)  outputLabel->setProperty("text", details);

    // Show the overlay
    overlay->setProperty("visible", true);
    emit detailOpened();
}

void NavigationAdapter::openReportPreview() {
    if (!m_appContent) return;

    // 5WHY: m_appContent->findChild<AppState*>() fails — use direct pointer.
    if (!m_appState) return;

    // Build HTML and navigate to settings tab where report is accessible.
    QString html = m_appState->buildReportHtml(true, m_appState->isDarkMode());
    if (html.isEmpty()) return;

    QMetaObject::invokeMethod(m_appContent, "switchToTab",
                              Q_ARG(int, 3)); // switch to settings (report accessible from there)

    // 5WHY: The loop below iterated the StackView depth and did nothing with
    // each item.  Q_UNUSED(html) explicitly discarded the built report.
    // This was dead code that never showed a report preview.
    // TODO: Show the actual report preview overlay for automated capture.
    // Currently report capture is best-effort — the screenshot captures
    // the settings page, not the report content itself.
    emit diagnosticComplete(); // signal we're done (report capture is best-effort)
}
