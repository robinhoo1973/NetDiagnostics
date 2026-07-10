// =============================================================================
// QtConfigAdapter.h — Production Qt config adapter for the simulator.
//
// Reads the platform-specific qtquickcontrols2.conf files from
// resources/config/ and exposes their key/value pairs so the simulator can
// apply the same style, theme, palette, and font settings that the production
// app uses on that platform.
//
// Usage:
//   QtConfigAdapter adapter;
//   adapter.load(":/config/windows.conf");  // or ios.conf, linux.conf, etc.
//   QString style = adapter.value("Style");        // → "Fusion"
//   QString theme = adapter.value("Theme");        // → "Dark"
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariantMap>
#include <QFile>
#include <QTextStream>

class QtConfigAdapter : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString configPath READ configPath WRITE setConfigPath NOTIFY configChanged)
    Q_PROPERTY(QString style      READ style                        NOTIFY configChanged)
    Q_PROPERTY(QString theme      READ theme                        NOTIFY configChanged)
    Q_PROPERTY(QString background READ background                   NOTIFY configChanged)
    Q_PROPERTY(QString foreground READ foreground                   NOTIFY configChanged)
    Q_PROPERTY(QString accent     READ accent                       NOTIFY configChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily                   NOTIFY configChanged)

public:
    explicit QtConfigAdapter(QObject* parent = nullptr) : QObject(parent) {}

    QString configPath() const { return m_configPath; }
    void setConfigPath(const QString& path) {
        if (m_configPath != path) {
            m_configPath = path;
            load(path);
        }
    }

    // ── Convenience accessors ──────────────────────────────────────────
    QString style()      const { return value("Style"); }
    QString theme()      const { return value("Theme"); }
    QString background() const { return value("Background"); }
    QString foreground() const { return value("Foreground"); }
    QString accent()     const { return value("Accent"); }
    QString fontFamily() const { return value("FontFamily", "JetBrains Mono"); }

    // ── Generic access ──────────────────────────────────────────────────
    QString value(const QString& key, const QString& fallback = {}) const {
        return m_values.value(key, fallback);
    }

    bool load(const QString& path) {
        m_values.clear();
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        QTextStream ts(&f);
        QString currentSection;
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(';') || line.startsWith('#'))
                continue;
            if (line.startsWith('[') && line.endsWith(']')) {
                currentSection = line.mid(1, line.length() - 2);
                continue;
            }
            int eq = line.indexOf('=');
            if (eq > 0) {
                QString key = line.left(eq).trimmed();
                QString val = line.mid(eq + 1).trimmed();
                QString fullKey = currentSection.isEmpty()
                    ? key : currentSection + QStringLiteral("/") + key;
                m_values[fullKey] = val;
                // 5WHY: bare-key shortcut caused silent overwrites when
                // multiple sections define the same key name (e.g. [Style]/Background
                // vs [Theme]/Background). Only store section-qualified keys.
            }
        }
        m_configPath = path;
        emit configChanged();
        return true;
    }

signals:
    void configChanged();

private:
    QString            m_configPath;
    QMap<QString, QString> m_values;
};
