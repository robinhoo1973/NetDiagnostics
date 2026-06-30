// =============================================================================
// TaskFactory.cpp — Create DiagnosticTask objects for each DiagId
// =============================================================================
#include "engine/task/TaskFactory.h"
#include "engine/diagnostic/G1G2G3Native.h"
#include "engine/diagnostic/G4RemoteHost.h"
#include "engine/runner/NetworkProbe.h"
#include "util/Logger.h"
#ifndef NO_CURL
#include "engine/diagnostic/G5WebsiteUrl.h"
#endif

// Per-test timeout values (ms). Default is 60000; shorter for fast tests.
static int timeoutFor(DiagId id) {
    switch (id) {
        case DiagId::G4Ping:            return 30000; // 4 probes at ~3s each
        case DiagId::G4Traceroute:      return 90000; // 30 hops at ~2s each
        case DiagId::G4PathPing:        return 120000;// traceroute + per-hop ping
        case DiagId::G3InternetSpeedTest:return 180000;// download + upload phases
        case DiagId::G5CurlVerbose:     return 120000;
        case DiagId::G5HttpTiming:      return 90000;
        default:                        return 60000;
    }
}

std::unique_ptr<DiagnosticTask> TaskFactory::createTask(
    DiagId id, const QString& target, int fromPort, int toPort, bool useCommonPorts)
{
    int tmo = timeoutFor(id);
    auto T = [&](GenericTask::Impl impl, int customTmo = -1) {
        return std::make_unique<GenericTask>(id, target, std::move(impl),
                                             customTmo > 0 ? customTmo : tmo);
    };

    switch (id) {
        // ── G1: System & Adapters ──────────────────────────────────────
        case DiagId::G1NetworkAdapters:
            return T([](DiagId id, const QString&) { return G1G2G3Native::networkAdapters(id); }, 15000);
        case DiagId::G1NicAdvanced:
            return T([](DiagId id, const QString&) { return G1G2G3Native::nicAdvanced(id); });
        case DiagId::G1WifiDiagnostics:
            return T([](DiagId id, const QString&) { return G1G2G3Native::wifiDiagnostics(id); });
        case DiagId::G1WiredDiagnostics:
            return T([](DiagId id, const QString&) { return G1G2G3Native::wiredDiagnostics(id); });
        case DiagId::G1DhcpStatus:
            return T([](DiagId id, const QString&) { return G1G2G3Native::dhcpStatus(id); });
        case DiagId::G1IpConfiguration:
            return T([](DiagId id, const QString&) { return G1G2G3Native::ipConfiguration(id); });
        case DiagId::G1ActiveConnections:
            return T([](DiagId id, const QString&) { return G1G2G3Native::activeConnections(id); });
        case DiagId::G1CellularInfo:
            return T([](DiagId id, const QString&) { return G1G2G3Native::cellularInfo(id); });

        // ── G2: Connectivity & Security ────────────────────────────────
        case DiagId::G2NetworkProfile:
            return T([](DiagId id, const QString&) { return G1G2G3Native::networkProfile(id); });
        case DiagId::G2TcpSettings:
            return T([](DiagId id, const QString&) { return G1G2G3Native::tcpSettings(id); });
        case DiagId::G2DefaultGateway:
            return T([](DiagId id, const QString&) { return G1G2G3Native::defaultGateway(id); });
        case DiagId::G2RoutingTable:
            return T([](DiagId id, const QString&) { return G1G2G3Native::routingTable(id); });
        case DiagId::G2ArpTable:
            return T([](DiagId id, const QString&) { return G1G2G3Native::arpTable(id); });
        case DiagId::G2ProxySettings:
            return T([](DiagId id, const QString&) { return G1G2G3Native::proxySettings(id); });

        // ── G3: Internet & DNS ─────────────────────────────────────────
        case DiagId::G3NetskopeStatus:
            return T([](DiagId id, const QString&) { return G1G2G3Native::netskopeStatus(id); });
        case DiagId::G3DnsServers:
            return T([](DiagId id, const QString&) { return G1G2G3Native::dnsServers(id); });
        case DiagId::G3DnsCache:
            return T([](DiagId id, const QString&) { return G1G2G3Native::dnsCache(id); });
        case DiagId::G3DnsPollution:
            return T([](DiagId id, const QString&) { return G1G2G3Native::dnsPollution(id); });
        case DiagId::G3InternetSpeedTest:
            return T([](DiagId id, const QString&) { return G1G2G3Native::speedTest(id); });

        // ── G4: Remote Host ────────────────────────────────────────────
        case DiagId::G4DnsResolution:
            return T([](DiagId, const QString& t) { return G4RemoteHost::dnsResolution(t); });
        case DiagId::G4Ping:
            return T([](DiagId, const QString& t) { return G4RemoteHost::ping(t); });
        case DiagId::G4Traceroute:
            return T([](DiagId, const QString& t) { return G4RemoteHost::traceroute(t); });
        case DiagId::G4PathPing:
            return T([](DiagId, const QString& t) { return G4RemoteHost::pathPing(t); });
        case DiagId::G4MtuDiscovery:
            return T([](DiagId, const QString& t) { return G4RemoteHost::mtuDiscovery(t); });
        case DiagId::G4PortScan: {
            QString scanHost = G4RemoteHost::extractHostname(target);
            return T([scanHost, fromPort, toPort, useCommonPorts](DiagId id, const QString&) {
                QVector<int> ports;
                if (useCommonPorts)
                    ports = NetworkProbe::commonDiagnosticPorts();
                if (fromPort > 0 && toPort >= fromPort) {
                    for (int p = qBound(1, fromPort, 65535); p <= qBound(fromPort, toPort, 65535); ++p)
                        if (!ports.contains(p)) ports.append(p);
                }
                std::sort(ports.begin(), ports.end());

                QElapsedTimer t; t.start();
                auto results = NetworkProbe::portScan(scanHost, ports, 2000, 64);
                DiagnosticResult r;
                r.id = id; r.group = DiagGroup::G4;
                r.durationMs = t.elapsed(); r.timestamp = QDateTime::currentDateTime();

                QList<QPair<QString,QString>> portRows;
                QMap<int, QString> portSvcMap;
                for (const auto& e : results)
                    if (!e.serviceName.isEmpty()) portSvcMap[e.port] = e.serviceName;

                int rangeStart = -1, rangeEnd = -1;
                bool rangeOpen = false;
                auto flushRange = [&]() {
                    if (rangeStart < 0) return;
                    QString status = rangeOpen ? QStringLiteral("OPEN") : QStringLiteral("CLOSED");
                    QString portStr = (rangeStart == rangeEnd)
                        ? QString::number(rangeStart)
                        : QStringLiteral("%1-%2").arg(rangeStart).arg(rangeEnd);
                    portRows.append({portStr, status});
                    rangeStart = -1;
                };
                for (const auto& e : results) {
                    if (rangeStart < 0) { rangeStart = rangeEnd = e.port; rangeOpen = e.open; }
                    else if (e.port == rangeEnd + 1 && e.open == rangeOpen) { rangeEnd = e.port; }
                    else { flushRange(); rangeStart = rangeEnd = e.port; rangeOpen = e.open; }
                }
                flushRange();

                QStringList out;
                out.append(QString());
                out.append(QStringLiteral("Port Scan Results for %1").arg(scanHost));
                int portW = (int)strlen("Port range"), statusW = (int)strlen("Status");
                for (const auto& pr : portRows) {
                    portW = qMax(portW, pr.first.length());
                    statusW = qMax(statusW, pr.second.length());
                }
                out.append(QStringLiteral("  %1  %2").arg(QStringLiteral("Port range"), -portW).arg(QStringLiteral("Status"), -statusW));
                out.append(QStringLiteral("  %1  %2").arg(QString(portW, '-')).arg(QString(statusW, '-')));
                for (const auto& pr : portRows)
                    out.append(QStringLiteral("  %1  %2").arg(pr.first, -portW).arg(pr.second, -statusW));
                out.append(QString());
                r.rawOutput = out.join('\n'); r.details = out.join('\n');
                int openCount = 0, closedCount = 0; QStringList namedOpen;
                for (const auto& e : results) { if (e.open) openCount++; else closedCount++; }
                r.summary = QStringLiteral("%1 ports closed, %2 ports opened").arg(closedCount).arg(openCount);
                r.status = openCount > 0 ? DiagStatus::Pass : DiagStatus::Info;
                return r;
            }, 90000);
        }

        // ── G5: Website / URL ──────────────────────────────────────────
#ifndef NO_CURL
        case DiagId::G5UrlParsing:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::urlParsing(t); });
        case DiagId::G5TcpConnect:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::tcpConnect(t); });
        case DiagId::G5ServiceBanner:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::serviceBanner(t); });
        case DiagId::G5CurlVerbose:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::curlVerbose(t); });
        case DiagId::G5HttpHeaders:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::httpHeaders(t); });
        case DiagId::G5SecurityHeaders:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::securityHeaders(t); });
        case DiagId::G5SslCertificate:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::sslCertificate(t); });
        case DiagId::G5HttpRedirect:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::httpRedirect(t); });
        case DiagId::G5HttpCompression:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::httpCompression(t); });
        case DiagId::G5HttpTiming:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::httpTiming(t); });
        case DiagId::G5FtpDiagnostics:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::ftpDiagnostics(t); });
        case DiagId::G5SshDiagnostics:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::sshDiagnostics(t); });
        case DiagId::G5EmailDiagnostics:
            return T([](DiagId, const QString& t) { return G5WebsiteUrl::emailDiagnostics(t); });
#else
        case DiagId::G5UrlParsing:       // fall through — NO_CURL: skip all G5
        case DiagId::G5TcpConnect:
        case DiagId::G5ServiceBanner:
        case DiagId::G5CurlVerbose:
        case DiagId::G5HttpHeaders:
        case DiagId::G5SecurityHeaders:
        case DiagId::G5SslCertificate:
        case DiagId::G5HttpRedirect:
        case DiagId::G5HttpCompression:
        case DiagId::G5HttpTiming:
        case DiagId::G5FtpDiagnostics:
        case DiagId::G5SshDiagnostics:
        case DiagId::G5EmailDiagnostics:
            return T([](DiagId id, const QString&) {
                return DiagnosticResult::skipped(id, QStringLiteral("G5 Website/URL tests unavailable (no curl)"));
            });
#endif
    }
    Logger::instance().event(QStringLiteral("Unknown DiagId: %1").arg(static_cast<int>(id)));
    return nullptr;
}
