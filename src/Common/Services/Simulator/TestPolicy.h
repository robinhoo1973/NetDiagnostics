// =============================================================================
// TestPolicy.h — Per-platform test-skip policy for the simulator.
//
// Maps a simulated platform to a list of test-skip rules.  Each rule names
// a diagnostic test (by DiagId integer) and a human-readable reason.  The
// PlatformSimulationPolicyEngine evaluates these rules before every test run
// so that the simulator honours the *simulated* platform's capabilities even
// when the *host* platform could execute the test.
// =============================================================================
#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QHash>
#include <optional>

// ── DiagId string↔enum stable mapping ──────────────────────────────────
// 5WHY: Integer diagId values in JSON silently become wrong when the C++
// enum changes ordering. String keys ("G2TcpSettings") are stable across
// builds because they match the enum constant names.
inline std::optional<int> diagIdFromString(const QString& name) {
    static const QHash<QString, int> map = {
        {"G1NetworkAdapters",   0}, {"G1NicAdvanced",       1},
        {"G1WifiDiagnostics",   2}, {"G1WiredDiagnostics",  3},
        {"G1DhcpStatus",        4}, {"G1IpConfiguration",   5},
        {"G1ActiveConnections", 6}, {"G1CellularInfo",      7},
        {"G2NetworkProfile",    8}, {"G2TcpSettings",       9},
        {"G2DefaultGateway",   10}, {"G2RoutingTable",     11},
        {"G2ArpTable",         12}, {"G2ProxySettings",    13},
        {"G3NetskopeStatus",   14}, {"G3DnsServers",       15},
        {"G3DnsCache",         16}, {"G3DnsPollution",     17},
        {"G3InternetSpeedTest",18}, {"G4DnsResolution",    19},
        {"G4Ping",             20}, {"G4Traceroute",       21},
        {"G4PathPing",         22}, {"G4MtuDiscovery",     23},
        {"G5UrlParsing",       24}, {"G5TcpConnect",       25},
        {"G5ServiceBanner",    26}, {"G5CurlVerbose",      27},
        {"G5HttpHeaders",      28}, {"G5SecurityHeaders",  29},
        {"G5SslCertificate",   30}, {"G5HttpRedirect",     31},
        {"G5HttpCompression",  32}, {"G5HttpTiming",       33},
        {"G5FtpDiagnostics",   34}, {"G5SshDiagnostics",   35},
        {"G5EmailDiagnostics", 36}, {"G5Telnet",           37},
        {"G5Mysql",            38}, {"G5Postgres",         39},
        {"G5Redis",            40}, {"G5Mongodb",          41},
        {"G5Ldap",             42}, {"G5Mqtt",             43}
    };
    auto it = map.find(name);
    return it != map.end() ? std::optional<int>(*it) : std::nullopt;
}

struct SkipRule {
    int     diagId = -1;
    QString testName;
    QString reason;
};

struct TestPolicy {
    QString simulatedPlatform;
    QList<SkipRule> skipRules;

    // Fast lookup: diagId → reason (empty = not skipped)
    QHash<int, QString> skipReasonMap;

    void buildIndex() {
        skipReasonMap.clear();
        for (const auto& r : skipRules) {
            if (r.diagId >= 0)
                skipReasonMap[r.diagId] = r.reason;
        }
    }

    bool isValid() const { return !simulatedPlatform.isEmpty(); }

    QJsonObject toJson() const {
        QJsonObject o;
        o["simulatedPlatform"] = simulatedPlatform;
        QJsonArray arr;
        for (const auto& r : skipRules) {
            QJsonObject ro;
            ro["diagId"]   = r.diagId;
            ro["testName"] = r.testName;
            ro["reason"]   = r.reason;
            arr.append(ro);
        }
        o["skipRules"] = arr;
        return o;
    }

    static TestPolicy fromJson(const QJsonObject& o) {
        TestPolicy p;
        p.simulatedPlatform = o.value("simulatedPlatform").toString();
        for (const auto& v : o.value("skipRules").toArray()) {
            QJsonObject ro = v.toObject();
            SkipRule r;
            QJsonValue diagVal = ro.value("diagId");
            if (diagVal.isString()) {
                // 5WHY: String keys ("G2TcpSettings") are stable across
                // enum reorderings, unlike integer literals.
                auto opt = diagIdFromString(diagVal.toString());
                r.diagId = opt.value_or(-1);
            } else {
                // Backward-compat: legacy integer format (deprecated)
                r.diagId = diagVal.toInt(-1);
            }
            r.testName = ro.value("testName").toString();
            r.reason   = ro.value("reason").toString();
            if (r.diagId >= 0)
                p.skipRules.append(r);
        }
        p.buildIndex();
        return p;
    }

    static TestPolicy load(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        return fromJson(QJsonDocument::fromJson(f.readAll()).object());
    }
};
