// =============================================================================
// DiagnosticMeta.cpp — 44-entry metadata table (contract layer)
//
// platforms values are the NEW-1 code-verified matrix:
//   DiagCapability.h manifest + TaskFactory.cpp #if + g5DiagMatchesScheme.
// =============================================================================
#include "Common/Model/DiagnosticMeta.h"

#include <QHash>
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
static DP sysGrouped(const char* field = nullptr, const char* unit = nullptr, int prec = 0) {
    // 多实例检测（网卡/接口）：属性卡按实例分组渲染（PagePropertiesSection Grouped）
    DP d = sys(field, unit, prec); d.propLayout = DP::PropLayout::Grouped; return d;
}
static DP sysGroupedTT(const char* field = nullptr, const char* unit = nullptr, int prec = 0) {
    // 5WHY (复核 2026-08-20 Grouped 丢打字机): WiFi 行曾由 sysTT 换 sysGrouped——
    // 终端打字机动画属性随基类 sys() 丢失。分组 + 打字机需显式组合。
    DP d = sysGrouped(field, unit, prec); d.terminalTypewriter = true; return d;
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
    { DiagId::G1NetworkAdapters,   "Network Adapters",   "nd-diag-g1-network-adapters", PF_All,                       DiagAnimType::Pulse,  DiagTemplateType::System, sysGrouped(),              15000 },
    { DiagId::G1NicAdvanced,       "NIC Advanced",       "nd-diag-g1-nic-advanced", PF_Desktop|PF_Android,        DiagAnimType::BarsCycle,  DiagTemplateType::System, sysGrouped(),              60000 },
    // 5WHY (复核 2026-08-21 用户诉求 "WiFi 信息动画同款弧线"): 曾为 Pulse
    // （通用呼吸环，与图标三弧无语义锚点）——与 Internet Connectivity
    // 同用 WifiWave 弧线动画（锚点按图标名由 AnimationTokens.js
    // wifiWaveAnchorSets 分派，WiFi 信息图标有自己的同心弧几何）。
    { DiagId::G1WifiDiagnostics,   "WiFi Information",   "nd-diag-g1-wifi-info",         PF_All,                       DiagAnimType::WifiWave,  DiagTemplateType::System, sysGroupedTT(),            60000 },
    { DiagId::G1WiredDiagnostics,  "Wired Information",  "nd-diag-g1-wired",  PF_Desktop|PF_Android,        DiagAnimType::BarsCycle, DiagTemplateType::System, sysGrouped(),              60000 },
    { DiagId::G1DhcpStatus,        "DHCP Status",        "nd-diag-g1-dhcp",        PF_All,                       DiagAnimType::Bounce, DiagTemplateType::System, sysGrouped("leaseCount","leases",0), 60000 },
    { DiagId::G1IpConfiguration,   "IP Configuration",   "nd-diag-g1-ip-config",    PF_All,                       DiagAnimType::TypeText, DiagTemplateType::System, sysGrouped(),              60000 },
    { DiagId::G1ActiveConnections, "Active Connections", "nd-diag-g1-active-connections",  PF_Desktop|PF_Android,        DiagAnimType::PlugCycle,   DiagTemplateType::System, sys("tcpCount","connections",0), 60000 },
    { DiagId::G1CellularInfo,      "Cellular Information","nd-diag-g1-cellular",    PF_All,                       DiagAnimType::BarsCycle,  DiagTemplateType::System, sys(),              60000 },

    // ── G2  Connectivity & Security ──────────────────────────────────────────
    { DiagId::G2NetworkProfile,    "Network Profile",    "nd-diag-g2-network-profile", PF_All,                    DiagAnimType::FlashContent, DiagTemplateType::System, sys(),              60000 },
    // 5WHY (复核 2026-08-20): Gateway 行曾随 grouped 改造误改 animType
    // （Converge→Jiggle）且改为 sysGrouped——但其 props 是扁平
    // gateway→interface 对（无 children），Kv 呈现才是原设计。两者还原。
    { DiagId::G2DefaultGateway,    "Default Gateway",    "nd-diag-g2-gateway",     PF_All,                       DiagAnimType::Converge, DiagTemplateType::System, sys(),              60000 },
    { DiagId::G2RoutingTable,      "Routing Table",      "nd-diag-g2-routing-table",  PF_All,                       DiagAnimType::Path,   DiagTemplateType::System, sys("routeCount","routes",0), 60000 },
    { DiagId::G2ArpTable,          "ARP Table",          "nd-diag-g2-arp-table",    PF_Desktop|PF_Android,        DiagAnimType::BlinkText,   DiagTemplateType::System, sys("entryCount","entries",0), 60000 },
    { DiagId::G2ProxySettings,     "Proxy Settings",     "nd-diag-g2-proxy",        PF_Desktop|PF_Android,        DiagAnimType::ChevronCycle,   DiagTemplateType::System, sys(),              60000 },

    // ── G3  Internet & DNS ───────────────────────────────────────────────────
    { DiagId::G3DnsServers,        "DNS Servers",        "nd-diag-g3-dns-servers",   PF_All,                       DiagAnimType::BlinkText,  DiagTemplateType::System, sys("serverCount","servers",0), 60000 },
    { DiagId::G3DnsCache,          "DNS Cache",          "nd-diag-g3-dns-cache",    PF_Desktop|PF_Android,        DiagAnimType::BlinkText, DiagTemplateType::System, sys("cacheEntries","entries",0), 60000 },
    // 5WHY (2026-08-23 详情页信息前置): 三大件 summaryOutline——阶段序列
    // 静态声明于 meta（动态数值仍由探针 narrative 承载），Summary 卡在
    // 结论行之上渲染过程概览；值为翻译键（QML T.tr 按语言渲染）。
    { DiagId::G3DnsIntegrity,      "DNS Integrity",      "nd-diag-g3-dns-integrity",   PF_All,
      DiagAnimType::Tick, DiagTemplateType::Handshake,
      []{ DP d = metricOnly("overallScorePercent","%",0,DP::Gauge);
          d.summaryOutline = {
              QStringLiteral("outlineDnsIntegrityPh1"),
              QStringLiteral("outlineDnsIntegrityPh2"),
              QStringLiteral("outlineDnsIntegrityVerdict") };
          return d; }(), 120000 },
    // 5WHY (2026-08-19 用户诉求 "IP 动画不知道在干什么"): Bounce 是通用
    // 往返小球，地球+定位针图形上无语义锚点——改用 GeoRadar（针头雷达波
    // 扩散，语义即"正在定位"）。
    { DiagId::G3GeoIPLoc,          "IP Geolocation",     "nd-diag-g3-geoip", PF_All,
      DiagAnimType::GeoRadar, DiagTemplateType::System,
      []{ DP d = sys();
          d.summaryOutline = {
              QStringLiteral("outlineGeoIpPh1"),
              QStringLiteral("outlineGeoIpPh2"),
              QStringLiteral("outlineGeoIpPh3") };
          return d; }(), 150000 },
    { DiagId::G3InternetConnectivity, "Internet Connectivity & Speed", "nd-diag-g3-internet", PF_All,
      DiagAnimType::WifiWave, DiagTemplateType::Handshake,
      []{ DP d = metricOnly("downloadMbpsBest","Mbps",1,DP::Gauge);
          d.summaryOutline = {
              QStringLiteral("outlineInternetPh1"),
              QStringLiteral("outlineInternetPh2"),
              QStringLiteral("outlineInternetPh3"),
              QStringLiteral("outlineInternetPh4") };
          return d; }(), 180000 },

    // ── G4  Remote Host (all platforms — NEW-1) ─────────────────────────────
    { DiagId::G4DnsResolution,     "DNS Resolution",     "nd-diag-g4-dns-resolution",  PF_All,                       DiagAnimType::BlinkText, DiagTemplateType::System, sys("queryTimeMs","ms",0), 60000 },
    // 5WHY (2026-08-23): Ping/Traceroute/PathPing 三动画与 G4 新母版锚定
    // （Sonar=radar 扫掠 / RouteTrace=route S 脊 / HopSample=waypoints 逐跳
    // 采样）——通用 Bounce/Path 与图形无语义锚点，见 anim/04 设计文档。
    { DiagId::G4Ping,              "Ping",               "nd-diag-g4-ping",         PF_All,                       DiagAnimType::Sonar, DiagTemplateType::Ping,   metricOnly("rttAvgMs","ms",0,DP::BarChart,"individualRtts"), 30000 },
    { DiagId::G4Traceroute,        "Traceroute",         "nd-diag-g4-traceroute",   PF_All,                       DiagAnimType::RouteTrace,   DiagTemplateType::Path,   metricOnly("hopCount","hops",0,DP::BarChart,"hops"), 90000 },
    { DiagId::G4PathPing,          "PathPing",           "nd-diag-g4-pathping",    PF_All,                       DiagAnimType::HopSample, DiagTemplateType::Path,   metricOnly("hopCount","hops",0,DP::BarChart,"hops"), 120000 },
    { DiagId::G4MtuDiscovery,      "MTU Discovery",      "nd-diag-g4-mtu",          PF_All,                       DiagAnimType::Measure, DiagTemplateType::System, sys(),              60000 },
    { DiagId::G4IPv6Connectivity,  "IPv6 Connectivity",  "nd-diag-g4-ipv6",         PF_All,                       DiagAnimType::BlinkText,   DiagTemplateType::System, sys("connectedCount","ports",0), 60000 },

    // ── G5  Protocol ─────────────────────────────────────────────────────────
    { DiagId::G5UrlParsing,        "URL Parsing",        "nd-diag-g5-url-parsing",    PF_All,                       DiagAnimType::Type,   DiagTemplateType::System, sys(),              60000 },
    { DiagId::G5TcpConnect,        "TCP Connect",        "nd-diag-g5-tcp-connect",  PF_All,                       DiagAnimType::Path,   DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5ServiceBanner,     "Service Banner",     "nd-diag-g5-service-banner", PF_All,                       DiagAnimType::Type,   DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5CurlVerbose,       "HTTP Request",       "nd-diag-g5-curl-verbose", PF_All,                       DiagAnimType::FlashContent,   DiagTemplateType::Request, []{ DP d = metricOnly("totalMs","ms",0,DP::BarChart,"waterfall"); d.showProperties=false; return d; }(), 120000 },
    { DiagId::G5HttpHeaders,       "HTTP Headers",       "nd-diag-g5-http-headers", PF_All,                       DiagAnimType::FlashContent,   DiagTemplateType::Request, sys("headerCount","headers",0), 60000 },
    { DiagId::G5SecurityHeaders,   "Security Headers",   "nd-diag-g5-security-headers", PF_All,                   DiagAnimType::FlashContent,   DiagTemplateType::Handshake, metricOnly("score","score",0,DP::Gauge), 60000 },
    { DiagId::G5SslCertificate,    "SSL Certificate",    "nd-diag-g5-ssl-certificate",  PF_All,                       DiagAnimType::BlinkText,   DiagTemplateType::Handshake, metricOnly("daysLeft","days",0,DP::Gauge), 60000 },
    { DiagId::G5HttpRedirect,      "HTTP Redirect",      "nd-diag-g5-http-redirect",     PF_All,                       DiagAnimType::FlashContent,   DiagTemplateType::Request, sys("redirectCount","hops",0), 60000 },
    { DiagId::G5HttpCompression,   "HTTP Compression",   "nd-diag-g5-http-compression",  PF_All,                       DiagAnimType::FlashContent, DiagTemplateType::Request, sys("totalMs","ms",0), 60000 },
    { DiagId::G5HttpTiming,        "HTTP Timing",        "nd-diag-g5-http-timing",  PF_All,                       DiagAnimType::FlashContent, DiagTemplateType::Request, []{ DP d = metricOnly("totalMs","ms",0,DP::BarChart,"waterfall"); d.showProperties=false; return d; }(), 90000 },
    { DiagId::G5FtpDiagnostics,    "FTP Diagnostics",    "nd-diag-g5-ftp",          PF_All,                       DiagAnimType::TermType,  DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5SshDiagnostics,    "SSH Diagnostics",    "nd-diag-g5-ssh",          PF_All,                       DiagAnimType::TermType,   DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5EmailDiagnostics,  "Email Diagnostics",  "nd-diag-g5-email",         PF_All,                       DiagAnimType::Jiggle, DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Telnet,            "Telnet",             "nd-diag-g5-telnet",       PF_All,                       DiagAnimType::TermType,   DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Mysql,             "MySQL",              "nd-diag-g5-mysql",        PF_Desktop,                 DiagAnimType::FlashLabel,  DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Postgres,          "PostgreSQL",         "nd-diag-g5-postgres",         PF_Desktop,                 DiagAnimType::FlashLabel,  DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Redis,             "Redis",              "nd-diag-g5-redis",        PF_Desktop,                 DiagAnimType::FlashLabel,  DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Mongodb,           "MongoDB",            "nd-diag-g5-mongodb",      PF_Desktop,                 DiagAnimType::FlashLabel,  DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Ldap,              "LDAP",               "nd-diag-g5-ldap",         PF_Desktop,                 DiagAnimType::Pulse,   DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
    { DiagId::G5Mqtt,              "MQTT",               "nd-diag-g5-mqtt",         PF_Desktop,                 DiagAnimType::Bounce, DiagTemplateType::Query,  metricOnly("latencyMs","ms",0,DP::Gauge), 60000 },
};
static_assert(std::size(kDiagMeta) == 44, "kDiagMeta must cover all 44 DiagId values");

const DiagnosticMeta& diagnosticMeta(DiagId id) {
    const int idx = static_cast<int>(id);
    if (idx >= 0 && idx < static_cast<int>(std::size(kDiagMeta)))
        return kDiagMeta[idx];
    // L4 (5WHY): 越界 ID 返回 kDiagMeta[0]（G1NetworkAdapters）——静默
    // 返回错误元数据会导致 UI 显示错误图标/名称/超时。加警告日志，
    // 开发期暴露枚举与元数据表的漂移。once 守卫：诊断循环逐 id 调用，
    // 漂移时避免每次调用刷屏。
    static bool s_warned = false;
    if (!s_warned) {
        s_warned = true;
        qWarning("diagnosticMeta: DiagId %d out of range, returning G1NetworkAdapters fallback", idx);
    }
    return kDiagMeta[0];
}

// ── §6.1：registry 推导覆写（启动时 verifyAllDiagIds 调用）─────────────────
// Meyer's Singleton 覆写表——registry 是 platforms 的单一权威，编译期表仅基线。
static QHash<DiagId, unsigned>& platformOverrides() {
    static QHash<DiagId, unsigned> s_overrides;
    return s_overrides;
}

void setMetaPlatformOverride(DiagId id, unsigned flags) {
    if (diagnosticMeta(id).platforms != flags)
        platformOverrides().insert(id, flags);
}

unsigned effectiveMetaPlatforms(DiagId id) {
    const auto it = platformOverrides().constFind(id);
    return it != platformOverrides().constEnd() ? it.value() : diagnosticMeta(id).platforms;
}
