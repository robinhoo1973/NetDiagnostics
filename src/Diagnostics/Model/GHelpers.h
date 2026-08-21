// GHelpers.h — shared helpers for G1/G2/G3 per-function .cpp files.
// 5WHY (复核 2026-08-20 自包含): 本头曾依赖各 TU 传递包含提供 in_addr/
// inet_ntop——想共享的 TU（G1/G5）未包含即编译失败，导致同源 helper
// 在 G1/G2 各留一份本地副本（ipToStr 三份漂移）。in_addr 平台包含收敛
// 于此（winsock2 已在各 Windows TU 顶部先行，此处重复包含由守卫无害）。
#pragma once
#include "Diagnostics/Model/GBase.h"
#include "Diagnostics/View/DiagnosticFormatter.h"
#include "Common/Services/Logger.h"
#include "Common/Services/PlatformAdapter.h"   // RunContext / RunSnapshot

#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QMutexLocker>
#include <QtEndian>
#include <cmath>
#include <memory>

#if defined(Q_OS_WIN)
#include <winsock2.h>   // in_addr（各 Windows TU 已先包含，此为保证序）
#else
#include <netinet/in.h>
#include <arpa/inet.h>  // inet_ntop / INET_ADDRSTRLEN
#endif

namespace SystemDiagnostics {

// ── MAC address formatting ──────────────────────────────────────────
static QString macToStr(const unsigned char* mac) {
    return QStringLiteral("%1:%2:%3:%4:%5:%6")
        .arg(mac[0], 2, 16, QLatin1Char('0'))
        .arg(mac[1], 2, 16, QLatin1Char('0'))
        .arg(mac[2], 2, 16, QLatin1Char('0'))
        .arg(mac[3], 2, 16, QLatin1Char('0'))
        .arg(mac[4], 2, 16, QLatin1Char('0'))
        .arg(mac[5], 2, 16, QLatin1Char('0'));
}

// ── IPv4 formatting ─────────────────────────────────────────────────
static QString ip4ToStr(struct in_addr a) {
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &a, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}
static QString ipToStr(uint32_t ip) {
    // 5WHY (复核 2026-08-21 大端回归): 曾 s_addr = ip——/proc/net/route 以
    // 小端十六进制存地址，仅小端主机上其内存序恰合网络序；大端 Linux
    // （s390x/BE PowerPC）四段倒序打印（被删的 G2 本地副本显式逐字节
    // 反转故无此病）。qFromLittleEndian 显式按小端语义还原主机序，
    // 两端一致（小端恒等，行为不变）。
    struct in_addr a; a.s_addr = qFromLittleEndian<quint32>(ip);
    return ip4ToStr(a);
}

// ── 属性 → 终端转储文本（"label: value" + 两空格缩进子行）─────────────
// 5WHY (复核 2026-08-21 三份同构): G1 makeResult 派生 details、AppState
// 剪贴板属性循环、resultFor 兜底派生三处同格式复制——改缩进/分隔符漏
// 一处即剪贴板与终端区块分叉。单一 helper 消费（空 props 返回空串）。
static QString propsDumpText(const QVector<ResultProperty>& props) {
    QStringList lines;
    for (const ResultProperty& p : props) {
        lines.append(QStringLiteral("%1: %2").arg(p.label, p.value));
        for (const auto& c : p.children)
            lines.append(QStringLiteral("  %1: %2").arg(c.label, c.value));
    }
    return lines.join(QLatin1Char('\n'));
}

// ── 中心频率(MHz) → WiFi 信道号 ─────────────────────────────────────────
// 5WHY (复核 2026-08-20 双份漂移): G1（GHz double）与 Android G5（MHz int）
// 各一份频段表且边界不一致——5885 MHz 在 Linux 判 5 GHz、Android 判
// 6 GHz；5160 MHz 在 Linux 算 32、Android 判 0。收敛为此单一份
// （nl80211 定义：2.4: (f-2412)/5+1；5: (f-5000)/5；6: (f-5955)/5+1）。
// 5WHY (复核 2026-08-20 边界钳制): 曾 2.4 GHz 公式在带顶算出信道 15
// （2.484 GHz → (2.484-2.412)/0.005=14.4 → round+1=15）——2.4 GHz 只有
// 1-14。按频段钳制到真实信道范围，带外返回 0（调用方呈现诚实缺省）。
// 5WHY (复核 2026-08-21 中心校核): 曾带内任意值就近取整——5000/5100 MHz
// 报信道 32、5925 MHz 报信道 185（均非真实信道中心，诚實缺省原则落空）。
// 就近取整后校验与真实信道中心偏差 ≤2.5 MHz，非中心频率返回 0。
inline int wifiChannelFromFreqMhz(double freqMhz) {
    if (freqMhz >= 2412.0 && freqMhz <= 2484.0) {
        const int ch = qBound(1, int((freqMhz - 2412.0) / 5.0 + 0.5) + 1, 14);
        // 信道 14 中心 2484 不连续（2472→2484 间隔 12 MHz），单独处理
        const double center = (ch == 14) ? 2484.0 : 2412.0 + 5.0 * (ch - 1);
        return (std::abs(freqMhz - center) <= 2.5) ? ch : 0;
    }
    if (freqMhz >= 5000.0 && freqMhz <= 5925.0) {
        // 上钳 177（5885 MHz）：5925/5900 等非中心频率经 185 取整后
        // 会被钳回 177，中心校核（|f−5885|≤2.5）将其拒绝为 0——钳 200
        // 时它们自洽通过（5925→185 伪信道，诚实缺省原则落空）。
        const int ch = qBound(32, int((freqMhz - 5000.0) / 5.0 + 0.5), 177);
        const double center = 5000.0 + 5.0 * ch;
        return (std::abs(freqMhz - center) <= 2.5) ? ch : 0;
    }
    if (freqMhz > 5925.0 && freqMhz <= 7125.0) {
        // std::round 而非 int()+0.5：负偏移时 int() 向零截断（5935 →
        // round(-4)+1=-3 同样错）——中心校核把 5926–5954 间隙拒绝为 0，
        // 但取整本身应无符号歧义。
        const int ch = qBound(1, int(std::round((freqMhz - 5955.0) / 5.0)) + 1, 233);
        const double center = 5950.0 + 5.0 * ch;
        return (std::abs(freqMhz - center) <= 2.5) ? ch : 0;
    }
    return 0;   // 非 WiFi 频段（未关联/驱动不报频段）
}

// 频率(MHz) → 频段标签（与 wifiChannelFromFreqMhz 共享同一组边界常量）。
// 5WHY (复核 2026-08-21 标签漂移): 信道表收敛后 Android 频段标签仍是本地
// 边界表（>5825 判 6 GHz）——5885 MHz 信道算 177（5 GHz 表）标签却印
// "6 GHz"，同频自相矛盾。带外返回 "?"（曾误标 2.4 GHz）。
inline QString wifiBandLabelMhz(double freqMhz) {
    if (freqMhz >= 2412.0 && freqMhz <= 2484.0) return QStringLiteral("2.4 GHz");
    if (freqMhz >= 5000.0 && freqMhz <= 5925.0) return QStringLiteral("5 GHz");
    if (freqMhz > 5925.0 && freqMhz <= 7125.0) return QStringLiteral("6 GHz");
    return QStringLiteral("?");
}

// ── 蜂窝"无服务"叙述（iOS/Android 共用单一来源）──────────────────────
// 5WHY (复核 2026-08-20 文案三份): 同一句叙述曾逐字复制于 G1 iOS 与
// G5 Android——措辞修正/i18n 提取需逐文件改。收敛于此。
static QString cellularNoServiceNarrative() {
    return QStringLiteral("No cellular service is currently available on this device "
        "(no SIM registered with a usable data plan, or the modem is offline).");
}

// ── /proc 伪文件行读取（procfs atEnd 陷阱）─────────────────────────────
// 5WHY (复核 2026-08-20 六份同构): /proc size 恒 0，atEnd() 在首行缓冲前
// 恒真——"readLine 表头 + while(!atEnd())" 换无表头文件即零行（G2 IPv6
// 网关即因此从未上报）。该陷阱曾以 readLineInto 内联 + 逐处注释复制
// 六份。readLineInto 驱动，与文件大小无关；skipHeader 行跳过。
inline QStringList readProcLines(const QString& path, int skipHeader = 0) {
    QStringList out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return out;
    QTextStream ts(&f);
    QString line;
    while (ts.readLineInto(&line)) {
        if (skipHeader > 0) { --skipHeader; continue; }
        out.append(line);
    }
    return out;
}

// ── 每轮共享的工具执行（RunSnapshot 缓存）─────────────────────────────
// 5WHY (复核 2026-08-20 收敛): 曾 G1 本地 runTool 收敛 5 份样板，但 G2/G3
// 仍手写同构 start→waitForFinished(N)→kill→waitForFinished(2000)——kill
// 对漏写即 R5-1 类崩溃（运行中 QProcess 析构 qFatal）的再引入点。共享
// runner + RunSnapshot 按 exe+args 缓存（一轮只跑一次），key 含参数故
// 每 modem/每接口不同参数不串缓存。iOS QT_NO_PROCESS 下编译为空。
#if !defined(QT_NO_PROCESS)
inline QString cachedRunTool(RunContext& ctx, const QString& exe,
                             const QStringList& args, int timeoutMs) {
    const auto runOnce = [&]() -> QString {
        QProcess proc;
        proc.start(exe, args);
        if (proc.waitForFinished(timeoutMs))
            return QString::fromLocal8Bit(proc.readAllStandardOutput());
        proc.kill();
        proc.waitForFinished(2000);   // R5-1：析构前必须已终止
        return QString();
    };
    if (!ctx.snapshot) return runOnce();   // 无快照（harness/单探针）直跑
    const QString key = exe + QLatin1Char(' ') + args.join(QLatin1Char(' '));
    // 5WHY (复核 2026-08-21 串行化): 曾单把表锁覆盖 runOnce 全程——不同命令
    // 也被串行化（nmcli 4s + iw 3s + mmcli 3s 一轮最坏 ~10s）。改为：表锁
    // 只做命中检查/键锁注册（O(1)），进程执行在键锁下进行（同键互斥、
    // 异键并行）；执行后二次命中检查防同键双 spawn。
    std::shared_ptr<QMutex> keyMutex;
    {
        QMutexLocker lock(&ctx.snapshot->mutex);
        auto it = ctx.snapshot->toolOutputs.constFind(key);
        if (it != ctx.snapshot->toolOutputs.cend()) return it.value();
        keyMutex = ctx.snapshot->toolMutexes.value(key);
        if (!keyMutex) {
            keyMutex = std::make_shared<QMutex>();
            ctx.snapshot->toolMutexes.insert(key, keyMutex);
        }
    }
    // 5WHY (复核 2026-08-21 取消先行): 取消检查置于键锁获取之前——已取消
    // 探针不得为等锁再付持有者的完整超时（QThreadPool 槽位/整轮收尾延迟）。
    if (ctx.cancelled.load()) return QString();
    QMutexLocker keyLock(keyMutex.get());
    // 等锁期间可能已被取消——先响应取消再执行（同键互斥本意是去重，
    // 不是为已取消探针继续付外部命令延迟）。
    if (ctx.cancelled.load()) return QString();
    {
        QMutexLocker lock(&ctx.snapshot->mutex);
        auto it = ctx.snapshot->toolOutputs.constFind(key);
        if (it != ctx.snapshot->toolOutputs.cend()) return it.value();
    }
    const QString out = runOnce();
    // 5WHY (复核 2026-08-21 失败限次重试): 曾失败（超时/空输出）不入缓存
    // ——同键 k 个消费方对持续失败命令 k 次串行付满超时（nmcli 4s ×
    // 每消费方，"下一调用方重试一次"的承诺实为每消费方重试），且同轮
    // 首败次成令两个复用者快照不一致。空 = 失败按键计数：第 2 次失败后
    // 入缓存（每键每轮至多执行 2 次 = 首探 + 一次重试），其后消费方
    // 即时复用失败空串回退（旧行为；互斥保证同键仍只串行执行）。
    bool cacheIt = !out.isEmpty();
    if (!cacheIt) {
        QMutexLocker lock(&ctx.snapshot->mutex);
        const int attempts = ctx.snapshot->toolAttempts.value(key, 0) + 1;
        ctx.snapshot->toolAttempts.insert(key, attempts);
        cacheIt = attempts >= 2;
    }
    if (cacheIt) {
        QMutexLocker lock(&ctx.snapshot->mutex);
        ctx.snapshot->toolOutputs.insert(key, out);
    }
    return out;
}
#endif

// ── Cellular helpers ────────────────────────────────────────────────
static bool hasNonEmptyValue(const QVariantMap& values, const char* key) {
    auto it = values.constFind(QLatin1String(key));
    return it != values.cend() && !it->toString().trimmed().isEmpty();
}

static bool hasCellularIdentity(const QVariantMap& cell) {
    return hasNonEmptyValue(cell, "carrierName")
        || hasNonEmptyValue(cell, "radioAccess")
        || (hasNonEmptyValue(cell, "mcc") && hasNonEmptyValue(cell, "mnc"));
}

static QString cellularSummary(const QVariantMap& cell) {
    QString carrier = cell.value(QStringLiteral("carrierName")).toString().trimmed();
    QString radio = cell.value(QStringLiteral("radioAccess")).toString().trimmed();
    if (!carrier.isEmpty() && !radio.isEmpty())
        return QStringLiteral("Carrier: %1 (%2)").arg(carrier, radio);
    if (!carrier.isEmpty()) return QStringLiteral("Carrier: %1").arg(carrier);
    if (!radio.isEmpty()) return QStringLiteral("Radio: %1").arg(radio);
    return QStringLiteral("Cellular service detected");
}

// ── TCP state names ─────────────────────────────────────────────────
#if defined(_WIN32)
static const char* tcpStateName(int st) {
    switch(st){case 1:return"CLOSED";case 2:return"LISTEN";case 3:return"SYN_SENT";
    case 4:return"SYN_RCVD";case 5:return"ESTABLISHED";case 6:return"FIN_WAIT1";
    case 7:return"FIN_WAIT2";case 8:return"CLOSE_WAIT";case 9:return"CLOSING";
    case 10:return"LAST_ACK";case 11:return"TIME_WAIT";case 12:return"DELETE_TCB";
    default:return"UNKNOWN";}
}
#else
static const char* tcpStateName(int st) {
    switch(st){case 1:return"ESTABLISHED";case 2:return"SYN_SENT";case 3:return"SYN_RECV";
    case 4:return"FIN_WAIT1";case 5:return"FIN_WAIT2";case 6:return"TIME_WAIT";
    case 7:return"CLOSE";case 8:return"CLOSE_WAIT";case 9:return"LAST_ACK";
    case 10:return"LISTEN";case 11:return"CLOSING";default:return"UNKNOWN";}
}
#endif

// ── Shared URL parser — eliminates 5x duplicated parse logic ─────
struct ParsedUrl { QString host; int port = 80; QString path; };
inline ParsedUrl parseHttpUrl(const QString& urlStr) {
    ParsedUrl p;
    QString u = urlStr.trimmed();
    // 5WHY: this only recognized "http://" and returned an empty host for
    // any other scheme (including https://) — a silent parse failure. All
    // current callers pass http:// speed-test URLs, but a future caller
    // passing https:// would get "Invalid URL" with no hint. Recognize both
    // and set the scheme's default port; an explicit :port still wins.
    int defaultPort = 80;
    if (u.startsWith(QLatin1String("https://"))) { defaultPort = 443; u = u.mid(8); }
    else if (u.startsWith(QLatin1String("http://"))) { u = u.mid(7); }
    else return p;
    p.port = defaultPort;
    auto slash = u.indexOf('/');
    QString hp = (slash > 0) ? u.left(slash) : u;
    p.path = (slash > 0) ? u.mid(slash) : QStringLiteral("/");
    auto colon = hp.lastIndexOf(':');
    if (colon > 0) { p.host = hp.left(colon); p.port = hp.mid(colon + 1).toInt(); }
    else { p.host = hp; }
    return p;
}

// ── Hodges-Lehmann robust location estimator ─────────────────────
// Median of all N(N+1)/2 pairwise averages.  96% Gaussian efficiency,
// 29% breakdown point.  Best all-around robust estimator for N=3-100.
inline double hodgesLehmann(const QVector<double>& v) {
    int n = v.size();
    if (n == 1) return v[0];
    int npairs = n * (n + 1) / 2;
    QVector<double> pairs; pairs.reserve(npairs);
    for (int i = 0; i < n; i++)
        for (int j = i; j < n; j++)
            pairs.append((v[i] + v[j]) / 2.0);
    std::sort(pairs.begin(), pairs.end());
    return (npairs % 2 == 1) ? pairs[npairs / 2]
           : (pairs[npairs / 2 - 1] + pairs[npairs / 2]) / 2.0;
}

// ── Generic median ───────────────────────────────────────────────────
inline double median(QVector<double> v) {
    int n = v.size();
    if (n == 0) return 0.0;
    if (n == 1) return v[0];
    std::sort(v.begin(), v.end());
    return (n % 2 == 1) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

// Forward declarations (defined in GCommon.cpp, non-static — shared across TUs)
int      tcpPingMs(const QString& host, int port);
struct SpeedResult { double mbps; int bytes; int durationMs; bool ok; QString error; };
SpeedResult httpDownload(const QString& urlStr, int targetBytes, int timeoutMs);
SpeedResult httpUpload(const QString& urlStr, int targetBytes, int timeoutMs);

// HTTPS GET — uses QNetworkAccessManager for TLS.  Returns response body.
// Synchronous (local QEventLoop).  Used by G3GeoIPLoc for GeoIP providers.
QByteArray httpsGet(const QString& url, int timeoutMs = 5000);

// ── DoH DNS records ────────────────────────────────────────────────
struct DohDnsRecord {
    QString name;     // owner name (e.g. "www.google.com.")
    int     type = 0;  // 1=A, 5=CNAME, 2=NS, 28=AAAA
    int     ttl  = 0;
    QString data;     // IP for A/AAAA, target for CNAME/NS
};

struct DohDnsFullResult {
    QStringList     aRecords;    // A-record IPs (backward compat)
    QStringList     cnameChain;  // CNAME targets in order
    bool            hasCname = false;
    int             minTtl = -1;   // -1 = no TTL data sentinel; 0 = real TTL=0 (pollution signal)
};

// DoH (DNS-over-HTTPS) full-record query — returns A records, CNAME chain, TTL.
// Same 4-resolver majority logic as dohQuery(), but preserves record metadata.
// 5WHY: DoH timeout was 4000ms, but typical response is 50-500ms.
// 2000ms provides 4× headroom for congested networks while halving
// the worst-case wait for unreachable resolvers.
DohDnsFullResult dohQueryFull(const QString& domain,
                           const QString& type = QStringLiteral("A"), int timeoutMs = 2000);

// HTTP TTFB probe — TCP connect + HTTP GET → time to first byte (ms).
// Returns -1.0 on failure. Shared by GeoProbe and geoIPLoc.
double   httpTtfb(const QString& host, int port, const QString& path,
                  int connectTimeoutMs = 5000, int readTimeoutSec = 5);
inline double httpTtfb(const ParsedUrl& pu, int connectTimeoutMs = 5000, int readTimeoutSec = 5) {
    return httpTtfb(pu.host, pu.port, pu.path, connectTimeoutMs, readTimeoutSec);
}

// ── ISO 3166-1 country code display helpers ──────────────────────
// Converts 2-letter ISO code (e.g. "CN") → 3-letter (e.g. "CHN") for table view,
// or → full name (e.g. "China") for non-table display.
QString countryCode3(const QString& code2);
QString countryFullName(const QString& code2);

} // namespace SystemDiagnostics
