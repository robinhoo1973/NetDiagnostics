// =============================================================================
// Logger.cpp
// =============================================================================
#include "Common/Services/Logger.h"
#include "Common/Utils/TargetRedaction.h"

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
    // 日志脱敏：任何含 user:pass@ 的 target 以 *** 写入（TargetRedaction 恢复）
    const QString safe = TargetRedaction::forDisplay(message);
    const QString line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                       + QStringLiteral(" [") + level + QStringLiteral("] ") + safe;
    qInfo().noquote() << line;
}
