// =============================================================================
// PlatformRecording.cpp — Desktop recording via ffmpeg
// =============================================================================
#include "Common/Platform/PlatformRecording.h"
#include <QProcess>
#include <QGuiApplication>
#include <QScreen>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QTimer>
#include <atomic>

// ═════════════════════════════════════════════════════════════════════════════
// Desktop implementation: ffmpeg screen capture
// ═════════════════════════════════════════════════════════════════════════════

static QProcess*   s_recordingProc   = nullptr;
static QString     s_recordingPath;
static std::atomic<bool> s_recording{false};
static RecordingCallback s_startCallback;

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

void platformStartRecording(const QString& filePath, RecordingCallback callback) {
    if (s_recording.load()) {
        if (callback) callback(false, QStringLiteral("Recording already in progress"));
        return;
    }

    // Determine screen geometry
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        if (callback) callback(false, QStringLiteral("No screen available"));
        return;
    }
    QSize size = screen->size();
    QString videoSize = QStringLiteral("%1x%2").arg(size.width()).arg(size.height());

    // Build output path with .mp4 extension
    QString outPath = filePath;
    if (!outPath.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive)) {
        outPath += QStringLiteral(".mp4");
    }
    QDir().mkpath(QFileInfo(outPath).absolutePath());

    s_recordingPath = outPath;
    s_startCallback = callback;

    // Build ffmpeg command
    QStringList args;
#if defined(__linux__)
    // Linux: x11grab
    QString display = qEnvironmentVariable("DISPLAY", ":0.0");
    args << QStringLiteral("-f") << QStringLiteral("x11grab")
         << QStringLiteral("-video_size") << videoSize
         << QStringLiteral("-framerate") << QStringLiteral("15")
         << QStringLiteral("-i") << display
         << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("ultrafast")
         << QStringLiteral("-crf") << QStringLiteral("23")
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
#elif defined(__APPLE__)
    // macOS: avfoundation
    args << QStringLiteral("-f") << QStringLiteral("avfoundation")
         << QStringLiteral("-video_size") << videoSize
         << QStringLiteral("-framerate") << QStringLiteral("15")
         << QStringLiteral("-i") << QStringLiteral("1")  // screen device index
         << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("ultrafast")
         << QStringLiteral("-crf") << QStringLiteral("23")
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
#elif defined(_WIN32)
    // Windows: gdigrab
    args << QStringLiteral("-f") << QStringLiteral("gdigrab")
         << QStringLiteral("-video_size") << videoSize
         << QStringLiteral("-framerate") << QStringLiteral("15")
         << QStringLiteral("-i") << QStringLiteral("desktop")
         << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("ultrafast")
         << QStringLiteral("-crf") << QStringLiteral("23")
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
#endif

    // Prevent ffmpeg from prompting for overwrite
    args << QStringLiteral("-y");
    args << outPath;

    s_recordingProc = new QProcess();
    s_recordingProc->setProcessChannelMode(QProcess::MergedChannels);

    // Detect start: ffmpeg prints "Press [q] to stop" when ready
    QObject::connect(s_recordingProc, &QProcess::readyReadStandardOutput, [=]() {
        QByteArray data = s_recordingProc->readAllStandardOutput();
        if (data.contains("Press [q] to stop") || data.contains("frame=")) {
            if (!s_recording.load()) {
                s_recording = true;
                if (s_startCallback) {
                    s_startCallback(true, s_recordingPath);
                    s_startCallback = nullptr;
                }
            }
        }
    });

    // Handle errors
    QObject::connect(s_recordingProc, &QProcess::errorOccurred, [=](QProcess::ProcessError err) {
        Q_UNUSED(err);
        if (!s_recording.load()) {
            QString errMsg = s_recordingProc->errorString();
            s_recordingProc->deleteLater();
            s_recordingProc = nullptr;
            if (s_startCallback) {
                s_startCallback(false, errMsg);
                s_startCallback = nullptr;
            }
        }
    });

    // Handle process exit
    QObject::connect(s_recordingProc,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [=](int exitCode, QProcess::ExitStatus status) {
            Q_UNUSED(exitCode);
            Q_UNUSED(status);
            s_recording = false;
            s_recordingProc->deleteLater();
            s_recordingProc = nullptr;
        });

    s_recordingProc->start(QStringLiteral("ffmpeg"), args);

    // Fallback: if ffmpeg doesn't start after 5s, treat as started anyway
    // (ffmpeg might not print the "Press [q]" message on all versions)
    QTimer::singleShot(5000, [=]() {
        if (!s_recording.load() && s_recordingProc
            && s_recordingProc->state() == QProcess::Running) {
            s_recording = true;
            if (s_startCallback) {
                s_startCallback(true, s_recordingPath);
                s_startCallback = nullptr;
            }
        } else if (!s_recording.load() && s_startCallback) {
            s_startCallback(false, QStringLiteral("ffmpeg failed to start. Is ffmpeg installed?"));
            s_startCallback = nullptr;
            if (s_recordingProc) {
                s_recordingProc->kill();
                s_recordingProc->deleteLater();
                s_recordingProc = nullptr;
            }
        }
    });
}

void platformStopRecording(RecordingCallback callback) {
    if (!s_recording.load() || !s_recordingProc) {
        if (callback) callback(false, QStringLiteral("No recording in progress"));
        return;
    }

    QString path = s_recordingPath;

    // Send 'q' to ffmpeg to gracefully stop
    s_recordingProc->write("q");
    s_recordingProc->closeWriteChannel();

    // Wait up to 15s for ffmpeg to finalize
    QTimer::singleShot(15000, [=]() {
        if (s_recording.load()) {
            // Force kill if still running
            if (s_recordingProc) {
                s_recordingProc->kill();
            }
            s_recording = false;
            if (callback) callback(false, QStringLiteral("Recording stop timed out"));
        }
    });

    // When process finishes, report success
    QObject::connect(s_recordingProc,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [=](int exitCode, QProcess::ExitStatus status) {
            Q_UNUSED(exitCode);
            Q_UNUSED(status);
            s_recording = false;
            // Verify file exists and has content
            QFileInfo fi(path);
            if (fi.exists() && fi.size() > 0) {
                if (callback) callback(true, path);
            } else {
                if (callback) callback(false, QStringLiteral("Recording file is empty or missing"));
            }
        });
}

bool platformIsRecording() {
    return s_recording.load();
}

bool platformSupportsScreenshotWhileRecording() {
    return true; // Desktop: separate ffmpeg process, screenshot via QScreen is independent
}

#else
// Stub for unsupported platforms
void platformStartRecording(const QString&, RecordingCallback callback) {
    if (callback) callback(false, QStringLiteral("Screen recording not supported on this platform"));
}
void platformStopRecording(RecordingCallback callback) {
    if (callback) callback(false, QStringLiteral("Screen recording not supported on this platform"));
}
bool platformIsRecording() { return false; }
bool platformSupportsScreenshotWhileRecording() { return false; }
#endif
