// =============================================================================
// ScrollController.h — Smooth scrolling for Flickable during recording capture
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §4.5
//
// Drives a QML Flickable from its current position to the bottom at a
// controlled speed, emitting progress signals so the orchestrator can
// capture mid-scroll screenshots.
// =============================================================================
#pragma once

#include <QObject>
#include <QTimer>

class ScrollController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scrolling READ isScrolling NOTIFY scrollingChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

public:
    explicit ScrollController(QObject* parent = nullptr);

    bool isScrolling() const { return m_scrolling; }
    qreal progress() const { return m_progress; }

    // Set the target Flickable. Must be called before scrollToBottom().
    // The flickable must have contentHeight and contentY properties.
    Q_INVOKABLE void setFlickable(QObject* flickable);

    // Scroll from current position to bottom over durationMs.
    // Emits scrollFinished() when done.  Does nothing if no flickable set.
    Q_INVOKABLE void scrollToBottom(int durationMs = 3000);

    // Stop scrolling immediately.
    Q_INVOKABLE void cancel();

signals:
    void scrollingChanged();
    void progressChanged();
    void scrollFinished();

private slots:
    void onScrollTick();

private:
    QObject* m_flickable = nullptr;
    QTimer* m_timer;
    qreal m_startY = 0;
    qreal m_targetY = 0;
    qreal m_progress = 0;
    int m_durationMs = 0;
    int m_elapsedMs = 0;
    bool m_scrolling = false;
    static constexpr int kTickInterval = 33; // ~30 fps
};
