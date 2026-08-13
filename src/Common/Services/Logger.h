// =============================================================================
// Logger.h — Minimal structured logger (execution layer)
// =============================================================================
#pragma once

#include <QString>

class Logger {
public:
    static Logger& instance();

    void event(const QString& message);
    void warn(const QString& message);
    void error(const QString& message);

private:
    Logger() = default;
    void write(const QString& level, const QString& message);
};
