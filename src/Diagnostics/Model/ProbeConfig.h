// =============================================================================
// ProbeConfig.h — Probe configuration and result types
// =============================================================================
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// ── Probe configuration ──────────────────────────────────────────────
struct ProbeConfig {
    enum Scope { Global, ByCountry, ByRegion, ByServers };
    Scope scope = Global;
    QString scopeValue;           // country code or region tag
    QStringList serverHosts;      // explicit "host:port" list for ByServers

    int rounds = 3;               // TTFB measurements per server

    enum class Aggregation { None, ByCountry, ByRegion };
    Aggregation aggregation = Aggregation::ByCountry;

    int topN = 0;                 // 0 = return all, N = top N only
};

// ── Per-server probe result ──────────────────────────────────────────
struct ServerResult {
    QString host; int port = 80;
    QString country; QStringList regionTags;
    double ttfbMs = -1.0;        // Hodges-Lehmann median
    double ciHalf = 0.0;         // 95% CI half-width (ms)
    double mad = 0.0;            // Median Absolute Deviation
    int rounds = 0;              // actual measurement count
    bool ok = false;
};

// ── Per-country aggregate ────────────────────────────────────────────
struct CountryResult {
    QString code;
    double hlMs = 0.0;
    int serverCount = 0;
    QVector<ServerResult> servers;
};

// ── Per-region aggregate ─────────────────────────────────────────────
struct RegionResult {
    QString tag;                  // e.g. "Asia/East Asia"
    double hlMs = 0.0;
    int serverCount = 0;
    int countryCount = 0;
    QVector<ServerResult> servers;
};

// ── Full probe result ────────────────────────────────────────────────
struct ProbeResult {
    QVector<ServerResult>   servers;
    QVector<CountryResult>  countries;
    QVector<RegionResult>   regions;
    QString                 physicalCountry;
};
