// =============================================================================
// Logger.cpp
// =============================================================================
#include "Common/Utils/Logger.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QStandardPaths>

Logger::Logger() {
    QString logDir;
#if defined(Q_OS_WIN)
    logDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/NetDiagnostics";
#else
    logDir = QStringLiteral("/tmp/NetDiagnostics");
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
    QTextStream ts(&m_file);
    ts << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
       << " [" << level << "] " << msg << "\n";
    ts.flush();
}

void Logger::info(const QString& msg)  { write("INFO", msg); }
void Logger::event(const QString& msg) { write("EVENT", msg); }
void Logger::error(const QString& msg) { write("ERROR", msg); }
void Logger::warn(const QString& msg)  { write("WARN", msg); }
