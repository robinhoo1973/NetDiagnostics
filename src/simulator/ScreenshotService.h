// =============================================================================
// ScreenshotService.h — Phase 3: Viewport-only capture, auto-capture, evidence.
//
// Captures the DeviceViewport region (cropped from the window grab) and saves
// structured filenames.  Provides auto-capture hooks wired to diagnostic
// lifecycle events (test-start, test-end, on-failure).  Recording stubs are
// present for future ffmpeg / platform-encoder backing.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QVariantList>
#include <QVariantMap>

class ScreenshotService : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString     outputDir    READ outputDir    WRITE setOutputDir    NOTIFY outputDirChanged)
    Q_PROPERTY(bool        recording    READ recording                         NOTIFY recordingChanged)
    Q_PROPERTY(QString     lastCapturePath READ lastCapturePath                NOTIFY captureTaken)
    Q_PROPERTY(QVariantList evidenceLog READ evidenceLog                       NOTIFY evidenceChanged)
    Q_PROPERTY(bool        autoCaptureOnFailure READ autoCaptureOnFailure
                           WRITE setAutoCaptureOnFailure NOTIFY autoCaptureChanged)

public:
    explicit ScreenshotService(QObject* parent = nullptr) : QObject(parent) {}

    QString outputDir() const { return m_outputDir; }
    void setOutputDir(const QString& d) {
        if (m_outputDir != d) { m_outputDir = d; emit outputDirChanged(); }
    }
    bool recording() const { return m_recording; }
    QString lastCapturePath() const { return m_lastCapturePath; }
    QVariantList evidenceLog() const { return m_evidenceLog; }

    bool autoCaptureOnFailure() const { return m_autoCaptureOnFailure; }
    void setAutoCaptureOnFailure(bool v) {
        if (m_autoCaptureOnFailure != v) { m_autoCaptureOnFailure = v; emit autoCaptureChanged(); }
    }

    // ── Viewport binding ─────────────────────────────────────────────────
    Q_INVOKABLE void setViewport(QQuickItem* item) { m_viewport = item; }

    // ── Manual screenshot (Phase 3: viewport-cropped) ────────────────────
    Q_INVOKABLE QString capture(const QString& filename = {},
                                 const QString& testName = {},
                                 const QString& trigger = QStringLiteral("manual")) {
        QQuickWindow* win = m_viewport ? m_viewport->window() : nullptr;
        if (!win) {
            emit captureFailed(QStringLiteral("No viewport/window available"));
            return {};
        }
        QImage full = win->grabWindow();
        if (full.isNull()) {
            emit captureFailed(QStringLiteral("grabWindow returned null"));
            return {};
        }
        // Phase 3: crop to viewport bounds for device-only capture.
        // 5WHY: mapToScene returns logical (device-independent) coords while
        // grabWindow returns device pixels. On HiDPI/Retina displays, multiply
        // by devicePixelRatio to align the two coordinate spaces.
        QImage img = full;
        if (m_viewport) {
            qreal dpr = win->devicePixelRatio();
            QPointF vpOrigin = m_viewport->mapToScene(QPointF(0, 0));
            int x = qMax(0, qRound(vpOrigin.x() * dpr));
            int y = qMax(0, qRound(vpOrigin.y() * dpr));
            int w = qMin(qRound(m_viewport->width()  * dpr), full.width()  - x);
            int h = qMin(qRound(m_viewport->height() * dpr), full.height() - y);
            if (w > 0 && h > 0)
                img = full.copy(x, y, w, h);
        }

        QString outName = filename;
        if (outName.isEmpty()) {
            outName = QStringLiteral("screenshot_%1.png")
                .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        }
        QString outPath = ensureDir() + QStringLiteral("/") + outName;
        if (!img.save(outPath)) {
            emit captureFailed(QStringLiteral("Image save failed: ") + outPath);
            return {};
        }
        m_lastCapturePath = outPath;

        // Record in evidence log
        QVariantMap entry;
        entry["path"]    = outPath;
        entry["time"]    = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        entry["test"]    = testName;
        entry["trigger"] = trigger;
        m_evidenceLog.prepend(entry);
        if (m_evidenceLog.size() > 100) m_evidenceLog.removeLast(); // cap
        emit evidenceChanged();
        emit captureTaken(outPath);
        return outPath;
    }

    // ── Auto-capture hooks (wired from QML or C++) ──────────────────────
    // Call these from diagnostic lifecycle signals:
    //   onDiagCompleted → captureForTest(id, "end")
    //   onRunStatusChanged → if Completed → final capture
    Q_INVOKABLE QString captureForTest(int diagId, const QString& testName,
                                        const QString& os, const QString& device,
                                        const QString& trigger = QStringLiteral("auto")) {
        QString fname = makeFilename(os, device, testName);
        return capture(fname, testName, trigger);
    }

    Q_INVOKABLE QString captureOnFailure(int diagId, const QString& testName,
                                          const QString& os, const QString& device) {
        if (!m_autoCaptureOnFailure) return {};
        return captureForTest(diagId, testName, os, device, QStringLiteral("failure"));
    }

    // ── Structured filename helpers ─────────────────────────────────────
    Q_INVOKABLE QString makeFilename(const QString& os, const QString& device,
                                      const QString& testName) {
        return QStringLiteral("screenshot_%1_%2_%3_%4.png")
            .arg(os, device, testName,
                 QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    }

    // ── Recording stubs (backed by external encoder in future phases) ───
    Q_INVOKABLE void startRecording()  { m_recording = true;  emit recordingChanged(); }
    Q_INVOKABLE void stopRecording()   { m_recording = false; emit recordingChanged(); }
    Q_INVOKABLE void pauseRecording()  { /* future */ }

signals:
    void captureTaken(const QString& path);
    void captureFailed(const QString& reason);
    void outputDirChanged();
    void recordingChanged();
    void evidenceChanged();
    void autoCaptureChanged();

private:
    QString ensureDir() {
        QString d = m_outputDir.isEmpty()
            ? QDir::currentPath() + QStringLiteral("/evidence")
            : m_outputDir;
        QDir().mkpath(d);
        return d;
    }

    QPointer<QQuickItem> m_viewport;
    QString     m_outputDir;
    QString     m_lastCapturePath;
    QVariantList m_evidenceLog;
    bool        m_recording = false;
    bool        m_autoCaptureOnFailure = true;
};
