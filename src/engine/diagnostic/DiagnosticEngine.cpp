// =============================================================================
// DiagnosticEngine.cpp — Pure C++ dispatch, no native plugin abstraction
// =============================================================================
#include "engine/diagnostic/DiagnosticEngine.h"
#include "engine/diagnostic/G1G2G3Native.h"
#include "engine/diagnostic/G4RemoteHost.h"
#include "engine/runner/NetworkProbe.h"
#include "util/DebugSwitch.h"
#ifndef NO_CURL
#include "engine/diagnostic/G5WebsiteUrl.h"
#endif
#include "util/Logger.h"
#include "util/PingParser.h"
#include <QtConcurrent/QtConcurrent>

DiagnosticEngine::DiagnosticEngine(QObject* parent)
    : QObject(parent) {}

DiagnosticEngine::~DiagnosticEngine() {
    m_destroying.store(true, std::memory_order_release);
}

QFuture<DiagnosticResult> DiagnosticEngine::runDiag(DiagId id, const QString& target,
                                                       int fromPort, int toPort, bool useCommonPorts) {
    auto* engine = this;
    return QtConcurrent::run([engine, id, target, fromPort, toPort, useCommonPorts]() -> DiagnosticResult {
        TRACE(" runDiag lambda id=%d ENTER\n", (int)id);
        if (engine->m_destroying.load(std::memory_order_acquire))
            return DiagnosticResult::error(id, QStringLiteral("Engine shutting down"));
        DiagGroup group = diagGroup(id);

        switch (group) {
            case DiagGroup::G1: return engine->runG1(id);
            case DiagGroup::G2: return engine->runG2(id);
            case DiagGroup::G3: return engine->runG3(id);
            case DiagGroup::G4: return engine->runG4(id, target, fromPort, toPort, useCommonPorts);
            case DiagGroup::G5:
#ifndef NO_CURL
                return engine->runG5(id, target);
#else
                return DiagnosticResult::skipped(id, QStringLiteral("G5 Website/URL tests unavailable (no curl on iOS)"));
#endif
        }
        return DiagnosticResult::error(id, QStringLiteral("Unknown group"));
    });
}

DiagnosticResult DiagnosticEngine::runDiagSync(DiagId id, const QString& target,
                                                 int fromPort, int toPort, bool useCommonPorts) {
    if (m_destroying.load(std::memory_order_acquire))
        return DiagnosticResult::error(id, QStringLiteral("Engine shutting down"));
    DiagGroup group = diagGroup(id);
    switch (group) {
        case DiagGroup::G1: return runG1(id);
        case DiagGroup::G2: return runG2(id);
        case DiagGroup::G3: return runG3(id);
        case DiagGroup::G4: return runG4(id, target, fromPort, toPort, useCommonPorts);
        case DiagGroup::G5:
#ifndef NO_CURL
            return runG5(id, target);
#else
            return DiagnosticResult::skipped(id, QStringLiteral("G5 Website/URL tests unavailable (no curl on iOS)"));
#endif
    }
    return DiagnosticResult::error(id, QStringLiteral("Unknown group"));
}

// ── G1: System & Adapters ──────────────────────────────────────────────────

DiagnosticResult DiagnosticEngine::runG1(DiagId id) {
    switch (id) {
        case DiagId::G1NetworkAdapters:   return G1G2G3Native::networkAdapters(id);
        case DiagId::G1NicAdvanced:       return G1G2G3Native::nicAdvanced(id);
        case DiagId::G1WifiDiagnostics:   return G1G2G3Native::wifiDiagnostics(id);
        case DiagId::G1WiredDiagnostics:  return G1G2G3Native::wiredDiagnostics(id);
        case DiagId::G1DhcpStatus:        return G1G2G3Native::dhcpStatus(id);
        case DiagId::G1IpConfiguration:   return G1G2G3Native::ipConfiguration(id);
        case DiagId::G1ActiveConnections: return G1G2G3Native::activeConnections(id);
        case DiagId::G1CellularInfo:      return G1G2G3Native::cellularInfo(id);
        default:
            return DiagnosticResult::skipped(id, QStringLiteral("Unknown G1 test"));
    }
}

// ── G2: Connectivity & Security ────────────────────────────────────────────

DiagnosticResult DiagnosticEngine::runG2(DiagId id) {
    switch (id) {
        case DiagId::G2NetworkProfile:    return G1G2G3Native::networkProfile(id);
        case DiagId::G2TcpSettings:       return G1G2G3Native::tcpSettings(id);
        case DiagId::G2DefaultGateway:    return G1G2G3Native::defaultGateway(id);
        case DiagId::G2RoutingTable:      return G1G2G3Native::routingTable(id);
        case DiagId::G2ArpTable:          return G1G2G3Native::arpTable(id);
        case DiagId::G2ProxySettings:     return G1G2G3Native::proxySettings(id);
        default:
            return DiagnosticResult::skipped(id, QStringLiteral("Unknown G2 test"));
    }
}

// ── G3: Internet & DNS ─────────────────────────────────────────────────────

DiagnosticResult DiagnosticEngine::runG3(DiagId id) {
    switch (id) {
        case DiagId::G3NetskopeStatus:        return G1G2G3Native::netskopeStatus(id);
        case DiagId::G3DnsServers:            return G1G2G3Native::dnsServers(id);
        case DiagId::G3DnsCache:              return G1G2G3Native::dnsCache(id);
        case DiagId::G3DnsPollution:          return G1G2G3Native::dnsPollution(id);
        case DiagId::G3InternetSpeedTest:     return G1G2G3Native::speedTest(id);
        default:
            return DiagnosticResult::skipped(id, QStringLiteral("Unknown G3 test"));
    }
}

// ── G4: Remote Host ────────────────────────────────────────────────────────

DiagnosticResult DiagnosticEngine::runG4(DiagId id, const QString& target,
                                            int fromPort, int toPort, bool useCommonPorts) {
    switch (id) {
        case DiagId::G4DnsResolution: return G4RemoteHost::dnsResolution(target);
        case DiagId::G4Ping:          return G4RemoteHost::ping(target);
        case DiagId::G4Traceroute:    return G4RemoteHost::traceroute(target);
        case DiagId::G4PathPing:      return G4RemoteHost::pathPing(target);
        case DiagId::G4MtuDiscovery:  return G4RemoteHost::mtuDiscovery(target);
        case DiagId::G4PortScan: {
            // Build port list
            QVector<int> portsToScan;
            if (useCommonPorts)
                portsToScan = NetworkProbe::commonDiagnosticPorts();
            if (fromPort > 0 && toPort >= fromPort) {
                fromPort = qBound(1, fromPort, 65535);
                toPort = qBound(fromPort, toPort, 65535);
                for (int p = fromPort; p <= toPort; ++p)
                    if (!portsToScan.contains(p))
                        portsToScan.append(p);
            }
            std::sort(portsToScan.begin(), portsToScan.end());

            QString scanHost = G4RemoteHost::extractHostname(target);

            QElapsedTimer t; t.start();
            auto results = NetworkProbe::portScan(scanHost, portsToScan, 2000, 64);
            DiagnosticResult r;
            r.id = id; r.group = DiagGroup::G4;
            r.durationMs = t.elapsed(); r.timestamp = QDateTime::currentDateTime();

            // ── Build merged-range output ──────────────────────────────
            int openCount = 0, closedCount = 0;
            QStringList namedOpen; // e.g. "22(ssh)"

            for (const auto& e : results) {
                if (e.open) { openCount++; }
                else { closedCount++; }
            }

            // ── Merge consecutive same-status ports into ranges ─────────
            int rangeStart = -1, rangeEnd = -1;
            bool rangeOpen = false;
            QMap<int, QString> portSvcMap; // port → service name for named ports

            for (const auto& e : results) {
                if (!e.serviceName.isEmpty()) portSvcMap[e.port] = e.serviceName;
            }

            // Collect raw (portStr, status) pairs for auto-width formatting
            QList<QPair<QString,QString>> portRows;

            auto flushRange = [&]() {
                if (rangeStart < 0) return;
                QString status = rangeOpen ? QStringLiteral("OPEN") : QStringLiteral("CLOSED");
                QString portStr = (rangeStart == rangeEnd)
                    ? QString::number(rangeStart)
                    : QStringLiteral("%1-%2").arg(rangeStart).arg(rangeEnd);
                portRows.append({portStr, status});
                // Collect for summary
                if (rangeOpen) {
                    if (rangeStart == rangeEnd) {
                        if (portSvcMap.contains(rangeStart))
                            namedOpen.append(QStringLiteral("%1(%2)").arg(rangeStart).arg(portSvcMap[rangeStart]));
                        else
                            namedOpen.append(QString::number(rangeStart));
                    } else if (rangeEnd - rangeStart <= 2) {
                        for (int p = rangeStart; p <= rangeEnd; p++) {
                            if (portSvcMap.contains(p))
                                namedOpen.append(QStringLiteral("%1(%2)").arg(p).arg(portSvcMap[p]));
                            else
                                namedOpen.append(QString::number(p));
                        }
                    } else {
                        namedOpen.append(QStringLiteral("%1-%2").arg(rangeStart).arg(rangeEnd));
                    }
                }
                rangeStart = -1;
            };

            for (const auto& e : results) {
                bool isOpen = e.open;
                if (rangeStart < 0) {
                    rangeStart = rangeEnd = e.port;
                    rangeOpen = isOpen;
                } else if (e.port == rangeEnd + 1 && isOpen == rangeOpen) {
                    rangeEnd = e.port;
                } else {
                    flushRange();
                    rangeStart = rangeEnd = e.port;
                    rangeOpen = isOpen;
                }
            }
            flushRange();

            // ── Table mode output (auto-width) ──────────────────────────
            QStringList out;
            out.append(QString());
            out.append(QStringLiteral("Port Scan Results for %1").arg(scanHost));

            // Compute column widths from data
            int portW = (int)strlen("Port range");
            int statusW = (int)strlen("Status");
            for (const auto& pr : portRows) {
                portW = qMax(portW, pr.first.length());
                statusW = qMax(statusW, pr.second.length());
            }
            // Header + separator
            out.append(QStringLiteral("  %1  %2")
                .arg(QStringLiteral("Port range"), -portW)
                .arg(QStringLiteral("Status"), -statusW));
            out.append(QStringLiteral("  %1  %2")
                .arg(QString(portW, '-'))
                .arg(QString(statusW, '-')));
            // Data rows
            for (const auto& pr : portRows)
                out.append(QStringLiteral("  %1  %2")
                    .arg(pr.first, -portW).arg(pr.second, -statusW));
            out.append(QString());
            r.rawOutput = out.join('\n');
            r.details = out.join('\n');

            // ── Summary: "18 ports closed, 1-3 opened, 22(ssh)" ─────────
            QStringList parts;
            if (closedCount > 0)
                parts.append(QStringLiteral("%1 ports closed").arg(closedCount));
            if (openCount > 0)
                parts.append(QStringLiteral("%1 ports opened").arg(openCount));
            if (!namedOpen.isEmpty())
                parts.append(namedOpen.join(QStringLiteral(", ")));
            r.summary = parts.join(QStringLiteral(", "));
            r.status = openCount > 0 ? DiagStatus::Pass : DiagStatus::Info;
            return r;
        }
        default:
            return DiagnosticResult::skipped(id, QStringLiteral("Unknown G4 test"));
    }
}

// ── G5: Website / URL ──────────────────────────────────────────────────────

#ifndef NO_CURL
DiagnosticResult DiagnosticEngine::runG5(DiagId id, const QString& target) {
    switch (id) {
        case DiagId::G5UrlParsing:      return G5WebsiteUrl::urlParsing(target);
        case DiagId::G5TcpConnect:      return G5WebsiteUrl::tcpConnect(target);
        case DiagId::G5ServiceBanner:   return G5WebsiteUrl::serviceBanner(target);
        case DiagId::G5CurlVerbose:     return G5WebsiteUrl::curlVerbose(target);
        case DiagId::G5HttpHeaders:     return G5WebsiteUrl::httpHeaders(target);
        case DiagId::G5SecurityHeaders: return G5WebsiteUrl::securityHeaders(target);
        case DiagId::G5SslCertificate:  return G5WebsiteUrl::sslCertificate(target);
        case DiagId::G5HttpRedirect:    return G5WebsiteUrl::httpRedirect(target);
        case DiagId::G5HttpCompression: return G5WebsiteUrl::httpCompression(target);
        case DiagId::G5HttpTiming:      return G5WebsiteUrl::httpTiming(target);
        case DiagId::G5FtpDiagnostics:  return G5WebsiteUrl::ftpDiagnostics(target);
        case DiagId::G5SshDiagnostics:  return G5WebsiteUrl::sshDiagnostics(target);
        case DiagId::G5EmailDiagnostics:return G5WebsiteUrl::emailDiagnostics(target);
        default:
            return DiagnosticResult::skipped(id, QStringLiteral("Unknown G5 test"));
    }
}
#endif // NO_CURL