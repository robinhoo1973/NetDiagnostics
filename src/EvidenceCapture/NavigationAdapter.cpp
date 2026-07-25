// =============================================================================
// NavigationAdapter.cpp — C++ bridge to QML AppContent navigation
// =============================================================================
#include "EvidenceCapture/NavigationAdapter.h"
#include "app/AppState.h"
#include <QMetaObject>
#include <QVariant>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
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

    bool invoked = QMetaObject::invokeMethod(m_appContent, "switchToTab",
                               Q_ARG(int, index));
    // 5WHY: invokeMethod return was unchecked — if a QML refactoring
    // renames/removes switchToTab, capture silently proceeds with wrong
    // page.  Log and emit failure so the caller can react.
    if (!invoked) {
        qWarning() << "NavigationAdapter: switchToTab not invocable";
        emit tabSwitchFailed(index);
        return;
    }
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
    // 5WHY: Use QPointer so the polling lambda auto-detects StackView
    // destruction (app teardown, QML engine reset).  A raw QObject* would
    // dangle silently — stackView->property() on freed memory is UB.
    QPointer<QObject> stackView = m_appContent->property("stackView").value<QObject*>();
    if (!stackView) {
        if (onReady) onReady(false);
        return;
    }

    // Shared state for the polling lambda
    auto elapsed = std::make_shared<int>(0);
    // 5WHY: stop any prior poll timer before creating a new one — if
    // waitForPageReady is called again before the previous timer completes
    // (unlikely but possible with rapid re-entry), both timers would
    // poll concurrently and each emit pageReady, advancing steps twice.
    if (m_pageReadyTimer) {
        m_pageReadyTimer->stop();
        m_pageReadyTimer->deleteLater();
    }
    auto pollTimer = new QTimer(this);
    m_pageReadyTimer = pollTimer;
    pollTimer->setInterval(200);

    // 5WHY: Explicit capture list — [=] in a member function implicitly
    // captures this, hiding the lifecycle dependency.  stackView is QPointer
    // so it auto-nulls if the StackView is destroyed before the timer fires.
    connect(pollTimer, &QTimer::timeout, this,
            [this, elapsed, targetName, onReady, stackView]() mutable {
        // 5WHY: Guard against StackView destruction during polling.
        // QPointer auto-nulls; bail out rather than dereferencing nullptr.
        if (!stackView) {
            m_pageReadyTimer->stop();
            m_pageReadyTimer->deleteLater();
            m_pageReadyTimer = nullptr;
            if (onReady) onReady(false);
            return;
        }
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
            m_pageReadyTimer = nullptr;
            emit pageReady(tabIndex);
            if (onReady) onReady(true);
        } else if (*elapsed >= timeoutMs) {
            pollTimer->stop();
            pollTimer->deleteLater();
            m_pageReadyTimer = nullptr;
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

    // 5WHY: was using findChild by objectName string to poke QML widget
    // internals — brittle coupling to DiagnosticScreen's private structure.
    // Now calls the well-known showDetailOverlay() QML function instead.
    // The DiagnosticScreen owns the mapping from data fields to its widgets.
    bool invoked = QMetaObject::invokeMethod(diagScreen, "showDetailOverlay",
                              Q_ARG(QVariant, QVariant::fromValue(detail)));
    if (!invoked) {
        qWarning() << "NavigationAdapter: showDetailOverlay not invocable on DiagnosticScreen";
        return;
    }
    // 5WHY: showDetailOverlay() only populates label texts, not the
    // currentDetail property that drives the properties Repeater grid.
    // Without this, the detail overlay shows an empty properties section
    // when opened from automated capture (the QML path sets currentDetail
    // separately via page.currentDetail = d || {}).
    diagScreen->setProperty("currentDetail", QVariant::fromValue(detail));
    emit detailOpened();
}

void NavigationAdapter::openReportPreview() {
    if (!m_appContent) return;
    if (!m_appState) return;

    // 5WHY: The old code switched to tab 3 (settings) and captured that page
    // instead of the actual report preview.  The dashboard tab (index 0) has
    // a built-in preview overlay controlled by `previewVisible` and
    // `openPreview()`.  Switch to dashboard, call openPreview() on the
    // dashboard QML item, then let the scenario's Capture step screenshot it.

    // Switch to dashboard tab where the report preview overlay lives
    bool switched = QMetaObject::invokeMethod(m_appContent, "switchToTab",
                                              Q_ARG(int, 0)); // dashboard tab
    if (!switched) {
        qWarning() << "NavigationAdapter: openReportPreview switchToTab(0) not invocable";
        emit reportPreviewReady(false);
        return;
    }

    // 5WHY: StackView.pop/push operations are asynchronous — the
    // transition animation completes on the next frame.  Accessing
    // currentItem synchronously after switchToTab returns the OLD
    // item (or the wrong page).  Defer openPreview() by 500ms so
    // the StackView has time to settle and currentItem reflects the
    // dashboard page.
    QTimer::singleShot(500, this, &NavigationAdapter::doOpenReportPreview);
}

void NavigationAdapter::doOpenReportPreview() {
    // 5WHY: openReportPreview() defers by 500ms for the StackView transition
    // to settle.  m_appContent (now QPointer) may have been destroyed in that
    // window (e.g. QML engine teardown during app exit).  Guard the deref.
    if (!m_appContent) {
        emit reportPreviewReady(false);
        return;
    }
    bool ok = false;
    QObject* stackView = m_appContent->property("stackView").value<QObject*>();
    if (stackView) {
        QObject* cur = stackView->property("currentItem").value<QObject*>();
        if (cur && cur->property("objectName").toString() == QStringLiteral("dashboard")) {
            ok = QMetaObject::invokeMethod(cur, "openPreview");
            if (!ok) {
                qWarning() << "NavigationAdapter: openPreview not invocable on DashboardScreen";
            }
        } else {
            qWarning() << "NavigationAdapter: openReportPreview dashboard not current after StackView settle";
        }
    } else {
        qWarning() << "NavigationAdapter: openReportPreview cannot access stackView (async)";
    }
    emit reportPreviewReady(ok);
}
