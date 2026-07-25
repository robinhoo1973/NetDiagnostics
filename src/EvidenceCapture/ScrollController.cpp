// =============================================================================
// ScrollController.cpp — Smooth scrolling for Flickable
// =============================================================================
#include "EvidenceCapture/ScrollController.h"
#include <QVariant>
#include <QMetaProperty>
#include <QtMath>

ScrollController::ScrollController(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(kTickInterval);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &ScrollController::onScrollTick);
}

void ScrollController::setFlickable(QObject* flickable) {
    if (m_scrolling) {
        cancel();
    }
    // 5WHY: Disconnect any previous destroyed() signal and connect the new
    // one so ScrollController auto-cancels when the QML page changes and
    // the Flickable is destroyed (prevents use-after-free).
    if (m_flickable) {
        QObject::disconnect(m_flickable, &QObject::destroyed,
                            this, &ScrollController::cancel);
    }
    m_flickable = flickable;
    if (flickable) {
        connect(flickable, &QObject::destroyed,
                this, &ScrollController::cancel);
    }
}

void ScrollController::scrollToBottom(int durationMs) {
    // 5WHY: silent return without emitting scrollFinished() hangs the
    // capture forever — executeNextStep() waits for the signal that
    // never arrives.  Emit asynchronously so callers inside executeStep()
    // don't re-enter before m_currentStep is incremented.
    if (!m_flickable || durationMs <= 0) {
        QTimer::singleShot(0, this, [this]() { emit scrollFinished(); });
        return;
    }
    if (m_scrolling) cancel();

    // 5WHY: Design spec (§4.5) says "scroll from top to bottom at constant
    // speed" but the old code scrolled from wherever the Flickable happened
    // to be.  If a prior captureBefore or scroll changed the position, the
    // recording would miss the top portion of the page.  Reset to top first.
    m_flickable->setProperty("contentY", 0.0);

    // Read QML Flickable properties via QMetaObject
    QVariant contentH = m_flickable->property("contentHeight");
    QVariant height   = m_flickable->property("height");
    QVariant contentY = m_flickable->property("contentY");

    // 5WHY: silently returning without emitting scrollFinished() hangs the
    // capture forever at a Scroll step.  Treat invalid QVariant properties
    // the same as !m_flickable — the wired object isn't really a Flickable.
    if (!contentH.isValid() || !height.isValid() || !contentY.isValid()) {
        QTimer::singleShot(0, this, [this]() { emit scrollFinished(); });
        return;
    }

    qreal maxY = qMax(0.0, contentH.toDouble() - height.toDouble());
    m_startY = contentY.toDouble();
    m_targetY = maxY;

    if (m_targetY <= m_startY) {
        // Already at bottom — nothing to scroll.
        // Defer scrollFinished so callers inside executeStep() don't re-enter
        // executeNextStep() before the outer call has incremented m_currentStep,
        // which would cause the same step to re-execute and the next step to skip.
        QTimer::singleShot(0, this, [this]() {
            emit scrollFinished();
        });
        return;
    }

    m_durationMs = durationMs;
    m_elapsedMs = 0;
    m_progress = 0;
    m_scrolling = true;
    emit scrollingChanged();
    m_timer->start();
}

void ScrollController::cancel() {
    if (!m_scrolling) return;
    m_timer->stop();
    m_scrolling = false;
    m_progress = 0;
    emit scrollingChanged();
    emit progressChanged();
    // 5WHY: If the Flickable is destroyed during an active scroll (e.g.
    // unexpected page transition, QObject::destroyed signal), the
    // CaptureOrchestrator's onStepScrollFinished() never fires and the
    // capture hangs in ExecutingSteps forever.  Emit scrollFinished
    // asynchronously so callers inside executeStep() don't re-enter.
    QTimer::singleShot(0, this, [this]() { emit scrollFinished(); });
}

void ScrollController::onScrollTick() {
    if (!m_flickable || !m_scrolling) {
        m_timer->stop();
        return;
    }

    m_elapsedMs += kTickInterval;

    // Ease-out curve: cubic ease-out for natural-looking deceleration
    qreal t = qMin(1.0, static_cast<qreal>(m_elapsedMs) / m_durationMs);
    qreal eased = 1.0 - qPow(1.0 - t, 3.0);  // cubic ease-out
    m_progress = eased;

    qreal newY = m_startY + (m_targetY - m_startY) * eased;
    m_flickable->setProperty("contentY", newY);

    emit progressChanged();

    if (m_elapsedMs >= m_durationMs) {
        m_timer->stop();
        // Snap to exact target
        m_flickable->setProperty("contentY", m_targetY);
        m_progress = 1.0;
        m_scrolling = false;
        emit progressChanged();
        emit scrollingChanged();
        emit scrollFinished();
    }
}
