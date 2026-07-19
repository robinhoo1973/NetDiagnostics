// =============================================================================
// G2GeoProbe.h — Geographic Probe Engine
//
// Multi-threaded TTFB-based geographic location detection.  Probes all
// speed-test servers in the database, groups results by country and region,
// uses Hodges-Lehmann robust estimation to find the lowest-latency
// country/region (physical location), then selects the best server for
// upload/download speed testing via repeated measurement + CI.
// =============================================================================
#pragma once
#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GBase.h"
#include "Diagnostics/Model/GHelpers.h"
#include <QString>
#include <QVector>
#include <QMap>
#include <QStringList>
#include <QElapsedTimer>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <cmath>

namespace G1G2G3Native {

class GeoProbe {
public:
    // ── Region tags ────────────────────────────────────────────────
    // Standardized geographic aggregation labels.
    // Each server gets a chain: Continent → Sub-region → Economic zone
    // e.g. "Asia", "Asia/East Asia", "Asia/East Asia/Greater China"
    static QStringList regionTags(const QString& countryCode);

    // ── Per-server probe result ───────────────────────────────────
    struct ServerResult {
        QString host; int port; QString country;
        QString sponsor; QString name;
        QStringList regions;       // auto-assigned region tags
        double tcpMs = -1.0;
        double ttfbMs = -1.0;
        bool   ok = false;
    };

    // ── Per-country aggregate ─────────────────────────────────────
    struct CountryResult {
        QString code;
        double hlMs = 0.0;
        int    serverCount = 0;
        QVector<ServerResult> servers;
    };

    // ── Per-region aggregate ──────────────────────────────────────
    struct RegionResult {
        QString tag;               // e.g. "Asia/East Asia"
        double hlMs = 0.0;
        int    serverCount = 0;
        int    countryCount = 0;
        QVector<ServerResult> servers;
    };

    // ── Best-server selection ─────────────────────────────────────
    struct BestServer {
        QString host; int port;
        QString sponsor; QString country; QStringList regions;
        double ttfbMs = 0.0;      // median TTFB across rounds
        double ttfbCI = 0.0;      // 95% CI half-width (ms)
        int    rounds = 0;
        bool   valid = false;
    };

    // ── Full probe result ────────────────────────────────────────
    struct Result {
        QVector<CountryResult> countries;   // sorted by hlMs
        QVector<RegionResult> regions;      // sorted by hlMs
        QString physicalCountry;
        BestServer bestServer;
        int totalServers; int totalOk;
        double durationSec;
    };

    // ── Public API ────────────────────────────────────────────────
    GeoProbe();

    // Full probe: all servers → by country + by region → best server
    Result probe(int maxTimeSec = 45);

    // Pick best server in a country, with repeated testing + CI
    BestServer pickBestInCountry(const QString& country, int rounds = 5);

    // Pick best server in a region (e.g. "Asia" or "Asia/East Asia")
    BestServer pickBestInRegion(const QString& regionTag, int rounds = 5);

private:
    SpeedTest m_speedTest;

    QVector<ServerResult> probeAllServers(
        const QVector<SpeedTest::Server>& targets, int maxTimeSec);

    QVector<CountryResult> aggregateByCountry(
        const QVector<ServerResult>& results);

    QVector<RegionResult> aggregateByRegion(
        const QVector<ServerResult>& results);

    BestServer selectBestServer(
        const QVector<ServerResult>& candidates, int rounds);
};

// ── Standalone diagnostics ────────────────────────────────────────
DiagnosticResult internetConnectivity(DiagId id);

} // namespace G1G2G3Native
