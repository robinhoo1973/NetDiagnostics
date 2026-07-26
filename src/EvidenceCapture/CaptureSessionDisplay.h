// =============================================================================
// CaptureSessionDisplay.h — Display-formatted capture session status
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §4.1 (Q_PROPERTY supplement)
//
// Architecture (Display Model pattern):
//   This lightweight QObject mirrors key CaptureOrchestrator state as
//   display-formatted Q_PROPERTYs.  All string formatting (pad2, pad0,
//   HH:MM:SS) lives in C++ — the QML overlay binds declaratively with
//   zero JavaScript logic, zero Component.onCompleted seeding, and zero
//   Timer polling.
//
// 5WHY: The old QML overlay used Component.onCompleted + Connections +
// a 1-second Timer to seed and update labels.  This created a fragile
// dual-initialization path (Component.onCompleted uses currentStep without
// +1, Connections.onStepChanged uses current+1) that depended on C++
// signal emission ordering.  The Display Model eliminates this entirely:
// one formatting path in C++, one declarative binding in QML.
// =============================================================================
#pragma once

#include <QObject>
#include <QTimer>
#include <QString>

class CaptureOrchestrator;

class CaptureSessionDisplay : public QObject {
    Q_OBJECT

    // ── Display-formatted strings (bound declaratively by QML) ──────
    Q_PROPERTY(QString stepDisplay    READ stepDisplay    NOTIFY stepDisplayChanged)
    Q_PROPERTY(QString totalDisplay   READ totalDisplay   NOTIFY totalDisplayChanged)
    Q_PROPERTY(QString countDisplay   READ countDisplay   NOTIFY countDisplayChanged)
    Q_PROPERTY(QString elapsedDisplay READ elapsedDisplay NOTIFY elapsedDisplayChanged)

    // ── Visibility flags (derived from session mode) ────────────────
    Q_PROPERTY(bool showRecordingDot    READ showRecordingDot    NOTIFY visibilityFlagsChanged)
    Q_PROPERTY(bool showScreenshotGroup READ showScreenshotGroup NOTIFY visibilityFlagsChanged)

public:
    explicit CaptureSessionDisplay(CaptureOrchestrator* orchestrator,
                                   QObject* parent = nullptr);

    // ── Accessors ───────────────────────────────────────────────────
    QString stepDisplay() const    { return m_stepDisplay; }
    QString totalDisplay() const   { return m_totalDisplay; }
    QString countDisplay() const   { return m_countDisplay; }
    QString elapsedDisplay() const { return m_elapsedDisplay; }
    bool showRecordingDot() const  { return m_showRecordingDot; }
    bool showScreenshotGroup() const { return m_showScreenshotGroup; }

signals:
    void stepDisplayChanged();
    void totalDisplayChanged();
    void countDisplayChanged();
    void elapsedDisplayChanged();
    void visibilityFlagsChanged();

private slots:
    void onStepChanged(int current, int total);
    void onCaptureCountChanged(int count);
    void onCaptureModeChanged();
    void onElapsedTick();

private:
    // ── Numeric formatting helpers (used for display strings) ────────
    static QString pad0(int n) { return (n < 10 ? QLatin1String("0") : QString()) + QString::number(n); }
    static QString pad2(int n) { return (n < 10 ? QLatin1String(" ") : QString()) + QString::number(n); }

    void updateVisibility();

    CaptureOrchestrator* m_orchestrator = nullptr;
    QTimer* m_elapsedTimer = nullptr;

    QString m_stepDisplay    = QStringLiteral(" 0");
    QString m_totalDisplay   = QStringLiteral(" 0");
    QString m_countDisplay   = QStringLiteral(" 0");
    QString m_elapsedDisplay = QStringLiteral("00:00:00");
    bool m_showRecordingDot    = false;
    bool m_showScreenshotGroup = true;
};
