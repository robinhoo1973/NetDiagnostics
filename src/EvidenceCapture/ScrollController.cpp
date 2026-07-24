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
    m_flickable = flickable;
}

void ScrollController::scrollToBottom(int durationMs) {
    if (!m_flickable || durationMs <= 0) return;
    if (m_scrolling) cancel();

    // Read QML Flickable properties via QMetaObject
    QVariant contentH = m_flickable->property("contentHeight");
    QVariant height   = m_flickable->property("height");
    QVariant contentY = m_flickable->property("contentY");

    if (!contentH.isValid() || !height.isValid() || !contentY.isValid()) return;

    qreal maxY = qMax(0.0, contentH.toDouble() - height.toDouble());
    m_startY = contentY.toDouble();
    m_targetY = maxY;

    if (m_targetY <= m_startY) {
        // Already at bottom — nothing to scroll
        emit scrollFinished();
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
