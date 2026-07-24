// =============================================================================
// CaptureService.cpp — Automated screenshot capture during diagnostics
// =============================================================================
#include "EvidenceCapture/CaptureService.h"
#include "Common/Model/CaptureFeatureGate.h"
#include "Common/Platform/PlatformCapture.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

CaptureService::CaptureService(QObject* parent)
    : QObject(parent) {}

QString CaptureService::captureRoot() const {
    // 5WHY: co-locate with crash reports under Evidence/ so all diagnostic
    // evidence (crashes + captures) is in one discoverable location.
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/Evidence/CaptureService");
}

QString CaptureService::nextSeq() const {
    return QStringLiteral("%1").arg(m_captureCount, 2, 10, QLatin1Char('0'));
}

void CaptureService::startSession(const QString& label) {
    // Gate check: only start if the hidden feature is enabled
    if (!CaptureFeatureGate::isFeatureEnabled()) return;
    if (m_active) return;

    QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    m_sessionDir = captureRoot() + QStringLiteral("/") + ts;
    QDir().mkpath(m_sessionDir);

    m_active = true;
    m_captureCount = 0;
    m_statusText = QStringLiteral("📸 Capture session started");

    emit activeChanged();
    emit sessionDirChanged();
    emit statusTextChanged();

    // Session log
    QFile log(m_sessionDir + QStringLiteral("/capture.log"));
    if (log.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream logStream(&log);
        logStream << "CaptureService session: " << ts << "\n"
                  << "Started: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n"
                  << "Label: " << (label.isEmpty() ? "diagnostic run" : label) << "\n\n";
    }

    // Take initial screenshot
    capture(label.isEmpty() ? QStringLiteral("00_session_start") : label);
}

void CaptureService::capture(const QString& label) {
    if (!m_active) return;
    if (m_sessionDir.isEmpty()) return;

    QString filename = nextSeq() + QStringLiteral("_") + label + QStringLiteral(".png");
    QString filePath = m_sessionDir + QStringLiteral("/") + filename;

    bool ok = platformCaptureScreenshot(filePath);
    if (ok) {
        m_captureCount++;
        m_statusText = QStringLiteral("📸 Captured: ") + label;
        emit captureCountChanged();
        emit statusTextChanged();
        emit captured(filePath);

        // Append to session log
        QFile log(m_sessionDir + QStringLiteral("/capture.log"));
        if (log.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream ts(&log);
            ts << "[" << QDateTime::currentDateTime().toString(Qt::ISODate)
               << "] " << filename << " — " << (ok ? "OK" : "FAIL")
               << " (" << QFileInfo(filePath).size() << " bytes)\n";
        }
    } else {
        m_statusText = QStringLiteral("⚠ Capture failed: ") + label;
        emit statusTextChanged();
    }
}

void CaptureService::endSession() {
    if (!m_active) return;

    capture(QStringLiteral("99_session_end"));

    // Write session summary
    QFile log(m_sessionDir + QStringLiteral("/capture.log"));
    if (log.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&log);
        ts << "\n--- Session complete ---\n"
           << "Total captures: " << m_captureCount << "\n"
           << "Directory: " << m_sessionDir << "\n";
    }

    m_statusText = QStringLiteral("📸 Session complete: %1 captures → %2")
        .arg(m_captureCount).arg(m_sessionDir);
    m_active = false;

    emit activeChanged();
    emit statusTextChanged();
}

void CaptureService::cancelSession() {
    if (!m_active) return;

    // Remove all captures from this session
    QDir dir(m_sessionDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }

    m_active = false;
    m_captureCount = 0;
    m_sessionDir.clear();
    m_statusText.clear();

    emit activeChanged();
    emit sessionDirChanged();
    emit captureCountChanged();
    emit statusTextChanged();
}
