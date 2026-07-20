// =============================================================================
// GeoProbe.h — Geographic Probe Engine (facade)
//
// Composes four internal components:
//   ProbeScheduler — submit & merge requests
//   ProbeDatabase  — thread-safe task table
//   ProbeExecutor  — persistent probe worker thread
//   ProbeFeedback  — statistics & aggregation
//
// Public API: probe(), getFeedback(), server database, region tags.
// =============================================================================
#pragma once

#include "Diagnostics/Model/ProbeConfig.h"
#include <QObject>
#include <QVector>
#include <QString>
#include <QMap>

class ProbeDatabase;
class ProbeScheduler;
class ProbeExecutor;
class ProbeFeedback;

// Speed-test server entry (global server database)
struct ProbeServer {
    QString host; int port = 80;
    QString name, sponsor, country, url;
};

class GeoProbe {
public:
    static GeoProbe& instance();

    // Submit probe request — non-blocking
    void probe(const ProbeConfig& config);

    // Block until results ready, then compute statistics, aggregate, return
    ProbeResult getFeedback(const ProbeConfig& config);

    // Clear all cached probe results (call at start of each diagnostic run)
    void clear();

    // ── Server database (static immutable data) ──
    static QVector<ProbeServer> allServers();
    static QVector<ProbeServer> serversForCountry(const QString& hint);

    // ── Region tag mapping (static immutable data) ──
    static QStringList regionTags(const QString& countryCode);

private:
    GeoProbe();
    ~GeoProbe();
    GeoProbe(const GeoProbe&) = delete;
    GeoProbe& operator=(const GeoProbe&) = delete;

    static void ensureServerDbLoaded();

    ProbeDatabase*  m_database  = nullptr;
    ProbeScheduler* m_scheduler = nullptr;
    ProbeExecutor*  m_executor  = nullptr;
    ProbeFeedback*  m_feedback  = nullptr;
};
