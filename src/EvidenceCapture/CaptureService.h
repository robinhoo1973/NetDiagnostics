// =============================================================================
// CaptureService.h — Automated screenshot capture during diagnostics
// =============================================================================
// Activated by: double-click app icon in Settings > About (CaptureFeatureGate).
//
// When enabled, automatically captures screenshots at key diagnostic moments:
//   1. Diagnostic run started
//   2. Each group (G1-G5) completed
//   3. All diagnostics complete
//
// Screenshots are saved to: <AppData>/CaptureService/<yyyyMMdd-HHmmss>/
//
// Design ref: review/06_Capture_Architecture_Design.md
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>

class CaptureService : public QObject {
    Q_OBJECT
    // Whether an active capture session is running
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    // Current session directory (empty if no active session)
    Q_PROPERTY(QString sessionDir READ sessionDir NOTIFY sessionDirChanged)
    // Number of screenshots taken in the current session
    Q_PROPERTY(int captureCount READ captureCount NOTIFY captureCountChanged)
    // Human-readable status for QML indicator
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit CaptureService(QObject* parent = nullptr);

    bool isActive() const { return m_active; }
    QString sessionDir() const { return m_sessionDir; }
    int captureCount() const { return m_captureCount; }
    QString statusText() const { return m_statusText; }

    // Start a new capture session. Creates a timestamped directory.
    // Does nothing if already active or if feature gate is disabled.
    Q_INVOKABLE void startSession(const QString& label = QString());

    // Take a screenshot with the given label (e.g. "G1_complete").
    // Saves as: <sessionDir>/<seq>_<label>.png
    // Does nothing if no active session.
    Q_INVOKABLE void capture(const QString& label);

    // End the current session. Final screenshot + writes capture.log.
    Q_INVOKABLE void endSession();

    // Cancel the session — removes all captures from this session.
    Q_INVOKABLE void cancelSession();

signals:
    void activeChanged();
    void sessionDirChanged();
    void captureCountChanged();
    void statusTextChanged();
    void captured(const QString& filePath);

private:
    QString captureRoot() const;
    QString nextSeq() const;

    bool m_active = false;
    QString m_sessionDir;
    int m_captureCount = 0;
    QString m_statusText;
};
