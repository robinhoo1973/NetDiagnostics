// =============================================================================
// NavigationAdapter.cpp — C++ bridge to QML AppContent navigation
// =============================================================================
#include "EvidenceCapture/NavigationAdapter.h"
#include "app/AppState.h"
#include <QMetaObject>
#include <QVariant>
#include <QDebug>
#include <QTimer>
#include <QQmlComponent>

const QStringList NavigationAdapter::kPageNames = {
    QStringLiteral("dashboard"),
    QStringLiteral("diagnostic"),
    QStringLiteral("config"),
    QStringLiteral("settings")
};

// 5WHY: After switching tabs, the page may retain its previous scroll
// position — a diagnostic run scrolled to the bottom, then navigating back
// to Dashboard would capture the footer instead of the header.  Recursively
// find the first Flickable (has contentY+contentHeight) in the page tree
// and reset it to the top so every screenshot starts from a consistent origin.
// 5WHY: Use property().isValid() rather than metaObject()->indexOfProperty().
// Qt 6's dynamic property system may bypass static meta-object registration
// for QML-instantiated types on iOS — indexOfProperty returns -1 for a valid
// dynamic property, while property() correctly reads it.
static void scrollPageToTop(QObject* page) {
    if (!page) return;

    // Check if this object has Flickable-like scroll properties
    QVariant cy = page->property("contentY");
    QVariant ch = page->property("contentHeight");
    if (cy.isValid() && ch.isValid()) {
        page->setProperty("contentY", 0.0);
        return;
    }

    // Recurse into children
    const QObjectList kids = page->children();
    for (QObject* kid : kids) {
        if (kid) scrollPageToTop(kid);
    }
}

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
    // 5WHY: Silent failure when m_appContent is null (wiring not yet done or
    // findChild("appContent") failed in main.cpp) — the capture scenario
    // proceeds with no tab switch, producing screenshots of the wrong page.
    // Log the failure so a developer investigating a silent capture mis-routing
    // can instantly identify the missing wiring as the root cause.
    if (!m_appContent) {
        qWarning() << "NavigationAdapter: switchToTab(" << index
                    << ") aborted — m_appContent is null (wiring failed?)";
        emit tabSwitchFailed(index);
        return;
    }
    if (index < 0 || index >= kPageNames.size()) {
        qWarning() << "NavigationAdapter: switchToTab(" << index
                    << ") aborted — index out of range [0," << kPageNames.size() - 1 << "]";
        emit tabSwitchFailed(index);
        return;
    }

    // 5WHY: QMetaObject::invokeMethod(m_appContent, "switchToTab", ...)
    // calls a QML-defined JavaScript function via the Qt meta-object system.
    // On iOS static builds the meta-method registration for QML functions
    // may differ from desktop dynamic builds — invokeMethod returns false
    // and the tab switch silently fails.  Bypass the QML function entirely:
    // directly manipulate the StackView from C++ using the same logic as
    // AppContent.switchToTab().
    QObject* stackView = m_appContent->property("stackView").value<QObject*>();
    if (!stackView) {
        qWarning() << "NavigationAdapter: switchToTab(" << index
                    << ") aborted — cannot access stackView from m_appContent";
        emit tabSwitchFailed(index);
        return;
    }

    const QString targetName = kPageNames.at(index);

    // Step 1: Check if target page already exists in the stack.
    // The QML switchToTab iterates stackView.depth and calls stackView.get(i).
    // The C++ equivalent uses the contentChildren property.
    QObjectList children = stackView->property("contentChildren").value<QObjectList>();
    for (auto* child : children) {
        if (child && child->property("objectName").toString() == targetName) {
            // Page exists — pop to it (makes it the top item).
            bool ok = QMetaObject::invokeMethod(stackView, "pop",
                                  Q_ARG(QVariant, QVariant::fromValue(child)));
            if (!ok) {
                qWarning() << "NavigationAdapter: pop to" << targetName << "failed";
                emit tabSwitchFailed(index);
                return;
            }
            qInfo() << "NavigationAdapter: popped to existing page" << targetName;
            scrollPageToTop(child);
            return;
        }
    }

    // Step 2: Page not found in stack — create a new instance and push it.
    // Access the tabComponents array (defined in AppContent.qml) to get
    // the pre-declared QQmlComponent for this tab.
    QVariantList components = m_appContent->property("tabComponents").toList();
    if (index >= components.size()) {
        qWarning() << "NavigationAdapter: tabComponents index" << index
                    << "out of range (size=" << components.size() << ")";
        emit tabSwitchFailed(index);
        return;
    }

    QQmlComponent* comp = qvariant_cast<QQmlComponent*>(components.at(index));
    if (!comp) {
        qWarning() << "NavigationAdapter: tabComponents[" << index
                    << "] is not a QQmlComponent";
        emit tabSwitchFailed(index);
        return;
    }

    QObject* page = comp->create();
    if (!page) {
        qWarning() << "NavigationAdapter: failed to create page for tab" << index
                    << "—" << comp->errorString();
        emit tabSwitchFailed(index);
        return;
    }

    page->setProperty("objectName", targetName);

    bool ok = QMetaObject::invokeMethod(stackView, "push",
                          Q_ARG(QVariant, QVariant::fromValue(page)));
    if (!ok) {
        qWarning() << "NavigationAdapter: push to" << targetName << "failed";
        delete page;
        emit tabSwitchFailed(index);
        return;
    }

    qInfo() << "NavigationAdapter: pushed new page" << targetName;
    scrollPageToTop(page);
}

void NavigationAdapter::waitForPageReady(int tabIndex, int timeoutMs,
                                          std::function<void(bool)> onReady) {
    if (!m_appContent) {
        qWarning() << "NavigationAdapter: waitForPageReady(" << tabIndex
                    << ") aborted — m_appContent is null";
        if (onReady) onReady(false);
        return;
    }
    if (tabIndex < 0 || tabIndex >= kPageNames.size()) {
        qWarning() << "NavigationAdapter: waitForPageReady(" << tabIndex
                    << ") aborted — index out of range";
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
        qWarning() << "NavigationAdapter: waitForPageReady(" << tabIndex
                    << ") aborted — stackView property null on m_appContent";
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
            [this, elapsed, targetName, onReady, stackView, pollTimer, tabIndex, timeoutMs]() mutable {
        // 5WHY: Guard against StackView destruction during polling.
        // QPointer auto-nulls; bail out rather than dereferencing nullptr.
        if (!stackView) {
            pollTimer->stop();
            pollTimer->deleteLater();
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

// 5WHY: cancel() in CaptureOrchestrator stops m_delayTimer and m_pollTimer,
// but NOT m_pageReadyTimer in NavigationAdapter — the poll timer kept
// firing at 200ms intervals, accessing stackView properties across the
// QML↔C++ boundary, until its natural 3s timeout.  On a cancelled session
// this is wasted work.  This method gives cancel() a clean stop point.
void NavigationAdapter::stopPageReadyPolling() {
    if (m_pageReadyTimer) {
        m_pageReadyTimer->stop();
        m_pageReadyTimer->deleteLater();
        m_pageReadyTimer = nullptr;
    }
}

void NavigationAdapter::openDiagnosticDetail(int diagIdInt) {
    // 5WHY: All silent-return paths below left zero diagnostic info on iOS.
    // If the capture scenario requests OpenDetail and the diagnostic page
    // isn't the current item, none of the five return paths log anything —
    // the developer has no way to know why the detail never opened.
    if (!m_appContent) {
        qWarning() << "NavigationAdapter: openDiagnosticDetail(" << diagIdInt
                    << ") aborted — m_appContent is null";
        return;
    }
    // 5WHY: m_appContent->property("appState") fails because appState is a
    // QML context property, not a QObject property on AppContent.  Similarly,
    // findChild<AppState*>() fails because AppState is not in AppContent's
    // QObject child hierarchy.  Use the direct m_appState pointer instead.
    if (!m_appState) {
        qWarning() << "NavigationAdapter: openDiagnosticDetail(" << diagIdInt
                    << ") aborted — m_appState is null";
        return;
    }

    // Find the DiagnosticScreen in the StackView
    QObject* stackView = m_appContent->property("stackView").value<QObject*>();
    if (!stackView) {
        qWarning() << "NavigationAdapter: openDiagnosticDetail(" << diagIdInt
                    << ") aborted — stackView property not found on m_appContent";
        return;
    }

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

    if (!diagScreen) {
        qWarning() << "NavigationAdapter: openDiagnosticDetail(" << diagIdInt
                    << ") aborted — DiagnosticScreen not found in StackView";
        return;
    }

    // 5WHY: When multiple OpenDetail steps run sequentially, the previous
    // overlay may still be visible.  showDetailOverlay() toggles/updates
    // the overlay content but calling it while an overlay is already open
    // may cause QML state confusion (stale data, visual flicker).  Close
    // any existing overlay first so each OpenDetail starts from a clean
    // diagnostic screen state.
    // 5WHY: Check the return value — if dismissDetailOverlay doesn't exist
    // (mismatched QML/C++ deployment), the subsequent showDetailOverlay
    // may repopulate a stale overlay instead of refreshing from scratch.
    if (!QMetaObject::invokeMethod(diagScreen, "dismissDetailOverlay")) {
        qWarning() << "NavigationAdapter: dismissDetailOverlay not invocable — "
                       "overlay may carry stale data from previous detail";
    }

    QVariantMap detail = m_appState->getDetailResult(diagIdInt);
    if (detail.isEmpty()) {
        qWarning() << "NavigationAdapter: openDiagnosticDetail(" << diagIdInt
                    << ") aborted — getDetailResult returned empty (diag not yet run?)";
        return;
    }

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
    if (!m_appContent) {
        qWarning() << "NavigationAdapter: openReportPreview aborted — m_appContent is null";
        return;
    }
    if (!m_appState) {
        qWarning() << "NavigationAdapter: openReportPreview aborted — m_appState is null";
        return;
    }

    // 5WHY: The old code switched to tab 3 (settings) and captured that page
    // instead of the actual report preview.  The dashboard tab (index 0) has
    // a built-in preview overlay controlled by `previewVisible` and
    // `openPreview()`.  Switch to dashboard, call openPreview() on the
    // dashboard QML item, then let the scenario's Capture step screenshot it.

    // Switch to dashboard tab where the report preview overlay lives.
    // 5WHY: Use our own switchToTab(0) which directly manipulates the
    // StackView via C++ — bypasses the QML AppContent.switchToTab() function
    // whose QMetaObject::invokeMethod fails on iOS static builds.
    switchToTab(0);

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
