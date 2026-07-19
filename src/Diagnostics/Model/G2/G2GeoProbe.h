// =============================================================================
// G2GeoProbe.h — Geographic Probe Engine
//
// Multi-threaded TTFB-based geographic location detection.  Probes all
// speed-test servers in the database, groups results by country, uses
// Hodges-Lehmann robust estimation to find the lowest-latency country
// (physical location), then selects the best server within that country
// for upload/download speed testing via repeated measurement + CI.
// =============================================================================
#pragma once
#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GBase.h"
#include "Diagnostics/Model/GHelpers.h"
#include <QString>
#include <QVector>
#include <QMap>
#include <QElapsedTimer>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <cmath>

namespace G1G2G3Native {

class GeoProbe {
public:
    // ── Per-server probe result ───────────────────────────────────
    struct ServerResult {
        QString host; int port; QString country;
        QString sponsor; QString name;
        double tcpMs = -1.0;      // TCP connect RTT
        double ttfbMs = -1.0;     // HTTP time-to-first-byte
        bool   ok = false;        // true if TTFB succeeded
    };

    // ── Per-country aggregate ─────────────────────────────────────
    struct CountryResult {
        QString code;
        double hlMs = 0.0;        // Hodges-Lehmann estimate (ms)
        int    serverCount = 0;   // number of successful servers
        QVector<ServerResult> servers; // raw per-server results
    };

    // ── Best-server selection result ──────────────────────────────
    struct BestServer {
        QString host; int port;
        QString sponsor; QString country;
        double ttfbMs = 0.0;      // median TTFB across rounds
        double ttfbCI = 0.0;      // half-width of 95% CI (ms)
        int    rounds = 0;        // successful test rounds
        bool   valid = false;
    };

    // ── Full probe result ────────────────────────────────────────
    struct Result {
        QVector<CountryResult> countries;  // sorted by hlMs ascending
        QString physicalCountry;           // lowest-TTFB country
        BestServer bestServer;             // for speed test
        int totalServers; int totalOk;
        double durationSec;
    };

    // ── Public API ────────────────────────────────────────────────
    GeoProbe();

    // Run full probe: TTFB all servers → rank countries → pick best server
    Result probe(int maxTimeSec = 45);

    // Quick probe: TTFB only servers in a specific country, pick best
    BestServer pickBestInCountry(const QString& country, int rounds = 5);

private:
    SpeedTest m_speedTest;

    // ── Internal helpers ──────────────────────────────────────────
    QVector<ServerResult> probeAllServers(
        const QVector<SpeedTest::Server>& targets, int maxTimeSec);

    QVector<CountryResult> aggregateByCountry(
        const QVector<ServerResult>& results);

    BestServer selectBestServer(
        const QVector<ServerResult>& countryServers, int rounds);

    static double hodgesLehmann(const QVector<double>& v);
};

// ── Standalone diagnostics built on GeoProbe ─────────────────────
// internetConnectivity: finds physical country + best speed test server
DiagnosticResult internetConnectivity(DiagId id);

} // namespace G1G2G3Native
