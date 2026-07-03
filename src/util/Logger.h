// =============================================================================
// Logger.h — File-based sequential debug logger
//
// Writes to /tmp/NetDiagnostics/debug.log (Unix) or
// %TEMP%/NetDiagnostics/debug.log (Windows).
// =============================================================================
#pragma once

#include <QString>
#include <QFile>
#include <QMutex>

class Logger {
public:
    static Logger& instance();

    void info(const QString& msg);
    void event(const QString& msg);
    void error(const QString& msg);
    void warn(const QString& msg);

private:
    Logger();
    ~Logger();
    void write(const QString& level, const QString& msg);

    QFile m_file;
    QMutex m_mutex;
};
