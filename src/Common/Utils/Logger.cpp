// =============================================================================
// Logger.cpp
// =============================================================================
#include "Common/Utils/Logger.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QStandardPaths>

#if defined(PLATFORM_ANDROID)
#include "Common/Platform/Android/AndroidLogPaths.h"
#endif

Logger::Logger() {
    QString logDir;
#if defined(PLATFORM_IOS)
    // 5WHY: /tmp on iOS is inside the sandbox and invisible to the user.
    // Route the runtime log to Documents so it is retrievable via Files.app
    // (requires UIFileSharingEnabled + LSSupportsOpeningDocumentsInPlace).
    logDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/NetDiagnostics";
#else
#if defined(PLATFORM_ANDROID)
    // 5WHY (Android b21294): /tmp is not writable on Android, and the private
    // cache dir is invisible to the user.  Use the app-scoped external dir
    // (Android/data/<pkg>/files, no permission needed, USB/MTP-visible).
    logDir = androidUserVisibleLogDir();
#else
#if defined(Q_OS_WIN)
    logDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/NetDiagnostics";
#else
    logDir = QStringLiteral("/tmp/NetDiagnostics");
#endif
#endif
#endif
    QDir().mkpath(logDir);
    m_file.setFileName(logDir + "/debug.log");
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "NetDiagnostics: Cannot open log file:" << m_file.fileName()
                   << "-" << m_file.errorString();
    }
}

Logger::~Logger() {
    if (m_file.isOpen())
        m_file.close();
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::write(const QString& level, const QString& msg) {
    QMutexLocker lock(&m_mutex);
    if (!m_file.isOpen()) return;
    // 5WHY: debug.log was Append-only with no rotation — a long session or a
    // crash loop could grow it to hundreds of MB on storage-constrained
    // devices. Rotate at 1MB: keep one previous file (debug.log.1) for
    // post-mortem, then start a fresh debug.log. Runs under m_mutex so no
    // writer races the rename.
    if (m_file.size() > 1024 * 1024) {
        m_file.close();
        QFile::remove(m_file.fileName() + QLatin1String(".1"));
        QFile::rename(m_file.fileName(), m_file.fileName() + QLatin1String(".1"));
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "NetDiagnostics: Cannot reopen log file after rotation:" << m_file.fileName();
        }
    }
    QTextStream ts(&m_file);
    ts << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
       << " [" << level << "] " << msg << "\n";
    ts.flush();
}

void Logger::info(const QString& msg)  { write("INFO", msg); }
void Logger::event(const QString& msg) { write("EVENT", msg); }
void Logger::error(const QString& msg) { write("ERROR", msg); }
void Logger::warn(const QString& msg)  { write("WARN", msg); }
