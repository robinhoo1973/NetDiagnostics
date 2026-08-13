// =============================================================================
// DiagnosticMeta.cpp — 46-entry metadata table (contract layer)
//
// platforms values are the NEW-1 code-verified matrix:
//   DiagCapability.h manifest + TaskFactory.cpp #if + g5DiagMatchesScheme.
// =============================================================================
#include "Common/Model/DiagnosticMeta.h"

#include <iterator>

using namespace PlatformFlag;
using DP = DetailProfile;

// Convenience detail builders
static DP sys(const char* field = nullptr, const char* unit = nullptr, int prec = 0) {
    DP d;
    d.showErrorOutput = true; d.showProperties = true; d.showTerminal = true;
    d.keyMetricField = field; d.keyMetricUnit = unit; d.keyMetricPrecision = prec;
    return d;
}
static DP sysTT(const char* field = nullptr, const char* unit = nullptr, int prec = 0) {
    DP d = sys(field, unit, prec); d.terminalTypewriter = true; return d;
}
static DP metricOnly(const char* field, const char* unit, int prec, DP::ChartType chart,
                     const char* chartField = nullptr) {
    DP d;
    d.showErrorOutput = true; d.showProperties = true; d.showTerminal = true;
    d.keyMetricField = field; d.keyMetricUnit = unit; d.keyMetricPrecision = prec;
    d.chartType = chart; d.showCharts = (chart != DP::NoChart);
    // R1-1：图表数据键常与 keyMetric 不同（Ping=individualRtts、Path=hops、Request=waterfall）
    d.chartField = chartField ? chartField : field;
    return d;
}

static const DiagnosticMeta kDiagMeta[] = {
    // ── G1  System & Adapters ────────────────────────────────────────────────
    { DiagId::G1NetworkAdapters,   "Network Adapters",   "network-card", PF_All,                       DiagAnimType::Pulse,  DiagTemplateType::System, sys(),              15000 },
    { DiagId::G1NicAdvanced,       "NIC Advanced",       "cpu",          PF_Desktop|PF_Android,        DiagAnimType::Pulse,  DiagTemplateType::System, sys(),              60000 },
    { DiagId::G1WifiDiagnostics,   "WiFi Information",   "wifi",         PF_All,                       DiagAnimType::Pulse,  DiagTemplateType::System, sysTT(),            60000 },
    { DiagId::G1WiredDiagnostics,  "Wired Information",  "ethernet",     PF_Desktop|PF_Android,        DiagAnimType::Jiggle, DiagTemplateType::System, sys(),              60000 },
    { DiagId::G1DhcpStatus,        "DHCP Status",        "dhcp",         PF_All,                       DiagAnimType::Bounce, DiagTemplateType::System, sys("leaseCount","leases",0), 60000 },
    { DiagId::G1IpConfiguration,   "IP Configuration",   "ip-config",    PF_All,                       DiagAnimType::Jiggle, DiagTemplateType::System, sys(),              60000 },
    { DiagId::G1ActiveConnections, "Active Connections", "connections",  PF_Desktop|PF_Android,        DiagAnimType::Path,   DiagTemplateType::System, sys("tcpCount","connections",0), 60000 },
    { DiagId::G1CellularInfo,      "Cellular Information","cellular",    PF_All,                       DiagAnimType::Pulse,  DiagTemplateType::System, sys(),              60000 },

    // ── G2  Connectivity & Security ──────────────────────────────────────────
    { DiagId::G2NetworkProfile,    "Network Profile",    "network-profile", PF_All,                    DiagAnimType::Jiggle, DiagTemplateType::System, sys(),              60000 },
    { DiagId::G2TcpSettings,       "TCP Settings",       "tcp-settings", PF_Desktop|PF_Android,        DiagAnimType::Jiggle, DiagTemplateType::System, sys(),              60000 },
    { DiagId::G2DefaultGateway,    "Default Gateway",    "gateway",      PF_All,                       DiagAnimType::Jiggle, DiagTemplateType::System, sys(),              60000 },
    { DiagId::G2RoutingTable,      "Routing Table",      "route-table",  PF_All,                       DiagAnimType::Path,   DiagTemplateType::System, sys("routeCount","routes",0), 60000 },
    { DiagId::G2ArpTable,          "ARP Table",          "arp-table",    PF_Desktop|PF_Android,        DiagAnimType::Type,   DiagTemplateType::System, sys("entryCount","entries",0), 60000 },
    { DiagId::G2ProxySettings,     "Proxy Settings",     "proxy",        PF_Desktop|PF_Android,        DiagAnimType::Path,   DiagTemplateType::System, sys(),              60000 },

    // ── G3  Internet & DNS ───────────────────────────────────────────────────
    { DiagId::_G3Reserved17_Deprecated, "(removed)",     "circle",       PF_Desktop|PF_Android,        DiagAnimType::Pulse,  DiagTemplateType::System, sys(),              60000 },
    { DiagId::G3DnsServers,        "DNS Servers",        "dns-server",   PF_All,                       DiagAnimType::Pulse,  DiagTemplateType::System, sys("serverCount","servers",0), 60000 },
    { DiagId::G3DnsCache,          "DNS Cache",          "dns-cache",    PF_Desktop|PF_Android,        DiagAnimType::Jiggle, DiagTemplateType::System, sys("cacheEntries","entries",0), 60000 },
    { DiagId::G3DnsIntegrity,      "DNS Integrity",      "dns-shield",   PF_All,                       DiagAnimType::Lock,   DiagTemplateType::Handshake, metricOnly("overallScorePercent","%",0,DP::Gauge), 120000 },
    { DiagId::G3GeoIPLoc,          "IP Geolocation",     "geo-location", PF_All,                       DiagAnimType::Bounce, DiagTemplateType::System, sys(),              60000 },
    { DiagId::G3InternetConnectivity, "Internet Connectivity & Speed", "internet-check", PF_All,        DiagAnimType::Lock,   DiagTemplateType::Handshake, metricOnly("downloadMbpsBest","Mbps",1,DP::Gauge), 180000 },

    // ── G4  Remote Host (all platforms — NEW-1) ─────────────────────────────
    { DiagId::G4DnsResolution,     "DNS Resolution",     "dns-resolve",  PF_All,                       DiagAnimType::Bounce, DiagTemplateType::System, sys("queryTimeMs","ms",0), 60000 },
    { DiagId::G4Ping,              "Ping",               "ping",         PF_All,                       DiagAnimType::Bounce, DiagTemplateType::Ping,   metricOnly("rttAvgMs","ms",0,DP::BarChart,"individualRtts"), 30000 },
    { DiagId::G4Traceroute,        "Traceroute",         "traceroute",   PF_All,                       DiagAnimType::Path,   DiagTemplateType::Path,   metricOnly("hopCount","hops",0,DP::BarChart,"hops"), 90000 },
    { DiagId::G4PathPing,          "PathPing",           "path-ping",    PF_All,                       DiagAnimType::Bounce, DiagTemplateType::Path,   metricOnly("hopCount","hops",0,DP::BarChart,"hops"), 120000 },
    { DiagId::G4MtuDiscovery,      "MTU Discovery",      "mtu",          PF_All,                       DiagAnimType::Jiggle, DiagTemplateType::System, sys(),              60000 },
    { DiagId::G4IPv6Connectivity,  "IPv6 Connectivity",  "ipv6",         PF_All,                       DiagAnimType::Type,   DiagTemplateType::System, sys("connectedCount","ports",0), 60000 },

    // ── G5  Protocol ─────────────────────────────────────────────────────────
    { DiagId::G5UrlParsing,        "URL Parsing",        "url-parse",    PF_All,                       DiagAnimType::Type,   DiagTemplateType::System, sys(),              60000 },
    { DiagId::G5TcpConnect,        "TCP Connect",        "tcp-connect",  PF_All,                       DiagAnimType::Path,   DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5ServiceBanner,     "Service Banner",     "banner",       PF_All,                       DiagAnimType::Type,   DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5CurlVerbose,       "HTTP Request",       "curl-verbose", PF_All,                       DiagAnimType::Type,   DiagTemplateType::Request, []{ DP d = metricOnly("totalMs","ms",0,DP::BarChart,"waterfall"); d.showProperties=false; return d; }(), 120000 },
    { DiagId::G5HttpHeaders,       "HTTP Headers",       "http-headers", PF_All,                       DiagAnimType::Type,   DiagTemplateType::Request, sys("headerCount","headers",0), 60000 },
    { DiagId::G5SecurityHeaders,   "Security Headers",   "security-headers", PF_All,                   DiagAnimType::Lock,   DiagTemplateType::Handshake, metricOnly("score","score",0,DP::Gauge), 60000 },
    { DiagId::G5SslCertificate,    "SSL Certificate",    "certificate",  PF_All,                       DiagAnimType::Lock,   DiagTemplateType::Handshake, metricOnly("daysLeft","days",0,DP::Gauge), 60000 },
    { DiagId::G5HttpRedirect,      "HTTP Redirect",      "redirect",     PF_All,                       DiagAnimType::Path,   DiagTemplateType::Request, sys("redirectCount","hops",0), 60000 },
    { DiagId::G5HttpCompression,   "HTTP Compression",   "compression",  PF_All,                       DiagAnimType::Jiggle, DiagTemplateType::Request, sys("totalMs","ms",0), 60000 },
    { DiagId::G5HttpTiming,        "HTTP Timing",        "http-timing",  PF_All,                       DiagAnimType::Jiggle, DiagTemplateType::Request, []{ DP d = metricOnly("totalMs","ms",0,DP::BarChart,"waterfall"); d.showProperties=false; return d; }(), 90000 },
    { DiagId::G5FtpDiagnostics,    "FTP Diagnostics",    "ftp",          PF_All,                       DiagAnimType::Pulse,  DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5SshDiagnostics,    "SSH Diagnostics",    "ssh",          PF_All,                       DiagAnimType::Type,   DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5EmailDiagnostics,  "Email Diagnostics",  "mail",         PF_All,                       DiagAnimType::Jiggle, DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Telnet,            "Telnet",             "telnet",       PF_All,                       DiagAnimType::Path,   DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Mysql,             "MySQL",              "mysql",        PF_All,                       DiagAnimType::Pulse,  DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Postgres,          "PostgreSQL",         "postgres",     PF_All,                       DiagAnimType::Pulse,  DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Redis,             "Redis",              "redis",        PF_All,                       DiagAnimType::Pulse,  DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Mongodb,           "MongoDB",            "mongodb",      PF_All,                       DiagAnimType::Pulse,  DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Ldap,              "LDAP",               "ldap",         PF_All,                       DiagAnimType::Type,   DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Mqtt,              "MQTT",               "mqtt",         PF_All,                       DiagAnimType::Bounce, DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
};
static_assert(std::size(kDiagMeta) == 46, "kDiagMeta must cover all 46 DiagId values");

const DiagnosticMeta& diagnosticMeta(DiagId id) {
    const int idx = static_cast<int>(id);
    if (idx >= 0 && idx < static_cast<int>(std::size(kDiagMeta)))
        return kDiagMeta[idx];
    return kDiagMeta[0];
}
