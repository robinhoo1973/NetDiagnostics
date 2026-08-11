// =============================================================================
// DiagnosticMeta.cpp — 46-test metadata registry
// =============================================================================
#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagNames.h"

using namespace Platform;

// ── DetailProfile helpers ────────────────────────────────────────────────
static DetailProfile D(bool err, bool props, bool charts, bool term,
                       const char* field, const char* unit, int prec,
                       DetailProfile::ChartType ct, bool tt) {
    DetailProfile d;
    d.showErrorOutput = err;
    d.showProperties  = props;
    d.showCharts      = charts;
    d.showTerminal    = term;
    d.keyMetricField  = field;
    d.keyMetricUnit   = unit;
    d.keyMetricPrecision = prec;
    d.chartType       = ct;
    d.terminalTypewriter = tt;
    return d;
}
// 5WHY: PING/PATH/HAND/REQ/NO_CHART defined but never called — dead code.
// Only SYS (14 uses) and QRY (11 uses) are active in the 46-test table.
static DetailProfile SYS()  { return D(true, true, false, false, nullptr, nullptr, 0, DetailProfile::NoChart, false); }
static DetailProfile QRY() { DetailProfile d = D(true,true,true,true,"latencyMs","ms",0,DetailProfile::Gauge,false); return d; }

// ── 46-test metadata table ──────────────────────────────────────────────
static const DiagnosticMeta kDiagMeta[] = {
    // ── G1: System & Adapters ──────────────────────────────────────────
    { DiagId::G1NetworkAdapters,  "Network Adapters",  "network-card",  All,     DiagAnimType::Pulse,  DiagTemplateType::System,    SYS() },
    { DiagId::G1NicAdvanced,      "NIC Advanced",      "cpu",           Desktop, DiagAnimType::Pulse,  DiagTemplateType::System,    SYS() },
    { DiagId::G1WifiDiagnostics,  "WiFi Information",  "wifi",          All,     DiagAnimType::Pulse,  DiagTemplateType::System,    D(true,true,false,false,nullptr,nullptr,0,DetailProfile::NoChart,true) },
    { DiagId::G1WiredDiagnostics, "Wired Information", "ethernet",      Desktop, DiagAnimType::Jiggle, DiagTemplateType::System,    SYS() },
    { DiagId::G1DhcpStatus,       "DHCP Status",       "dhcp",          All,     DiagAnimType::Bounce, DiagTemplateType::System,    D(true,true,false,true,"leaseCount","leases",0,DetailProfile::NoChart,false) },
    { DiagId::G1IpConfiguration,  "IP Configuration",  "ip-config",     All,     DiagAnimType::Jiggle, DiagTemplateType::System,    SYS() },
    // 5WHY (chart wiring, 2026-08-11): showCharts=true with BarChart type but
    // templateType=System has no chart wiring in ResultChart (only Ping/Path/
    // Handshake/Request/Query get BarChart/Gauge).  The "Detailed Data" section
    // was always hidden because _source returned "" for System template.  Set
    // showCharts=false until a System chart variant is wired.
    { DiagId::G1ActiveConnections,"Active Connections","connections",   Desktop, DiagAnimType::Path,   DiagTemplateType::System,    D(true,true,false,false,"tcpCount","connections",0,DetailProfile::BarChart,false) },
    { DiagId::G1CellularInfo,     "Cellular Information","cellular",    Mobile,  DiagAnimType::Pulse,  DiagTemplateType::System,    SYS() },

    // ── G2: Connectivity & Security ────────────────────────────────────
    { DiagId::G2NetworkProfile,   "Network Profile",   "network-profile",All,   DiagAnimType::Jiggle, DiagTemplateType::System,    SYS() },
    { DiagId::G2TcpSettings,      "TCP Settings",      "tcp-settings",  Desktop, DiagAnimType::Jiggle, DiagTemplateType::System,    SYS() },
    { DiagId::G2DefaultGateway,   "Default Gateway",   "gateway",       All,     DiagAnimType::Jiggle, DiagTemplateType::System,    SYS() },
    // 5WHY: G2RoutingTable emits routes[] (dest/gateway/metric) — no hop RTTs.
    // Path template requires hops[].rttMs; System template (properties+terminal) fits.
    { DiagId::G2RoutingTable,     "Routing Table",     "route-table",   Desktop, DiagAnimType::Path,   DiagTemplateType::System,    D(true,true,false,true,"routeCount","routes",0,DetailProfile::NoChart,false) },
    { DiagId::G2ArpTable,         "ARP Table",         "arp-table",     Desktop, DiagAnimType::Type,   DiagTemplateType::System,    D(true,true,false,true,"entryCount","entries",0,DetailProfile::NoChart,false) },
    { DiagId::G2ProxySettings,    "Proxy Settings",    "proxy",         All,     DiagAnimType::Path,   DiagTemplateType::System,    SYS() },

    // ── G3: Internet & DNS ─────────────────────────────────────────────
    { DiagId::_G3Reserved17_Deprecated, "(removed)",    "circle",        0,      DiagAnimType::Jiggle, DiagTemplateType::System,    SYS() },
    { DiagId::G3DnsServers,       "DNS Servers",       "dns-server",    All,     DiagAnimType::Pulse,  DiagTemplateType::System,    D(true,true,false,true,"serverCount","servers",0,DetailProfile::NoChart,false) },
    { DiagId::G3DnsCache,         "DNS Cache",         "dns-cache",     All,     DiagAnimType::Jiggle, DiagTemplateType::System,    D(true,true,false,true,"cacheEntries","entries",0,DetailProfile::NoChart,false) },
    { DiagId::G3DnsIntegrity,     "DNS Integrity",     "dns-shield",    All,     DiagAnimType::Lock,   DiagTemplateType::Handshake, D(true,true,true,true,"overallScorePercent","%",0,DetailProfile::Gauge,false) },
    { DiagId::G3GeoIPLoc,         "GeoIP Location",    "geo-location",  All,     DiagAnimType::Bounce, DiagTemplateType::System,    SYS() },
    { DiagId::G3InternetConnectivity,"Internet Connectivity","internet-check",All,DiagAnimType::Lock, DiagTemplateType::Handshake, D(true,true,true,true,"downloadMbpsBest","Mbps",1,DetailProfile::BarChart,false) },

    // ── G4: Remote Host ────────────────────────────────────────────────
    // 5WHY: G4DnsResolution produces queryTimeMs + answerCount — no individualRtts[].
    // Ping template requires individualRtts; System template (properties+terminal) fits.
    { DiagId::G4DnsResolution,    "DNS Resolution",    "dns-resolve",   All,     DiagAnimType::Bounce, DiagTemplateType::System,    D(true,true,false,true,"queryTimeMs","ms",0,DetailProfile::NoChart,false) },
    { DiagId::G4Ping,             "Ping",              "ping",          All,     DiagAnimType::Bounce, DiagTemplateType::Ping,      D(true,true,true,true,"rttAvgMs","ms",0,DetailProfile::BarChart,false) },
    { DiagId::G4Traceroute,       "Traceroute",        "traceroute",    All,     DiagAnimType::Path,   DiagTemplateType::Path,      D(true,true,true,true,"hopCount","hops",0,DetailProfile::BarChart,false) },
    // 5WHY: G4PathPing emits hops[] with per-hop rttMs — matches Path template,
    // not Ping (which requires individualRtts[]).  DiagNames.h already had this fix.
    { DiagId::G4PathPing,         "PathPing",          "path-ping",     All,     DiagAnimType::Bounce, DiagTemplateType::Path,      D(true,true,true,true,"hopCount","hops",0,DetailProfile::BarChart,false) },
    { DiagId::G4MtuDiscovery,     "MTU Discovery",     "mtu",           All,     DiagAnimType::Jiggle, DiagTemplateType::System,    SYS() },
    // 5WHY (chart wiring, 2026-08-11): showCharts=true with BarChart type but
    // templateType=System has no chart wiring in ResultChart.  Same root cause
    // as G1ActiveConnections — "Detailed Data" section was always hidden.
    { DiagId::G4IPv6Connectivity, "IPv6 Connectivity", "ipv6",          All,     DiagAnimType::Type,   DiagTemplateType::System,    D(true,true,false,false,"connectedCount","ports",0,DetailProfile::BarChart,false) },

    // ── G5: Protocol ───────────────────────────────────────────────────
    // 5WHY: G5UrlParsing emits URL components (scheme/host/port/path/query) —
    // no HTTP timing data.  Request template requires dnsMs/connectMs; System
    // template (properties table) avoids the broken waterfall chart.
    { DiagId::G5UrlParsing,       "URL Parsing",       "url-parse",     All,     DiagAnimType::Type,   DiagTemplateType::System,    D(true,true,false,true,nullptr,nullptr,0,DetailProfile::NoChart,false) },
    { DiagId::G5TcpConnect,       "TCP Connect",       "tcp-connect",   All,     DiagAnimType::Path,   DiagTemplateType::Query,     QRY() },
    { DiagId::G5ServiceBanner,    "Service Banner",    "banner",        All,     DiagAnimType::Type,   DiagTemplateType::Query,     D(true,true,false,true,"latencyMs","ms",0,DetailProfile::Gauge,false) },
    { DiagId::G5CurlVerbose,      "cURL Verbose",      "curl-verbose",  Desktop, DiagAnimType::Type,   DiagTemplateType::Request,   D(true,false,true,true,"totalMs","ms",0,DetailProfile::BarChart,false) },
    { DiagId::G5HttpHeaders,      "HTTP Headers",      "http-headers",  All,     DiagAnimType::Type,   DiagTemplateType::Request,   D(true,true,false,true,"headerCount","headers",0,DetailProfile::NoChart,false) },
    { DiagId::G5SecurityHeaders,  "Security Headers",  "security-headers",All,  DiagAnimType::Lock,   DiagTemplateType::Handshake, D(true,true,true,true,"score","score",0,DetailProfile::Gauge,false) },
    { DiagId::G5SslCertificate,   "SSL Certificate",   "certificate",   All,     DiagAnimType::Lock,   DiagTemplateType::Handshake, D(true,true,true,true,"daysLeft","days",0,DetailProfile::Gauge,false) },
    { DiagId::G5HttpRedirect,     "HTTP Redirect",     "redirect",      All,     DiagAnimType::Path,   DiagTemplateType::Request,   D(true,true,false,true,"redirectCount","hops",0,DetailProfile::NoChart,false) },
    { DiagId::G5HttpCompression,  "HTTP Compression",  "compression",   All,     DiagAnimType::Jiggle, DiagTemplateType::Request,   D(true,true,false,true,"totalMs","ms",0,DetailProfile::NoChart,false) },
    { DiagId::G5HttpTiming,       "HTTP Timing",       "http-timing",   All,     DiagAnimType::Jiggle, DiagTemplateType::Request,   D(true,false,true,true,"totalMs","ms",0,DetailProfile::BarChart,false) },
    { DiagId::G5FtpDiagnostics,   "FTP Diagnostics",   "ftp",           All,     DiagAnimType::Pulse,  DiagTemplateType::Query,     QRY() },
    { DiagId::G5SshDiagnostics,   "SSH Diagnostics",   "ssh",           All,     DiagAnimType::Type,   DiagTemplateType::Query,     QRY() },
    { DiagId::G5EmailDiagnostics, "Email Diagnostics", "mail",          All,     DiagAnimType::Jiggle, DiagTemplateType::Query,     QRY() },
    { DiagId::G5Telnet,           "Telnet",            "telnet",        All,     DiagAnimType::Path,   DiagTemplateType::Query,     QRY() },
    { DiagId::G5Mysql,            "MySQL",             "mysql",         All,     DiagAnimType::Pulse,  DiagTemplateType::Query,     QRY() },
    { DiagId::G5Postgres,         "PostgreSQL",        "postgres",      All,     DiagAnimType::Pulse,  DiagTemplateType::Query,     QRY() },
    { DiagId::G5Redis,            "Redis",             "redis",         All,     DiagAnimType::Pulse,  DiagTemplateType::Query,     QRY() },
    { DiagId::G5Mongodb,          "MongoDB",           "mongodb",       All,     DiagAnimType::Pulse,  DiagTemplateType::Query,     QRY() },
    { DiagId::G5Ldap,             "LDAP",              "ldap",          All,     DiagAnimType::Type,   DiagTemplateType::Query,     QRY() },
    { DiagId::G5Mqtt,             "MQTT",              "mqtt",          All,     DiagAnimType::Bounce, DiagTemplateType::Query,     QRY() },
};

// Compile-time guard: the table must have exactly 46 entries.
static_assert(sizeof(kDiagMeta) / sizeof(kDiagMeta[0]) == 46,
              "kDiagMeta must have exactly 46 entries — one per DiagId");

const DiagnosticMeta& diagnosticMeta(DiagId id) {
    auto idx = static_cast<int>(id);
    if (idx >= 0 && idx < static_cast<int>(sizeof(kDiagMeta) / sizeof(kDiagMeta[0])))
        return kDiagMeta[idx];
    static const DiagnosticMeta s_default{};
    return s_default;
}
