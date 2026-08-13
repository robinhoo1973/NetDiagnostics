// =============================================================================
// Logger.cpp
// =============================================================================
#include "Common/Services/Logger.h"

#include <QDateTime>
#include <QDebug>

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

void Logger::event(const QString& message)  { write(QStringLiteral("EVENT"), message); }
void Logger::warn(const QString& message)   { write(QStringLiteral("WARN"),  message); }
void Logger::error(const QString& message)  { write(QStringLiteral("ERROR"), message); }

void Logger::write(const QString& level, const QString& message) {
    const QString line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                       + QStringLiteral(" [") + level + QStringLiteral("] ") + message;
    qInfo().noquote() << line;
}
