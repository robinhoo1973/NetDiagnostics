// =============================================================================
// G5/Adapters.cpp — G5 Protocol diagnostics (20 real probes, no libcurl)
//
// Ported from the archived G5*.cpp behavioral code into the new adapter
// structure.  HTTP family uses a self-contained raw HTTP/1.1 client over
// blocking QSslSocket (5WHY: QNetworkAccessManager+QEventLoop in worker
// threads races Qt's lazy proxy/locale init → SIGSEGV; blocking sockets
// need no event loop).  Protocol family (FTP/SSH/Email/Telnet/MySQL/
// PostgreSQL/Redis/MongoDB/LDAP/MQTT) uses blocking QTcpSocket with the
// archived handshake-verification logic (PING→PONG, CONNECT→CONNACK,
// MySQL handshake version, PG StartupMessage, LDAP bind, Mongo isMaster).
// Platforms per NEW-1: all 20 = All.
// =============================================================================

#include "Common/Services/PlatformAdapter.h"
#include "Common/Services/DnsResolver.h"
#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagNames.h"
#include "Diagnostics/Model/GHelpers.h"   // configureCurlBasics/CurlSlist 共享（5WHY 2026-09-26）
#include "Common/Utils/TargetRedaction.h"   // 5WHY (2026-08-22 P0-2): 出口脱敏——探针输出不得含 user:pass@

#if defined(PLATFORM_IOS)
// iOS NSURLSession HTTP 族（.mm 实现，全局作用域声明）
DiagnosticResult iosHttpDiagnostic(DiagId id, const QString& target);
#endif
#if defined(PLATFORM_ANDROID)
#include "Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.h"
#endif

#include <QUrl>
#include <QHostAddress>   // 5WHY (2026-08-22 P1-4): IP 字面量数值分类
#include <QTcpSocket>
#include <QSslSocket>
#include <QSslCertificate>
#include <QElapsedTimer>
// 5WHY (2026-09-26 双份收敛): httpOnceCurl/httpOnceSocket 各持一份字面量判
// 定 + DnsResolver 3s 解析块——dnsMs 语义（字面量=0 诚实无查询）与失败串须
// 手工同步。单一助手：字面量返回 true 且 ip 空；解析失败返回 false。
// （定义于 NO_CURL 守卫外——socket 兜底路径同样使用，5WHY 2026-09-26）
static bool resolveHostForProbe(const QUrl& u, QElapsedTimer* phase,
                                qint64* dnsMsOut, QString* ipOut) {
    QHostAddress literalCheck;
    if (literalCheck.setAddress(u.host())) {
        *dnsMsOut = 0;
        ipOut->clear();
        return true;
    }
    const QString ip = DnsResolver::instance().resolve(u.host(), 3000);
    *dnsMsOut = phase->restart();
    *ipOut = ip;
    return !ip.isEmpty();
}

// 5WHY (2026-09-27 响应体上限): 目标 URL 用户自由粘贴可指向任意大文件——
// 曾无上限累积（100MB/s×15s ≈ 1.5GB 量级），诊断工具反被内存反噬（iOS
// 尤敏感）。8MB 上限：探测只需头部+预览；达量即中止（curl 路径 CURLE_
// WRITE_ERROR 视为成功），capped 标记落 verboseLines 披露。
// 5WHY (2026-09-27 NO_CURL 守卫): 曾声明于 !NO_CURL 守卫内却供守卫外的
// httpOnceSocket 消费——NO_CURL 构建编译失败（master CI 实红）。文件级
// 常量，双路径共用。
static constexpr int kHttpBodyCap = 8 * 1024 * 1024;

#if !defined(NO_CURL)
#include <curl/curl.h>   // 5WHY (2026-09-26 铁律): 桌面 HTTP 走成熟 curl easy API
#endif
#include <QDateTime>
#include <QHostInfo>
#include <QSet>

#include <functional>
#include <cstring>

using namespace PlatformFlag;

namespace g5 {

// ── Result helpers (errorOutput auto-population — archive contract) ────────
static DiagnosticResult makeResult(DiagId id, DiagStatus status,
                                   const QString& summary,
                                   const QVector<ResultProperty>& props,
                                   const QString& details) {
    // 5WHY (2026-09-26 单一工厂): 委托 DiagnosticResult::makeResult——五份副本
    // 曾漂移（G1/G2/G3 不回填 errorOutput -> 失败结果错误区空白）。统一契约。
    return DiagnosticResult::makeResult(id, status, summary, props, details);
}

static DiagnosticResult skippedProbe(DiagId id, const QString& reason) {
    return makeResult(id, DiagStatus::Skipped, reason, {}, {});
}

// ── URL normalization + default ports (G5WebsiteUrl.h contract) ────────────
static QUrl normalizeUrl(const QString& target) {
    QString t = target.trimmed();
    if (t.isEmpty()) return {};
    if (!t.contains(QLatin1String("://"))) {
        // bare host / host:port → https
        t = QStringLiteral("https://") + t;
    }
    QUrl u(t);
    return u.isValid() && !u.host().isEmpty() ? u : QUrl();
}

// 5WHY (2026-09-26 前导收敛): 21 个探针曾各抄 4 行前导（空目标/无效 URL 门）
// 且已漂移——G5UrlParsing 说 "Invalid URL" 其余 19 处 "Invalid target"，
// 同一坏目标两种文案。单一助手统一契约；失败结果经 out 参数返回。
// （定义于 normalizeUrl 之后——先声明后使用。）
static bool tryNormalizeTarget(DiagId id, const QString& target, QUrl* out,
                               DiagnosticResult* fail) {
    if (target.isEmpty()) {
        *fail = skippedProbe(id, QStringLiteral("No target"));
        return false;
    }
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) {
        *fail = makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
        return false;
    }
    *out = u;
    return true;
}

// 5WHY (2026-09-26 表收敛): 曾本地 22-scheme 表与 G4/parseHttpUrl 漂移——
// 委托 SystemDiagnostics::defaultPortForScheme 单一表。
// （rdp/mssql 虽不在下拉列表，粘贴 "rdp://host" 仍经 wildcard 适配器执行，
// 保留端口映射：粘贴路径的探测保持正确端口。）
static int defaultPort(const QString& schemeIn) {
    return SystemDiagnostics::defaultPortForScheme(schemeIn);
}

static int portForUrl(const QUrl& u) {
    return u.port() > 0 ? u.port() : defaultPort(u.scheme());
}

// ── Shared blocking TCP probe ──────────────────────────────────────────────
struct ProbeOutcome {
    bool connected = false;
    QByteArray banner;
    qint64 latencyMs = 0;
    qint64 wallMs = 0;      // 墙钟耗时（5WHY 2026-09-27 v2 计时分离复核：latencyMs 改首字节语义后，durationMs 不再等于墙钟——排空捕获的尾部时间不属延迟）
    QString error;
    bool bannerTruncated = false;   // 5WHY 2026-09-27 v5 截断披露
};

// ctx 可为空（无 RunContext 的辅助调用场景）；非空时每个阻塞原语前置取消
// 检查——取消后立即放弃（不再等满 connect/read 超时），套件排空不再付全
// 块延迟（5WHY 2026-09-26 原子时限：探针必须咨询取消通道）。
static bool cancelledBy(const RunContext* ctx) {
    return ctx && ctx->cancelled.load(std::memory_order_relaxed);
}

// readBanner=false：纯连接测量（G5TcpConnect）——跳过读相位，latencyMs 不含
// waitForReadyRead 的空转窗口（5WHY 2026-09-27：曾恒进读相位，无 send 时
// 静默烧 ~300ms，连接延迟指标系统性虚高）。
// 5WHY (2026-09-27 v5 上限全覆盖): 曾仅守护 sendData 为空路径——带请求的
// 探针（PING/LDAP/MQTT 等 5 处）读循环仍可无界累积（病理流同类冻结）。
// 硬上限应用到全部读循环：有界读（检查先于 append），超额披露截断标记。
static constexpr qint64 kBannerCapBytes = 64 * 1024;

static ProbeOutcome tcpProbe(RunContext* ctx, const QUrl& u, const QByteArray& sendData,
                             int connectTimeoutMs = 5000, int readTimeoutMs = 3000,
                             bool readBanner = true) {
    ProbeOutcome p;
    qint64 firstDataMs = -1;   // 首字节时延（5WHY 2026-09-27 v2 计时分离）
    qint64 connectMs = 0;           // 连接时延（静默服务器回退，5WHY v5）
    const int port = portForUrl(u);
    const QString scheme = u.scheme().toLower();
    // 5WHY (review 2026-08-17): 旧启发式 "以 s 结尾即隐式 TLS" 把 sftp（SSH
    // 家族，端口 22）误当 FTPS——对 SSH 服务器做 TLS 握手必然超时失败，
    // 健康的 SFTP 目标永远报 handshake 错误。改为显式白名单；sftp 走 SSH
    // banner 路径（probeSsh）。
    static const QSet<QString> implicitTlsSchemes = {
        QStringLiteral("ftps"), QStringLiteral("smtps"), QStringLiteral("imaps"),
        QStringLiteral("pop3s"), QStringLiteral("mqtts"), QStringLiteral("ldaps")
    };
    const bool useTls = implicitTlsSchemes.contains(scheme);
    QElapsedTimer t; t.start();
    if (cancelledBy(ctx)) { p.error = QStringLiteral("Cancelled"); p.latencyMs = 0; return p; }

    if (useTls) {
        QSslSocket sock;
        sock.setPeerVerifyMode(QSslSocket::VerifyNone);
        sock.connectToHostEncrypted(u.host(), (quint16)port);
        if (!sock.waitForEncrypted(connectTimeoutMs)) {
            p.error = cancelledBy(ctx) ? QStringLiteral("Cancelled") : sock.errorString();
            p.latencyMs = t.elapsed();
                    return p;
        }
        connectMs = t.elapsed();   // 连接+握手完成时点（5WHY v5）
        if (!sendData.isEmpty()) { sock.write(sendData); sock.waitForBytesWritten(2000); }
        const qint64 deadline = t.elapsed() + readTimeoutMs;
        QByteArray data;
        while (readBanner && t.elapsed() < deadline && !cancelledBy(ctx)) {
            if (!sock.waitForReadyRead(qMin<qint64>(300, deadline - t.elapsed()))) break;
            // 5WHY (2026-09-27 v5 硬上限): 有界读且检查先于 append（曾先
            // readAll 后查——软上限，火管流超量一个完整 drain）；上限覆盖
            // 全部读循环（曾仅 sendData 为空路径，带请求探针仍可无界）。
            data += sock.read(kBannerCapBytes - data.size());   // read(n) 自按可用截断
            // 5WHY (2026-09-27 v2 计时分离): 首字节计时（横幅指标本义）。
            if (firstDataMs < 0 && !data.isEmpty()) firstDataMs = t.elapsed();
            if (data.size() >= kBannerCapBytes) { p.bannerTruncated = true; break; }
            // 5WHY (2026-09-27 v5.1 首读即断回归): 纯横幅（sendData 空）读到数据
            // 即完成——v5 硬上限重写曾误删此断，静默服务端下空等 ~300ms/探针
            // 注水 wallMs（评审抓获）。上限检查之后恢复。
            if (sendData.isEmpty() && !data.isEmpty()) break;
        }
        sock.disconnectFromHost();
        if (cancelledBy(ctx)) { p.error = QStringLiteral("Cancelled"); p.latencyMs = t.elapsed(); return p; }
        p.connected = true;
        p.banner = data;
        // 5WHY (2026-09-27 v5 静默服务器): 首字节未到即无数据可测——回退连接
        // 时延（曾退化到 connect+整段读期限 ~3s 冒充 "Connect"）。
        p.latencyMs = firstDataMs >= 0 ? firstDataMs
                    : connectMs > 0 ? connectMs
                    : t.elapsed();
        p.wallMs = t.elapsed();
        return p;
    }

    QTcpSocket sock;
    sock.connectToHost(u.host(), (quint16)port);
    if (!sock.waitForConnected(connectTimeoutMs)) {
        p.error = cancelledBy(ctx) ? QStringLiteral("Cancelled") : sock.errorString();
        p.latencyMs = t.elapsed();
            return p;
    }
    connectMs = t.elapsed();   // TCP 连接完成时点（5WHY v5）
    if (!sendData.isEmpty()) { sock.write(sendData); sock.waitForBytesWritten(2000); }
    const qint64 deadline = t.elapsed() + readTimeoutMs;
    QByteArray data;
    while (readBanner && t.elapsed() < deadline && !cancelledBy(ctx)) {
        if (!sock.waitForReadyRead(qMin<qint64>(300, deadline - t.elapsed()))) break;
        data += sock.read(kBannerCapBytes - data.size());
        if (firstDataMs < 0 && !data.isEmpty()) firstDataMs = t.elapsed();   // 首字节计时（5WHY 2026-09-27 v2）
        if (data.size() >= kBannerCapBytes) { p.bannerTruncated = true; break; }   // 5WHY v5 硬上限
        if (sendData.isEmpty() && !data.isEmpty()) break;   // 5WHY v5.1 首读即断回归修复
    }
    sock.disconnectFromHost();
    if (cancelledBy(ctx)) { p.error = QStringLiteral("Cancelled"); p.latencyMs = t.elapsed(); return p; }
    p.connected = true;
    p.banner = data;
    p.latencyMs = firstDataMs >= 0 ? firstDataMs
                : connectMs > 0 ? connectMs
                : t.elapsed();
    p.wallMs = t.elapsed();
    return p;
}

static DiagnosticResult probeResultScaffold(DiagId id, const QUrl& u,
                                            const ProbeOutcome& p) {
    // 5WHY (2026-09-26 取消转换下沉): tcpProbe 取消时 p.error=="Cancelled"——
    // 曾每个调用点手写 ctx 检查转 Cancelled 终态（11 处，漏一处即 Fail
    // "Cancelled" 误报）。scaffold 单一转换，调用点不再可能漏。
    if (p.error == QLatin1String("Cancelled"))
        return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    // 5WHY (2026-09-27 空结论行): Pass 曾 summary=QString()——banner 空时详情页
    // hero 无结论行且全区块静默空（违背「无条件下发终端输出」诉求）。默认
    // 结论带目标与时延；各探针后续覆盖更具体文案者不受影响。
    DiagnosticResult r = makeResult(id, p.connected ? DiagStatus::Pass : DiagStatus::Fail,
        p.connected ? QStringLiteral("Connected to %1:%2 in %3ms")
                          .arg(u.host()).arg(portForUrl(u)).arg(p.latencyMs)
                    : QStringLiteral("Connection failed"), {}, {});
    // 墙钟耗时（5WHY 2026-09-27 v2 复核）：latencyMs 已改首字节语义，探针
    // 时长仍应是排空完成时刻的墙钟——曾 durationMs=latencyMs，首字节后
    // 排空尾部被静默从时长中抹除。
    r.durationMs = p.wallMs > 0 ? p.wallMs : p.latencyMs;
    r.data[QStringLiteral("host")] = u.host();
    r.data[QStringLiteral("port")] = portForUrl(u);
    r.data[QStringLiteral("connected")] = p.connected;
    r.data[QStringLiteral("latencyMs")] = p.latencyMs;
    r.data[QStringLiteral("banner")] = QString();   // 失败结果键齐备（消费方绝不读到 undefined）
    if (p.connected && !p.banner.isEmpty()) {
        r.details = QString::fromUtf8(p.banner);
        r.rawOutput = r.details;
    }
    // 5WHY (2026-09-27 v5.1 结构化披露): 曾把截断行拼进 details——rawOutput
    // 消费面（报告/剪贴板）永远不知道截断发生过。披露是元数据：结构化字段
    // 供各呈现通道本地渲染（与 G3 无分键同门「数据与文案分离」）。
    if (p.bannerTruncated)
        r.data[QStringLiteral("bannerTruncatedBytes")] = p.banner.size();
    if (!p.connected) {
        r.errorOutput = p.error.isEmpty()
            ? QStringLiteral("Connection to %1:%2 failed").arg(u.host()).arg(portForUrl(u))
            : QStringLiteral("Connection to %1:%2 failed: %3").arg(u.host()).arg(portForUrl(u)).arg(p.error);
    }
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// Raw HTTP/1.1 client over blocking QSslSocket (no event loop, no QNAM)
// ═════════════════════════════════════════════════════════════════════════
struct HttpResult {
    bool ok = false;
    int statusCode = 0;
    QByteArray statusLine;
    QList<QPair<QByteArray, QByteArray>> headers;   // lowercase name → value
    QByteArray body;
    QString error;
    QString redirectLocation;
    qint64 dnsMs = 0, connectMs = 0, tlsMs = 0, firstByteMs = 0, totalMs = 0;
    QStringList verboseLines;   // "> request" / "< response" style
};

// 5WHY (2026-09-27 口径统一): 截断披露曾三处复制且字节口径不一——curl 报纯
// body、socket 报含 headers 的 all.size()。单一助手，统一纯 body 字节。
static void appendBodyTruncNote(HttpResult& r, int bodyBytes) {
    r.verboseLines.append(QStringLiteral("< (body truncated at %1 bytes)").arg(bodyBytes));
}



static bool parseResponseHead(const QByteArray& head, HttpResult& r) {
    // 5WHY (2026-09-05 头部名全带换行): 曾按 '\r' 切分——CRLF 头体中除状态行
    // 外每行均以 '\n' 开头，name 从未剥掉它（"\ncontent-type"）→ 所有按
    // 精确名匹配的消费全部失配：SecurityHeaders 恒报 7 项缺失、Redirect
    // 永不跟随 location、Compression 恒判未压缩、头部列表显示带内嵌换行。
    // 按 '\n' 切分并剥行尾 '\r'（value 的 trimmed 已覆盖尾部空白）。
    const QList<QByteArray> lines = head.split('\n');
    if (lines.isEmpty()) return false;
    r.statusLine = lines.first();
    // 5WHY (2026-09-05 复核): 状态行同样以 '\r' 结尾——QML Text 把裸 '\r'
    // 当换行，终端/详情页在状态行后多一条幻影空行（且污染剪贴板）。
    // 与头体行同门剥掉。
    if (r.statusLine.endsWith('\r')) r.statusLine.chop(1);
    const QList<QByteArray> parts = r.statusLine.split(' ');
    if (parts.size() < 2) return false;
    r.statusCode = parts[1].toInt();
    for (int i = 1; i < lines.size(); ++i) {
        QByteArray line = lines[i];
        if (line.endsWith('\r')) line.chop(1);
        if (line.isEmpty()) continue;
        const int colon = line.indexOf(':');
        if (colon <= 0) continue;
        QByteArray name = line.left(colon).toLower();
        QByteArray value = line.mid(colon + 1).trimmed();
        r.headers.append({name, value});
        if (name == "location")
            r.redirectLocation = QString::fromUtf8(value);
    }
    return true;
}

static QByteArray headerValue(const HttpResult& r, const char* name) {
    for (const auto& kv : r.headers)
        if (kv.first == QByteArray(name)) return kv.second;
    return {};
}

// One request (no redirects). dnsMs measured with QHostInfo for host names.
// ctx 可为空；非空时各阻塞相位前置取消检查（5WHY 2026-09-26 原子时限）。
// 5WHY (2026-09-26 铁律禁造轮子): 桌面路径改走 libcurl easy API（项目已链接
// curl 却从未使用，dependencies.cmake 注释 "curl (G5 HTTP diagnostics)" 落了
// 空）——手写 HTTP/1.1 客户端曾无 chunked 解码（body 含块长行，G5CurlVerbose/
// G5HttpCompression 对 chunked 服务器误报）、响应解析两轮 5WHY 修正、重定向
// 手跟 6 跳。curl 提供成熟解析/TLS/超时，XFERINFO 回调保留取消语义；
// NO_CURL（iOS/Android/无库桌面）回退 socket 实现（httpOnceSocket）。
#if !defined(NO_CURL)
namespace {
struct CurlCapture {
    QByteArray headers;
    QByteArray body;
    RunContext* ctx = nullptr;
    int bodyCap = kHttpBodyCap;
    bool capped = false;
};

static size_t curlHeaderCb(char* ptr, size_t size, size_t nmemb, void* ud) {
    const size_t n = size * nmemb;
    static_cast<CurlCapture*>(ud)->headers.append(ptr, (int)n);
    return n;
}
static size_t curlWriteCb(char* ptr, size_t size, size_t nmemb, void* ud) {
    auto* cap = static_cast<CurlCapture*>(ud);
    if (cap->body.size() >= cap->bodyCap) { cap->capped = true; return 0; }
    const size_t n = qMin<size_t>(size * nmemb, (size_t)(cap->bodyCap - cap->body.size()));
    cap->body.append(ptr, (int)n);
    return n;
}
// 取消回调：ctx 置位即中止传输（curl 返回 CURLE_ABORTED_BY_CALLBACK）——
// 与 socket 路径同门的取消语义（5WHY 2026-09-26 原子时限）。
static int curlProgressCb(void* ud, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    const auto* cap = static_cast<const CurlCapture*>(ud);
    return (cap->ctx && cap->ctx->cancelled.load(std::memory_order_relaxed)) ? 1 : 0;
}
} // namespace

static HttpResult httpOnceCurl(RunContext* ctx, const QUrl& u, const QByteArray& method,
                               const QByteArray& extraHeaders, int timeoutMs) {
    HttpResult r;
    QElapsedTimer total; total.start();
    QElapsedTimer phase; phase.start();
    if (cancelledBy(ctx)) { r.error = QStringLiteral("Cancelled"); r.totalMs = 0; return r; }

    // DNS 相位保留 DnsResolver 3s 有界语义（curl 自带解析无独立 3s 界）；
    // 预解析结果经 CURLOPT_RESOLVE 注入——curl 不再自行解析，相位计时干净。
    QString resolvedIp;
    if (!resolveHostForProbe(u, &phase, &r.dnsMs, &resolvedIp)) {
        r.error = QStringLiteral("DNS resolution failed (3s timeout)");
        r.totalMs = total.elapsed();
        return r;
    }
    QByteArray resolveEntry;
    if (!resolvedIp.isEmpty()) {
        const int port = portForUrl(u);
        resolveEntry = u.host().toUtf8() + ':' + QByteArray::number(port) + ':' + resolvedIp.toUtf8();
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        r.error = QStringLiteral("curl_easy_init failed");
        r.totalMs = total.elapsed();
        return r;
    }
    CurlCapture cap;
    cap.ctx = ctx;

    const QByteArray urlBytes = u.toString(QUrl::FullyEncoded).toUtf8();
    curl_easy_setopt(curl, CURLOPT_URL, urlBytes.constData());
    // 共享基线（NOSIGNAL/HTTP1.1/UA/VerifyNone/超时）——SystemDiagnostics::
    // configureCurlBasics 单一来源（5WHY 2026-09-26 复用收敛）。
    // HTTP/1.1 与 parseResponseHead 契约一致；chunked 由 curl 自动解码。
    SystemDiagnostics::configureCurlBasics(curl, 0L, static_cast<long>(timeoutMs));
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.constData());
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L); // 启用取消回调
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cap);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &cap);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cap);
    // 5WHY (2026-09-26 curl slist 生命周期): CURLOPT_RESOLVE 的 slist 必须
    // 存活至传输完成——曾 setopt 后立即 free，perform 期 curl 读已释放链表
    // → SIGSEGV。RAII CurlSlist：析构统一释放，生命周期错误不可能再写出来。
    SystemDiagnostics::CurlSlist resolveList;
    if (!resolveEntry.isEmpty())
        resolveList.list = curl_slist_append(nullptr, resolveEntry.constData());
    curl_easy_setopt(curl, CURLOPT_RESOLVE, resolveList.list);
    SystemDiagnostics::CurlSlist headerList;
    if (!extraHeaders.isEmpty()) {
        for (const QByteArray& h : extraHeaders.split('\n')) {
            const QByteArray trimmed = h.trimmed();
            if (!trimmed.isEmpty()) headerList.list = curl_slist_append(headerList.list, trimmed.constData());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList.list);
    }

    r.verboseLines.append(QStringLiteral("> %1 %2 HTTP/1.1").arg(QString::fromUtf8(method), u.path()));

    const CURLcode code = curl_easy_perform(curl);
    const bool capReached = (code == CURLE_WRITE_ERROR && cap.capped);
    if (code != CURLE_OK && !capReached) {
        r.error = cancelledBy(ctx) ? QStringLiteral("Cancelled")
                                   : QString::fromLatin1(curl_easy_strerror(code));
        r.totalMs = total.elapsed();
        curl_easy_cleanup(curl);
        return r;
    }
    if (capReached)
        appendBodyTruncNote(r, cap.body.size());
    long statusCode = 0;
    double tConnect = 0.0, tApp = 0.0, tStart = 0.0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &tConnect);
    curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &tApp);
    curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &tStart);
    curl_easy_cleanup(curl);

    // 相位语义与 socket 路径对齐：connectMs ≈ TCP 连接（预解析后），
    // tlsMs = TLS 握手相位，firstByteMs 含 DNS（与旧"从函数进入到首字节"
    // 语义一致），totalMs 为本地墙钟。
    r.connectMs = qRound64(tConnect * 1000.0);
    r.tlsMs = qRound64(qMax(0.0, tApp - tConnect) * 1000.0);
    r.firstByteMs = r.dnsMs + qRound64(tStart * 1000.0);
    r.totalMs = total.elapsed();
    r.body = cap.body;
    // 头部块（状态行 + 逐行头）——去掉结尾空行后复用既有解析器
    QByteArray head = cap.headers;
    while (head.endsWith("\r\n")) head.chop(2);
    if (head.isEmpty() || !parseResponseHead(head, r)) {
        r.error = QStringLiteral("Malformed HTTP response");
        return r;
    }
    r.statusCode = (int)statusCode;   // parseResponseHead 亦会填，此处对齐 curl 权威值
    r.verboseLines.append(QStringLiteral("< %1").arg(QString::fromLatin1(r.statusLine)));
    for (const auto& kv : r.headers)
        r.verboseLines.append(QStringLiteral("< %1: %2").arg(QString::fromLatin1(kv.first), QString::fromLatin1(kv.second)));
    r.ok = true;
    return r;
}
#endif // !NO_CURL

// NO_CURL 兜底：阻塞 QSslSocket 实现（iOS/Android 不链接 curl；桌面缺库同）
static HttpResult httpOnceSocket(RunContext* ctx, const QUrl& u, const QByteArray& method,
                                 const QByteArray& extraHeaders, int timeoutMs) {
    HttpResult r;
    QElapsedTimer total; total.start();
    QElapsedTimer phase; phase.start();
    if (cancelledBy(ctx)) { r.error = QStringLiteral("Cancelled"); r.totalMs = 0; return r; }

    // Phase: DNS (only for hostnames) — DnsResolver 3s 超时单例（H4：裸 QHostInfo
    // 在坏网络下可阻塞数十秒，远超 watchdog 预算）
    // 5WHY (2026-08-22 P1-4): 曾以 contains('.') + startsWith 前缀族判断
    // "是否 IP 字面量"——192.example.com 被 192. 前缀误伤、172.32 公网被
    // 误跳 DNS、缺 CGNAT 100.64/10 与 IPv6 ULA。QHostAddress 数值分类：
    // 真字面量 dnsMs=0（诚实——无查询发生），其余一律真实解析。
    QString resolvedIp;
    if (!resolveHostForProbe(u, &phase, &r.dnsMs, &resolvedIp)) {
        r.error = QStringLiteral("DNS resolution failed (3s timeout)");
        r.totalMs = total.elapsed();
        return r;
    }
    Q_UNUSED(resolvedIp);   // socket 路径自行连接解析；预解析仅作 3s 有界与 dnsMs 计时

    const int port = portForUrl(u);
    const bool https = u.scheme().toLower() == QLatin1String("https");

    if (https) {
        QSslSocket sock;
        sock.setPeerVerifyMode(QSslSocket::VerifyNone);
        // M6：TCP 连接与 TLS 握手分段计时——先 connectToHost，再 startClientEncryption
        sock.connectToHost(u.host(), (quint16)port);
        if (!sock.waitForConnected(timeoutMs)) {
            r.error = cancelledBy(ctx) ? QStringLiteral("Cancelled") : sock.errorString();
            r.totalMs = total.elapsed();
            return r;
        }
        r.connectMs = phase.restart();
        sock.startClientEncryption();
        if (!sock.waitForEncrypted(timeoutMs)) {
            r.error = cancelledBy(ctx) ? QStringLiteral("Cancelled") : sock.errorString();
            r.totalMs = total.elapsed();
            return r;
        }
        r.tlsMs = phase.elapsed();
        if (cancelledBy(ctx)) { r.error = QStringLiteral("Cancelled"); r.totalMs = total.elapsed(); return r; }

        QByteArray req;
        req += method + " " + (u.path(QUrl::FullyEncoded).isEmpty() ? QByteArray("/") : u.path(QUrl::FullyEncoded).toUtf8());
        if (u.hasQuery()) { req += '?'; req += u.query(QUrl::FullyEncoded).toUtf8(); }
        req += " HTTP/1.1\r\n";
        req += "Host: " + u.host().toUtf8() + "\r\n";
        req += "User-Agent: NetDiagnostics/1.0\r\n";
        req += "Connection: close\r\n";
        if (!extraHeaders.isEmpty()) req += extraHeaders;
        req += "\r\n";
        sock.write(req);
        r.verboseLines.append(QStringLiteral("> %1 %2 HTTP/1.1").arg(QString::fromUtf8(method), u.path()));
        QByteArray all;
        while (total.elapsed() < timeoutMs && !cancelledBy(ctx)) {
            if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - total.elapsed()))) break;
            all += sock.readAll();
            if (all.contains("\r\n\r\n")) { r.firstByteMs = total.elapsed(); break; }
        }
        // drain remaining body（5WHY 2026-09-27: 上限同 curl 路径——大响应不无界累积）
        while (all.size() < kHttpBodyCap && total.elapsed() < timeoutMs && !cancelledBy(ctx)) {
            if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - total.elapsed()))) break;
            all += sock.readAll();
        }
        sock.disconnectFromHost();
        r.totalMs = total.elapsed();
        if (cancelledBy(ctx)) { r.error = QStringLiteral("Cancelled"); return r; }
        const int hdrEnd = all.indexOf("\r\n\r\n");
        if (all.size() >= kHttpBodyCap && hdrEnd >= 0)
            appendBodyTruncNote(r, all.size() - (hdrEnd + 4));   // 纯 body 字节口径
        if (hdrEnd < 0) {
            r.error = QStringLiteral("No HTTP response");
            return r;
        }
        if (!parseResponseHead(all.left(hdrEnd), r)) {
            r.error = QStringLiteral("Malformed HTTP response");
            return r;
        }
        r.body = all.mid(hdrEnd + 4);
        r.verboseLines.append(QStringLiteral("< %1").arg(QString::fromLatin1(r.statusLine)));
        for (const auto& kv : r.headers)
            r.verboseLines.append(QStringLiteral("< %1: %2").arg(QString::fromLatin1(kv.first), QString::fromLatin1(kv.second)));
        r.ok = true;
        return r;
    }

    // plain HTTP
    QTcpSocket sock;
    sock.connectToHost(u.host(), (quint16)port);
    if (!sock.waitForConnected(timeoutMs)) {
        r.error = cancelledBy(ctx) ? QStringLiteral("Cancelled") : sock.errorString();
        r.totalMs = total.elapsed();
        return r;
    }
    r.connectMs = phase.elapsed();
    if (cancelledBy(ctx)) { r.error = QStringLiteral("Cancelled"); r.totalMs = total.elapsed(); return r; }
    QByteArray req;
    req += method + " " + (u.path(QUrl::FullyEncoded).isEmpty() ? QByteArray("/") : u.path(QUrl::FullyEncoded).toUtf8());
    if (u.hasQuery()) { req += '?'; req += u.query(QUrl::FullyEncoded).toUtf8(); }
    req += " HTTP/1.1\r\n";
    req += "Host: " + u.host().toUtf8() + "\r\n";
    req += "User-Agent: NetDiagnostics/1.0\r\n";
    req += "Connection: close\r\n";
    if (!extraHeaders.isEmpty()) req += extraHeaders;
    req += "\r\n";
    sock.write(req);
    r.verboseLines.append(QStringLiteral("> %1 %2 HTTP/1.1").arg(QString::fromUtf8(method), u.path()));
    QByteArray all;
    while (total.elapsed() < timeoutMs && !cancelledBy(ctx)) {
        if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - total.elapsed()))) break;
        all += sock.readAll();
        if (all.contains("\r\n\r\n")) { r.firstByteMs = total.elapsed(); break; }
    }
    while (all.size() < kHttpBodyCap && total.elapsed() < timeoutMs && !cancelledBy(ctx)) {
        if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - total.elapsed()))) break;
        all += sock.readAll();
    }
    sock.disconnectFromHost();
    r.totalMs = total.elapsed();
    if (cancelledBy(ctx)) { r.error = QStringLiteral("Cancelled"); return r; }
    const int hdrEnd = all.indexOf("\r\n\r\n");
    if (all.size() >= kHttpBodyCap && hdrEnd >= 0)
        appendBodyTruncNote(r, all.size() - (hdrEnd + 4));   // 纯 body 字节口径
    if (hdrEnd < 0) {
        r.error = QStringLiteral("No HTTP response");
        return r;
    }
    if (!parseResponseHead(all.left(hdrEnd), r)) {
        r.error = QStringLiteral("Malformed HTTP response");
        return r;
    }
    r.body = all.mid(hdrEnd + 4);
    r.verboseLines.append(QStringLiteral("< %1").arg(QString::fromLatin1(r.statusLine)));
    for (const auto& kv : r.headers)
        r.verboseLines.append(QStringLiteral("< %1: %2").arg(QString::fromLatin1(kv.first), QString::fromLatin1(kv.second)));
    r.ok = true;
    return r;
}

// One request（无重定向）。桌面 !NO_CURL → curl easy；NO_CURL → socket 兜底。
static HttpResult httpOnce(RunContext* ctx, const QUrl& u, const QByteArray& method,
                           const QByteArray& extraHeaders, int timeoutMs) {
#if !defined(NO_CURL)
    return httpOnceCurl(ctx, u, method, extraHeaders, timeoutMs);
#else
    return httpOnceSocket(ctx, u, method, extraHeaders, timeoutMs);
#endif
}

// ═════════════════════════════════════════════════════════════════════════
// G5UrlParsing
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeUrlParsing(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    const QString details = QStringLiteral("Scheme: %1\nHost: %2\nPort: %3\nPath: %4\nQuery: %5")
        .arg(u.scheme(), u.host()).arg(portForUrl(u)).arg(u.path(), u.query());
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("Scheme=%1 Host=%2 Port=%3").arg(u.scheme(), u.host()).arg(portForUrl(u)),
        {}, details);
    r.data[QStringLiteral("scheme")] = u.scheme();
    r.data[QStringLiteral("host")] = u.host();
    r.data[QStringLiteral("port")] = portForUrl(u);
    r.data[QStringLiteral("path")] = u.path();
    r.data[QStringLiteral("query")] = u.query();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5TcpConnect
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeTcpConnect(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    // 5WHY (2026-09-26 复用): 曾手写 QTcpSocket connect+waitForConnected(5000)
    // ——tcpProbe 已封装连接/时延/取消（全组唯一不响应取消的探针修复）。
    // readBanner=false：本探针语义是纯连接延迟，不读横幅（5WHY 2026-09-27）。
    const ProbeOutcome p = tcpProbe(&ctx, u, {}, 5000, 2000, false);
    if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    const bool ok = p.connected;
    const int port = portForUrl(u);
    const qint64 ms = p.latencyMs;
    // 5WHY (复核 2026-08-19 v0.0.3 对等): Host/Port 曾以属性行呈现
    // （G5TcpConnect.cpp: Host/Port）——现只存 data 键（无区块消费）。
    // 补属性行：测试目标对用户可见。
    const QVector<ResultProperty> props = {
        {QStringLiteral("Host"), u.host()},
        {QStringLiteral("Port"), QString::number(port)},
    };
    DiagnosticResult r = makeResult(id, ok ? DiagStatus::Pass : DiagStatus::Fail,
        ok ? QStringLiteral("Connected in %1ms").arg(ms)
           : QStringLiteral("Failed: %1").arg(p.error.isEmpty() ? QStringLiteral("Connection failed") : p.error), props, {});
    r.data[QStringLiteral("host")] = u.host();
    r.data[QStringLiteral("port")] = port;
    r.data[QStringLiteral("connected")] = ok;
    r.data[QStringLiteral("latencyMs")] = ms;
    if (!ok) r.errorOutput = QStringLiteral("TCP connect to %1:%2 failed: %3")
        .arg(u.host()).arg(port).arg(p.error);
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5ServiceBanner
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeServiceBanner(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    const ProbeOutcome p = tcpProbe(&ctx, u, {}, 5000, 2000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QString banner = QString::fromUtf8(p.banner).left(500);
    r.summary = p.banner.isEmpty() ? QStringLiteral("No banner received")
                                   : QStringLiteral("Banner received");
    r.status = p.banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    r.data[QStringLiteral("banner")] = banner;
    r.data[QStringLiteral("bannerLength")] = p.banner.size();
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5CurlVerbose — full request/response dump + timing waterfall
// ═════════════════════════════════════════════════════════════════════════
// 5WHY (Reuse 2026-09-05): HTTP 计时数据键 + waterfall 5 段构造曾逐字复制于
// probeCurlVerbose（diag-g5 §2.4）与 probeHttpTiming（§2.10）——契约键名
// （sslMs/waterfall 阶段）两处维护，新增阶段或改名漏一处即两测试图表
// 分叉。收敛单一助手，两个结果构建器共用。
static void attachHttpTiming(DiagnosticResult& r, const HttpResult& hr) {
    r.data[QStringLiteral("dnsMs")] = hr.dnsMs;
    r.data[QStringLiteral("connectMs")] = hr.connectMs;
    r.data[QStringLiteral("sslMs")] = hr.tlsMs;   // 契约键名 sslMs（diag-g5 §2.4/§2.10）
    r.data[QStringLiteral("firstByteMs")] = hr.firstByteMs;
    r.data[QStringLiteral("totalMs")] = hr.totalMs;
    r.data[QStringLiteral("statusCode")] = hr.statusCode;
    // 5WHY (2026-09-27 死发射删除): waterfall 数组曾构造并落 data——meta 已
    // 删 chartField 声明（verify-keymetrics.py 对账），且全仓无消费方
    // （ResultChart request 模板读标量相位键）。标量键已齐备，数组不再构造。
}
static DiagnosticResult probeCurlVerbose(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    const HttpResult hr = httpOnce(&ctx, u, QByteArrayLiteral("GET"), QByteArray(), 15000);
    if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    if (!hr.ok) return makeResult(id, DiagStatus::Fail,
        hr.error.isEmpty() ? QStringLiteral("HTTP request failed") : hr.error, {}, {});

    QStringList out;
    for (const auto& l : hr.verboseLines) out.append(l);
    out.append(QString());
    out.append(QStringLiteral("* Timing breakdown:"));
    out.append(QStringLiteral("  DNS:        %1 ms").arg(hr.dnsMs));
    out.append(QStringLiteral("  Connect:    %1 ms").arg(hr.connectMs));
    out.append(QStringLiteral("  TLS:        %1 ms").arg(hr.tlsMs));
    out.append(QStringLiteral("  First byte: %1 ms").arg(hr.firstByteMs));
    out.append(QStringLiteral("  Total:      %1 ms").arg(hr.totalMs));
    if (!hr.body.isEmpty()) {
        out.append(QString());
        out.append(QStringLiteral("* Body: %1 bytes").arg(hr.body.size()));
        const QByteArray preview = hr.body.left(500);
        for (const auto& line : QString::fromUtf8(preview).split(QLatin1Char('\n')))
            if (!line.trimmed().isEmpty()) out.append(QStringLiteral("  %1").arg(line.left(120)));
        if (hr.body.size() > 500)
            out.append(QStringLiteral("  ... (%1 more bytes)").arg(hr.body.size() - 500));
    }

    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("HTTP %1 — %2 ms").arg(hr.statusCode).arg(hr.totalMs), {}, out.join(QLatin1Char('\n')));
    attachHttpTiming(r, hr);
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5HttpHeaders
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeHttpHeaders(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    const HttpResult hr = httpOnce(&ctx, u, QByteArrayLiteral("GET"), QByteArray(), 12000);
    if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    if (!hr.ok) return makeResult(id, DiagStatus::Fail,
        hr.error.isEmpty() ? QStringLiteral("HTTP request failed") : hr.error, {}, {});
    QStringList out;
    out.append(QStringLiteral("HTTP/1.1 response headers for %1:").arg(TargetRedaction::forDisplay(u.toString())));
    out.append(QStringLiteral("  %1").arg(QString::fromLatin1(hr.statusLine)));
    for (const auto& kv : hr.headers)
        out.append(QStringLiteral("  %1: %2").arg(QString::fromLatin1(kv.first), QString::fromLatin1(kv.second)));
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("HTTP %1 — %2 headers").arg(hr.statusCode).arg(hr.headers.size()),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("statusCode")] = hr.statusCode;
    r.data[QStringLiteral("headerCount")] = hr.headers.size();
    QVariantList headers;
    for (const auto& kv : hr.headers) {
        QVariantMap m;
        m[QStringLiteral("name")] = QString::fromLatin1(kv.first);
        m[QStringLiteral("value")] = QString::fromLatin1(kv.second);
        headers.append(m);
    }
    r.data[QStringLiteral("headers")] = headers;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5SecurityHeaders
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeSecurityHeaders(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    const HttpResult hr = httpOnce(&ctx, u, QByteArrayLiteral("GET"), QByteArray(), 15000);
    if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    if (!hr.ok) return makeResult(id, DiagStatus::Fail,
        hr.error.isEmpty() ? QStringLiteral("HTTP request failed") : hr.error, {}, {});

    static const char* kRequired[] = {
        "strict-transport-security", "content-security-policy", "x-frame-options",
        "x-content-type-options", "x-xss-protection", "referrer-policy", "permissions-policy",
    };
    QStringList found, missing;
    for (const char* req : kRequired) {
        bool present = false;
        for (const auto& kv : hr.headers)
            if (kv.first == QByteArray(req)) { present = true; break; }
        if (present) found.append(QLatin1String(req));
        else missing.append(QLatin1String(req));
    }
    QStringList out;
    out.append(QStringLiteral("Security Header Analysis:"));
    out.append(QStringLiteral("  %1  %2").arg(QStringLiteral("Header"), -30).arg(QStringLiteral("Status")));
    out.append(QStringLiteral("  %1  %2").arg(QString(30, QLatin1Char('-')), QString(10, QLatin1Char('-'))));
    for (const char* req : kRequired) {
        const bool present = found.contains(QLatin1String(req));
        out.append(QStringLiteral("  %1  %2").arg(QLatin1String(req), -30)
            .arg(present ? QStringLiteral("✓ Present") : QStringLiteral("✗ Missing")));
    }
    out.append(QString());
    out.append(QStringLiteral("  Result: %1 of 7 security headers present").arg(found.size()));

    const int score = found.size();
    const DiagStatus status = missing.isEmpty() ? DiagStatus::Pass
        : missing.size() <= 4 ? DiagStatus::Warning : DiagStatus::Fail;
    DiagnosticResult r = makeResult(id, status,
        missing.isEmpty() ? QStringLiteral("All 7 present")
                          : QStringLiteral("%1 missing").arg(missing.size()),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("presentHeaders")] = found;
    r.data[QStringLiteral("missingHeaders")] = missing;
    r.data[QStringLiteral("score")] = score;
    r.data[QStringLiteral("totalRequired")] = 7;
    r.data[QStringLiteral("statusCode")] = hr.statusCode;
    // 5WHY (2026-08-23 报告样本审计 F4): Fail 只列 [MISS] 清单无任何解释与
    // 建议——诊断价值闭环 = 发现→解释→下一步（Lighthouse/SSL Labs 惯例）。
    // 补叙述：影响面 + 可执行修复路径。
    r.narrative = missing.isEmpty()
        ? QStringLiteral("All 7 recommended security response headers are present — the endpoint "
            "follows current hardening guidance.")
        : QStringLiteral("%1 of 7 recommended security headers are missing (%2). These headers mitigate "
            "clickjacking, MIME sniffing, cross-site scripting and protocol-downgrade attacks. "
            "Recommended: enable them at the web server or edge CDN — Strict-Transport-Security to "
            "enforce HTTPS, Content-Security-Policy to constrain script origins, plus X-Frame-Options, "
            "X-Content-Type-Options, Referrer-Policy and Permissions-Policy as baseline hardening.")
            .arg(missing.size()).arg(missing.join(QStringLiteral(", ")));
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5SslCertificate
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeSslCertificate(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    QSslSocket sock;
    sock.setPeerVerifyMode(QSslSocket::VerifyNone);
    QElapsedTimer t; t.start();
    sock.connectToHostEncrypted(u.host(), (quint16)portForUrl(u));
    if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    // 5WHY (2026-09-26 取消缺口说明): 握手期不可分片等待——QSslSocket 的
    // wait* 超时会把 socket 置 SocketTimeoutError 终态（实测 300ms 分片令
    // 每次 >300ms 的真实握手死亡）。单次 10s 等待 + 前后取消检查；握手期
    // 中取消的响应延迟至握手下一次 wait 返回（≤10s，已知局限）。
    if (!sock.waitForEncrypted(10000)) {
        if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        return makeResult(id, DiagStatus::Fail,
            QStringLiteral("TLS handshake failed: %1").arg(sock.errorString()), {}, {});
    }
    const auto chain = sock.peerCertificateChain();
    sock.disconnectFromHost();
    if (chain.isEmpty())
        return makeResult(id, DiagStatus::Warning, QStringLiteral("No certificate presented"), {}, {});

    QStringList out;
    int certIdx = 0;
    for (const auto& cert : chain) {
        const QString cn = cert.subjectInfo(QSslCertificate::CommonName).value(0);
        const QString issuer = cert.issuerInfo(QSslCertificate::CommonName).value(0);
        const auto sans = cert.subjectAlternativeNames().values();
        const QDateTime notBefore = cert.effectiveDate();
        const QDateTime notAfter = cert.expiryDate();
        const qint64 daysLeft = QDateTime::currentDateTime().daysTo(notAfter);
        out.append(QStringLiteral("Certificate #%1:").arg(++certIdx));
        out.append(QStringLiteral("  CN:        %1").arg(cn));
        out.append(QStringLiteral("  Issuer:    %1").arg(issuer));
        out.append(QStringLiteral("  SANs:      %1").arg(sans.join(QStringLiteral(", "))));
        out.append(QStringLiteral("  Valid:     %1 → %2").arg(notBefore.toString(Qt::ISODate), notAfter.toString(Qt::ISODate)));
        out.append(QStringLiteral("  Days left: %1").arg(daysLeft));
        out.append(QStringLiteral("  Serial:    %1").arg(QString::fromLatin1(cert.serialNumber().toHex())));
        // 5WHY (复核 2026-08-19 v0.0.3 对等): SHA-256 指纹曾以表格行呈现
        // （G5SslCertificate.cpp: Thumbprint 40 hex）——现仅存 data 键无
        // 区块消费。补入终端转储行，指纹核对场景可见。
        out.append(QStringLiteral("  SHA-256:   %1").arg(QString::fromLatin1(
            cert.digest(QCryptographicHash::Sha256).toHex())));
        out.append(QString());
        if (certIdx == 1) {
            const QSslCertificate& leaf = cert;
            const qint64 dl = QDateTime::currentDateTime().daysTo(leaf.expiryDate());
            const bool expired = dl < 0;
            const bool soonExpiring = dl >= 0 && dl <= 30;
            DiagnosticResult r = makeResult(id, expired ? DiagStatus::Fail
                : soonExpiring ? DiagStatus::Warning : DiagStatus::Pass,
                expired ? QStringLiteral("Certificate EXPIRED %1 days ago").arg(-dl)
                : soonExpiring ? QStringLiteral("Expires in %1 days").arg(dl)
                : QStringLiteral("Valid for %1 days").arg(dl), {}, out.join(QLatin1Char('\n')));
            // diag-g5 §2.7 契约键：daysLeft/issuer/validFrom/validTo/subject
            r.data[QStringLiteral("daysLeft")] = dl;
            r.data[QStringLiteral("issuer")] = issuer;
            r.data[QStringLiteral("subject")] = cn;
            r.data[QStringLiteral("validFrom")] = notBefore;
            r.data[QStringLiteral("validTo")] = notAfter;
            r.data[QStringLiteral("sans")] = sans;
            r.data[QStringLiteral("chainLength")] = chain.size();
            r.data[QStringLiteral("handshakeMs")] = t.elapsed();
            // 归档 NetworkProbe::sslCertInfo 的 SHA-256 指纹
            r.data[QStringLiteral("thumbprint")] = QString::fromLatin1(
                cert.digest(QCryptographicHash::Sha256).toHex());
            return r;
        }
    }
    return makeResult(id, DiagStatus::Warning, QStringLiteral("No leaf certificate"), {}, out.join(QLatin1Char('\n')));
}

// ═════════════════════════════════════════════════════════════════════════
// G5HttpRedirect
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeHttpRedirect(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    QStringList out;
    out.append(QStringLiteral("Redirect chain for %1:").arg(TargetRedaction::forDisplay(u.toString())));
    int redirectCount = 0;
    bool finalOk = false;
    HttpResult last;
    QVariantList hops;
    QElapsedTimer hopBudget; hopBudget.start();
    bool budgetExhausted = false;
    for (int hop = 0; hop <= 5; ++hop) {
        if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        // 5WHY (2026-09-26 预算算术修正): 曾注释 "每跳 8s（6×8=48s < 60s）"——
        // httpOnce 每跳最坏 ≈ DNS 3s + 连接 8s + TLS 8s = 19s，6 跳 ≈ 114s
        // 远超 60s watchdog，注定编造 Timeout。55s 全局预算内推进跳链：慢链
        // 提前收敛为部分结果（Warning），杜绝超档案时限（60000 与最坏一致）。
        if (hop > 0 && hopBudget.elapsed() > 55000) {
            budgetExhausted = true;
            break;
        }
        const HttpResult hr = httpOnce(&ctx, u, QByteArrayLiteral("GET"), QByteArray(), 8000);
        last = hr;
        if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        if (!hr.ok) break;
        QVariantMap hm;
        hm[QStringLiteral("url")] = TargetRedaction::forDisplay(u.toString());
        hm[QStringLiteral("statusCode")] = hr.statusCode;
        hm[QStringLiteral("location")] = hr.redirectLocation;
        hops.append(hm);
        out.append(QStringLiteral("  → %1 [HTTP %2]").arg(TargetRedaction::forDisplay(u.toString())).arg(hr.statusCode));
        if (hr.statusCode >= 300 && hr.statusCode < 400 && !hr.redirectLocation.isEmpty()) {
            out.append(QStringLiteral("    Location: %1").arg(hr.redirectLocation));
            u = u.resolved(QUrl(hr.redirectLocation));
            ++redirectCount;
            if (u.scheme().isEmpty() || u.host().isEmpty()) break;
            continue;
        }
        finalOk = (hr.statusCode >= 200 && hr.statusCode < 400);
        break;
    }
    if (!last.ok) {
        return makeResult(id, DiagStatus::Fail,
            last.error.isEmpty() ? QStringLiteral("HTTP request failed") : last.error,
            {}, out.join(QLatin1Char('\n')));
    }
    if (!finalOk) {
        out.append(budgetExhausted
            ? QStringLiteral("Chain truncated — hop budget (55s) exhausted before the final status.")
            : QStringLiteral("Final status %1 is an error").arg(last.statusCode));
        DiagnosticResult r = makeResult(id, DiagStatus::Warning,
            budgetExhausted ? QStringLiteral("%1 redirect(s), chain truncated at hop budget").arg(redirectCount)
                            : QStringLiteral("%1 redirect(s), final HTTP %2").arg(redirectCount).arg(last.statusCode),
            {}, out.join(QLatin1Char('\n')));
        r.data[QStringLiteral("redirectCount")] = redirectCount;
        r.data[QStringLiteral("redirects")] = hops;   // diag-g5 §2.8 契约键
        r.data[QStringLiteral("finalStatus")] = last.statusCode;
        return r;
    }
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        redirectCount > 0 ? QStringLiteral("%1 redirect(s), final HTTP %2").arg(redirectCount).arg(last.statusCode)
                          : QStringLiteral("No redirect — HTTP %1").arg(last.statusCode),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("redirectCount")] = redirectCount;
    r.data[QStringLiteral("redirects")] = hops;   // diag-g5 §2.8 契约键
    r.data[QStringLiteral("finalStatus")] = last.statusCode;
    r.data[QStringLiteral("finalUrl")] = TargetRedaction::forDisplay(u.toString());
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5HttpCompression
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeHttpCompression(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    // 5WHY (2026-09-26 档案时限收敛): 曾 15000×2——每请求最坏 ≈ DNS 3s + 连
    // 接 15s + TLS 15s = 33s，两次串行 66s > 60s watchdog，注定编造 Timeout。
    // 收敛到 12000：2×(3+12+12)=54s < 60s 档案；且两次请求之间响应取消。
    const HttpResult hr = httpOnce(&ctx, u, QByteArrayLiteral("GET"),
        QByteArrayLiteral("Accept-Encoding: gzip, deflate, br\r\n"), 12000);
    if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    if (!hr.ok) return makeResult(id, DiagStatus::Fail,
        hr.error.isEmpty() ? QStringLiteral("HTTP request failed") : hr.error, {}, {});
    const QByteArray encoding = headerValue(hr, "content-encoding");
    // M7：按规格补 originalSize/compressedSize/ratio——identity 对照请求测实体比
    const HttpResult hrIdentity = httpOnce(&ctx, u, QByteArrayLiteral("GET"),
        QByteArrayLiteral("Accept-Encoding: identity\r\n"), 12000);
    if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    const int originalSize = hrIdentity.ok ? hrIdentity.body.size() : 0;
    const int compressedSize = hr.body.size();
    const double ratio = (originalSize > 0)
        ? 100.0 * (1.0 - double(compressedSize) / double(originalSize)) : 0.0;
    QStringList out;
    out.append(QStringLiteral("Compression negotiation (Accept-Encoding: gzip, deflate, br):"));
    out.append(QStringLiteral("  HTTP %1").arg(hr.statusCode));
    out.append(QStringLiteral("  Content-Encoding: %1").arg(encoding.isEmpty() ? QStringLiteral("(none)") : QString::fromLatin1(encoding)));
    out.append(QStringLiteral("  Body bytes received: %1").arg(hr.body.size()));
    out.append(QStringLiteral("  Identity body bytes: %1").arg(originalSize));
    out.append(QStringLiteral("  Size ratio: %1%").arg(ratio, 0, 'f', 1));
    const bool supported = !encoding.isEmpty() && encoding != QByteArrayLiteral("identity");
    DiagnosticResult r = makeResult(id, supported ? DiagStatus::Pass : DiagStatus::Info,
        supported ? QStringLiteral("%1 enabled").arg(QString::fromLatin1(encoding))
                  : QStringLiteral("No compression negotiated"),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("contentEncoding")] = QString::fromLatin1(encoding);
    r.data[QStringLiteral("supported")] = supported;
    r.data[QStringLiteral("bodyBytes")] = hr.body.size();
    r.data[QStringLiteral("totalMs")] = hr.totalMs;
    r.data[QStringLiteral("originalSize")] = originalSize;
    r.data[QStringLiteral("compressedSize")] = compressedSize;
    r.data[QStringLiteral("ratio")] = ratio;
    // 5WHY (2026-08-23 报告样本审计 F4): "Uncompressed" 曾无影响说明——
    // 270KB 未压缩传输对用户意味着什么没有交代。补叙述：量化代价 + 修复路径。
    r.narrative = supported
        ? QStringLiteral("%1 compression is negotiated — payload transferred at %2% of the identity "
            "size (%3 KB → %4 KB).").arg(QString::fromLatin1(encoding))
              .arg(100.0 - ratio, 0, 'f', 0)
              .arg(originalSize / 1024).arg(compressedSize / 1024)
        : QStringLiteral("The server did not negotiate any compression despite an explicit "
            "Accept-Encoding offer. Text assets transfer uncompressed — expect roughly 3–5× larger "
            "payloads and proportionally slower first loads on metered or high-latency links"
            "%1. Enabling gzip or brotli at the server/CDN typically resolves this.")
            .arg(originalSize > 0
                ? QStringLiteral(" (%1 KB received here)").arg(hr.body.size() / 1024)
                : QString());
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5HttpTiming
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeHttpTiming(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    const HttpResult hr = httpOnce(&ctx, u, QByteArrayLiteral("GET"), QByteArray(), 15000);
    if (cancelledBy(&ctx)) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    if (!hr.ok) return makeResult(id, DiagStatus::Fail,
        hr.error.isEmpty() ? QStringLiteral("HTTP request failed") : hr.error, {}, {});
    QStringList out;
    out.append(QStringLiteral("HTTP timing breakdown (%1):").arg(TargetRedaction::forDisplay(u.toString())));
    out.append(QStringLiteral("  DNS lookup:   %1 ms").arg(hr.dnsMs));
    out.append(QStringLiteral("  TCP connect:  %1 ms").arg(hr.connectMs));
    out.append(QStringLiteral("  TLS handshake:%1 ms").arg(hr.tlsMs));
    out.append(QStringLiteral("  First byte:   %1 ms").arg(hr.firstByteMs));
    out.append(QStringLiteral("  Total:        %1 ms").arg(hr.totalMs));
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("TTFB %1 ms, total %2 ms").arg(hr.firstByteMs).arg(hr.totalMs),
        {}, out.join(QLatin1Char('\n')));
    attachHttpTiming(r, hr);
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// Protocol family — banner / handshake verification
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeFtp(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    if (u.scheme().toLower() != QLatin1String("ftp") && u.scheme().toLower() != QLatin1String("ftps"))
        return skippedProbe(id, QStringLiteral("Not FTP"));
    const ProbeOutcome p = tcpProbe(&ctx, u, {});
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    r.data[QStringLiteral("banner")] = banner;
    r.summary = banner.isEmpty() ? QStringLiteral("No banner") : banner;
    r.status = banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeSsh(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    if (u.scheme().toLower() != QLatin1String("ssh") && u.scheme().toLower() != QLatin1String("sftp"))
        return skippedProbe(id, QStringLiteral("Not SSH"));
    const ProbeOutcome p = tcpProbe(&ctx, u, {});
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    const QString version = banner.startsWith(QLatin1String("SSH-"))
        ? banner.section(QLatin1Char(' '), 0, 0) : QString();
    r.data[QStringLiteral("sshVersion")] = version;
    r.data[QStringLiteral("banner")] = banner;
    r.summary = version.isEmpty() ? QStringLiteral("No SSH banner") : version;
    r.status = version.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeEmail(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    const QString scheme = u.scheme().toLower();
    if (scheme != QLatin1String("smtp") && scheme != QLatin1String("imap") && scheme != QLatin1String("pop3")
        && scheme != QLatin1String("smtps") && scheme != QLatin1String("imaps") && scheme != QLatin1String("pop3s"))
        return skippedProbe(id, QStringLiteral("Not email protocol (smtp/smtps/imap/imaps/pop3/pop3s)"));
    const ProbeOutcome p = tcpProbe(&ctx, u, {});
    DiagnosticResult r = probeResultScaffold(id, u, p);
    r.data[QStringLiteral("protocol")] = scheme;
    if (!p.connected) return r;
    const QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    r.data[QStringLiteral("banner")] = banner;
    r.summary = banner.isEmpty() ? QStringLiteral("No banner") : banner;
    r.status = banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeTelnet(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    if (u.scheme().toLower() != QLatin1String("telnet"))
        return skippedProbe(id, QStringLiteral("Not Telnet"));
    const ProbeOutcome p = tcpProbe(&ctx, u, {}, 5000, 2000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    r.data[QStringLiteral("banner")] = banner;
    r.summary = banner.isEmpty() ? QStringLiteral("Connected (no banner)") : banner;
    r.status = banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeMysql(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    if (u.scheme().toLower() != QLatin1String("mysql"))
        return skippedProbe(id, QStringLiteral("Not MySQL"));
    const ProbeOutcome p = tcpProbe(&ctx, u, {}, 5000, 2000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QByteArray& data = p.banner;
    if (data.size() < 5) {
        r.summary = QStringLiteral("No handshake packet");
        r.status = DiagStatus::Warning;
        r.data[QStringLiteral("version")] = QString();
        r.data[QStringLiteral("protocolVersion")] = 0;
        if (r.errorOutput.isEmpty()) r.errorOutput = r.summary;
        return r;
    }
    const int verStart = 5;
    const int verEnd = data.indexOf('\0', verStart);
    const QString version = (verEnd > verStart)
        ? QString::fromUtf8(data.mid(verStart, verEnd - verStart)) : QString();
    r.summary = version.isEmpty() ? QStringLiteral("MySQL (version unknown)")
                                  : QStringLiteral("MySQL %1").arg(version);
    r.status = version.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    r.rawOutput = r.details = QString::fromUtf8(data.toHex(' '));
    r.data[QStringLiteral("version")] = version;
    r.data[QStringLiteral("protocolVersion")] = (int)(quint8)data.at(4);
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probePostgres(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    if (u.scheme().toLower() != QLatin1String("postgresql"))
        return skippedProbe(id, QStringLiteral("Not PostgreSQL"));
    // StartupMessage (protocol 3.0, user "diagnostic")
    QByteArray startup;
    startup.append(char(0x00)); startup.append(char(0x00));
    startup.append(char(0x03)); startup.append(char(0x00));
    startup.append("user"); startup.append('\0');
    startup.append("diagnostic"); startup.append('\0');
    startup.append('\0');
    QByteArray packet;
    const quint32 len = startup.size() + 4;
    packet.append(char((len >> 24) & 0xFF));
    packet.append(char((len >> 16) & 0xFF));
    packet.append(char((len >> 8) & 0xFF));
    packet.append(char(len & 0xFF));
    packet.append(startup);
    const ProbeOutcome p = tcpProbe(&ctx, u, packet, 5000, 3000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    // 失败结果键齐备（归档契约：消费者绝不读到 undefined key）
    r.data[QStringLiteral("responseType")] = QString();
    r.data[QStringLiteral("authOk")] = false;
    if (!p.connected) return r;
    const QByteArray& resp = p.banner;
    if (resp.isEmpty()) {
        r.summary = QStringLiteral("No response");
        r.status = DiagStatus::Warning;
        r.data[QStringLiteral("responseType")] = QString();
        r.data[QStringLiteral("authOk")] = false;
        if (r.errorOutput.isEmpty()) r.errorOutput = r.summary;
        return r;
    }
    const char type = resp.at(0);
    QString info;
    switch (type) {
        case 'R': info = QStringLiteral("Authentication request"); break;
        case 'E': info = QStringLiteral("Error response"); break;
        case 'N': info = QStringLiteral("Notice"); break;
        case 'S': info = QStringLiteral("Parameter status"); break;
        default:  info = QStringLiteral("Response type '%1'").arg(type); break;
    }
    r.summary = QStringLiteral("PostgreSQL: %1").arg(info);
    r.status = (type == 'R') ? DiagStatus::Pass : DiagStatus::Warning;
    r.rawOutput = r.details = QString::fromUtf8(resp.toHex(' '));
    r.data[QStringLiteral("responseType")] = QString(QChar(type));
    r.data[QStringLiteral("responseInfo")] = info;
    r.data[QStringLiteral("authOk")] = (type == 'R');
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeRedis(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    if (u.scheme().toLower() != QLatin1String("redis"))
        return skippedProbe(id, QStringLiteral("Not Redis"));
    const ProbeOutcome p = tcpProbe(&ctx, u, QByteArrayLiteral("PING\r\n"), 5000, 2000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QString resp = QString::fromUtf8(p.banner).trimmed();
    const bool pong = resp.contains(QLatin1String("PONG"));
    r.data[QStringLiteral("pong")] = pong;
    r.data[QStringLiteral("response")] = resp;
    r.data[QStringLiteral("banner")] = resp.left(200);
    r.summary = pong ? QStringLiteral("Redis: PONG")
        : resp.isEmpty() ? QStringLiteral("No response") : resp.left(200);
    r.status = pong ? DiagStatus::Pass : DiagStatus::Warning;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeMongodb(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    if (u.scheme().toLower() != QLatin1String("mongodb"))
        return skippedProbe(id, QStringLiteral("Not MongoDB"));
    // OP_QUERY isMaster on admin.$cmd (little-endian header)
    QByteArray bson;
    bson.append('\x13', 1);       // BSON total size = 19
    bson.append('\0', 3);
    bson.append('\x10');          // int32 type
    bson.append("isMaster");
    bson.append('\0');
    bson.append('\x01'); bson.append('\0', 3);
    bson.append('\0');
    QByteArray msg;
    auto appendLE32 = [&msg](quint32 v) {
        msg.append(char(v & 0xFF));
        msg.append(char((v >> 8) & 0xFF));
        msg.append(char((v >> 16) & 0xFF));
        msg.append(char((v >> 24) & 0xFF));
    };
    appendLE32(16 + bson.size());  // messageLength
    appendLE32(1);                 // requestID
    appendLE32(0);                 // responseTo
    appendLE32(2004);              // OP_QUERY
    msg.append('\x3f', 1);         // SlaveOk
    msg.append('\0', 3);
    msg.append("admin.$cmd");
    msg.append('\0');
    appendLE32(0);                 // numberToSkip
    appendLE32(1);                 // numberToReturn
    msg.append(bson);
    const ProbeOutcome p = tcpProbe(&ctx, u, msg, 5000, 3000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    // 失败结果键齐备（归档契约）
    r.data[QStringLiteral("responded")] = false;
    r.data[QStringLiteral("version")] = QString();
    if (!p.connected) return r;
    const QByteArray& resp = p.banner;
    const bool responded = resp.size() >= 16;
    // OP_REPLY 布局（MongoDB 线协议）：MsgHeader(16) + responseFlags(4) +
    // cursorID(8) + startingFrom(4) + numberReturned(4) = 36 字节头，首个
    // BSON 文档从偏移 36 开始。5WHY (2026-09-05 版本恒未知): 曾按 20 字节
    // 偏移读 docSize——字节 20-27 是 cursorID（isMaster 应答恒 0）→
    // docSize 恒 0，版本提取从未执行。
    QString version;
    if (resp.size() > 36) {
        const int docSize = (int)((quint8)resp[36]) | ((quint8)resp[37] << 8)
                          | ((quint8)resp[38] << 16) | ((quint8)resp[39] << 24);
        if (docSize > 0 && 36 + docSize <= resp.size()) {
            const QByteArray doc = resp.mid(36, docSize);
            const int vPos = doc.indexOf("version");
            // BSON string 元素 = type(1) + cstring 名("version\0"=8) +
            // int32 串长(4，含结尾 \0) + 内容。5WHY (2026-09-05 复核):
            // indexOf("version") 命中的是元素【名】起点而非 type 字节——
            // 串长在 vPos+8..11、内容自 vPos+12（曾按 type 字节起点计算
            // 整体偏移 1，slen 高位混入首字符字节，边界检查恒失败）。
            // 5WHY (2026-09-05 复核): indexOf("version") 可能命中其它元素
            // 字符串值内的同名子串——必须校验命中字节是 BSON string 元素
            // 的【名】起点（前 1 字节为类型 0x02），否则用值字节当 slen
            // 解析出垃圾版本串。
            if (vPos > 0 && doc[vPos - 1] == static_cast<char>(0x02)
                && vPos + 12 <= doc.size()) {
                const int slen = (int)((quint8)doc[vPos + 8]) | ((quint8)doc[vPos + 9] << 8)
                              | ((quint8)doc[vPos + 10] << 16) | ((quint8)doc[vPos + 11] << 24);
                if (slen > 1 && vPos + 12 + slen - 1 <= doc.size())
                    version = QString::fromUtf8(doc.mid(vPos + 12, slen - 1));
            }
        }
    }
    r.summary = responded ? (version.isEmpty() ? QStringLiteral("MongoDB responded (version unknown)") : QStringLiteral("MongoDB %1").arg(version))
                          : QStringLiteral("No response");
    r.status = responded ? DiagStatus::Pass : DiagStatus::Warning;
    r.rawOutput = r.details = QString::fromUtf8(resp.left(400).toHex(' '));
    r.data[QStringLiteral("responded")] = responded;
    r.data[QStringLiteral("version")] = version;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeLdap(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    const QString scheme = u.scheme().toLower();
    if (scheme != QLatin1String("ldap") && scheme != QLatin1String("ldaps"))
        return skippedProbe(id, QStringLiteral("Not LDAP(S)"));
    // Minimal anonymous BindRequest
    QByteArray ldapMsg;
    ldapMsg.append('\x30'); ldapMsg.append('\x0c');
    ldapMsg.append('\x02'); ldapMsg.append('\x01'); ldapMsg.append('\x01');
    ldapMsg.append('\x60'); ldapMsg.append('\x07');
    ldapMsg.append('\x02'); ldapMsg.append('\x01'); ldapMsg.append('\x03');
    ldapMsg.append('\x04'); ldapMsg.append('\x00');
    ldapMsg.append('\x80'); ldapMsg.append('\x00');
    const ProbeOutcome p = tcpProbe(&ctx, u, ldapMsg, 5000, 3000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    // 失败结果键齐备（归档契约）
    r.data[QStringLiteral("hasBindResp")] = false;
    r.data[QStringLiteral("bindOk")] = false;
    r.data[QStringLiteral("resultCode")] = -1;
    if (!p.connected) return r;
    const QByteArray& resp = p.banner;
    if (resp.isEmpty()) {
        r.summary = QStringLiteral("No response");
        r.status = DiagStatus::Warning;
        r.data[QStringLiteral("hasBindResp")] = false;
        r.data[QStringLiteral("bindOk")] = false;
        r.data[QStringLiteral("resultCode")] = -1;
        if (r.errorOutput.isEmpty()) r.errorOutput = r.summary;
        return r;
    }
    // BindResponse: 0x30 [len] ... 0x61 [len] 0x0a 0x01 [resultCode] ...
    // resultCode 在 0x61 标签块内（不是报文最后一个字节）。
    const int bResp = resp.indexOf('\x61');
    const bool hasBindResp = bResp >= 0 && bResp + 3 < resp.size();
    int resultCode = -1;
    if (hasBindResp) {
        const int lenByte = (int)(quint8)resp.at(bResp + 1);
        if (bResp + 2 + 2 < resp.size() && (quint8)resp.at(bResp + 2) == 0x0a
            && (quint8)resp.at(bResp + 3) == 0x01)
            resultCode = (int)(quint8)resp.at(bResp + 4);
        else
            resultCode = (int)lenByte;   // 简并情形：长度字节本身（错误包）
    }
    const bool bindOk = hasBindResp && resultCode == 0;
    r.summary = bindOk ? QStringLiteral("LDAP bind OK (resultCode 0)")
        : hasBindResp ? QStringLiteral("LDAP bind result code %1").arg(resultCode)
        : QStringLiteral("No LDAP bind response");
    r.status = bindOk ? DiagStatus::Pass : DiagStatus::Warning;
    r.rawOutput = r.details = QString::fromUtf8(resp.toHex(' '));
    r.data[QStringLiteral("hasBindResp")] = hasBindResp;
    r.data[QStringLiteral("bindOk")] = bindOk;
    r.data[QStringLiteral("resultCode")] = resultCode;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeMqtt(DiagId id, const QString& target, RunContext& ctx) {
    QUrl u;
    DiagnosticResult fail;
    if (!tryNormalizeTarget(id, target, &u, &fail)) return fail;
    const QString scheme = u.scheme().toLower();
    if (scheme != QLatin1String("mqtt") && scheme != QLatin1String("mqtts"))
        return skippedProbe(id, QStringLiteral("Not MQTT(S)"));
    // MQTT 3.1.1 CONNECT: clean session, keep-alive 60s, zero-length client id
    QByteArray connect;
    connect.append('\x10');
    // M1（5WHY）：remaining length = 12（Variable Header 10 + Payload 2），
    // 原 0x10=16 会让 broker 多等 4 字节 → CONNACK 永不返回。
    connect.append('\x0c');
    connect.append('\x00'); connect.append('\x04');
    connect.append("MQTT");
    connect.append('\x04');
    connect.append('\x02');
    connect.append('\x00'); connect.append('\x3c');
    connect.append('\x00'); connect.append('\x00');
    const ProbeOutcome p = tcpProbe(&ctx, u, connect, 5000, 3000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    // 失败结果键齐备（归档契约）
    r.data[QStringLiteral("isConnack")] = false;
    r.data[QStringLiteral("resultCode")] = -1;
    r.data[QStringLiteral("accepted")] = false;
    if (!p.connected) return r;
    const QByteArray& resp = p.banner;
    if (resp.size() < 2) {
        r.summary = QStringLiteral("No CONNACK");
        r.status = DiagStatus::Warning;
        r.data[QStringLiteral("isConnack")] = false;
        r.data[QStringLiteral("resultCode")] = -1;
        r.data[QStringLiteral("accepted")] = false;
        if (r.errorOutput.isEmpty()) r.errorOutput = r.summary;
        return r;
    }
    const bool isConnack = (quint8)resp.at(0) == 0x20;
    const quint8 retCode = resp.size() >= 4 ? (quint8)resp.at(3) : 255;
    static const char* kDesc[] = {
        "Accepted", "Protocol version refused", "Identifier rejected",
        "Server unavailable", "Bad credentials", "Not authorized",
    };
    const QString desc = (retCode <= 5) ? QLatin1String(kDesc[retCode])
                                        : QStringLiteral("Unknown code %1").arg(retCode);
    r.summary = isConnack ? QStringLiteral("MQTT CONNACK: %1").arg(desc)
                          : QStringLiteral("No CONNACK received");
    r.status = (isConnack && retCode == 0) ? DiagStatus::Pass : DiagStatus::Warning;
    r.rawOutput = r.details = QString::fromUtf8(resp.toHex(' '));
    r.data[QStringLiteral("isConnack")] = isConnack;
    r.data[QStringLiteral("resultCode")] = (int)retCode;
    r.data[QStringLiteral("returnDescription")] = desc;
    r.data[QStringLiteral("accepted")] = (isConnack && retCode == 0);
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

} // namespace g5

// ── Registration（NEW-1/NEW-2/DIAG-4：scheme 过滤唯一入口 = select()）────
// diag-g5 §2.21 映射表落库：不匹配的检测不在调度/统计/Config 出现（隐藏，
// 不计 skipped）。DB/目录/消息 6 项仅 Desktop 注册（移动端隐藏，有效 Desktop）。
void registerG5Adapters() {
    using namespace PlatformFlag;
    using F = std::function<DiagnosticResult(DiagId, const QString&, RunContext&)>;
    const auto tri = [](F fn, SchemeFilter s) {
        return QVector<PlatformAdapter>{
            {PF_Desktop, "Desktop", s, fn},
            {PF_IOS,     "iOS",     s, fn},
            {PF_Android, "Android", s, fn},
        };
    };
    const auto desktopOnly = [](F fn, SchemeFilter s) {
        return QVector<PlatformAdapter>{
            {PF_Desktop, "Desktop", s, fn},   // H2：DB/目录/消息 6 项有效平台 = Desktop
        };
    };
    const SchemeFilter wildcard;                       // include 空 = 任意 scheme
    const SchemeFilter httpSchemes{{QStringLiteral("http"), QStringLiteral("https")}, false};
    const SchemeFilter nonHttp{{QStringLiteral("http"), QStringLiteral("https")}, true};  // ServiceBanner 排除语义
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
    // 移动端 HTTP 族平台工厂（iOS NSURLSession / Android HttpURLConnection JNI）
    const auto platformHttp = [](DiagId i, const QString& t, RunContext&) -> DiagnosticResult {
#if defined(PLATFORM_IOS)
        return iosHttpDiagnostic(i, t);
#else
        return androidHttpDiag(i, t);
#endif
    };
    const auto triHttp = [&platformHttp](F, SchemeFilter s) {
        return QVector<PlatformAdapter>{
            {PF_Desktop, "Desktop", s, platformHttp},
            {PF_IOS,     "iOS",     s, platformHttp},
            {PF_Android, "Android", s, platformHttp},
        };
    };
#else
    const auto triHttp = tri;
#endif
    AdapterRegistry::registerAdapters(DiagId::G5UrlParsing,       tri(g5::probeUrlParsing, wildcard));
    AdapterRegistry::registerAdapters(DiagId::G5TcpConnect,       tri(g5::probeTcpConnect, wildcard));
    AdapterRegistry::registerAdapters(DiagId::G5ServiceBanner,    tri(g5::probeServiceBanner, nonHttp));
    AdapterRegistry::registerAdapters(DiagId::G5CurlVerbose,      triHttp(g5::probeCurlVerbose, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5HttpHeaders,      triHttp(g5::probeHttpHeaders, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5SecurityHeaders,  triHttp(g5::probeSecurityHeaders, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5SslCertificate,   tri(g5::probeSslCertificate, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5HttpRedirect,     triHttp(g5::probeHttpRedirect, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5HttpCompression,  triHttp(g5::probeHttpCompression, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5HttpTiming,       triHttp(g5::probeHttpTiming, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5FtpDiagnostics,   tri(g5::probeFtp,
        SchemeFilter{{QStringLiteral("ftp"), QStringLiteral("ftps")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5SshDiagnostics,   tri(g5::probeSsh,
        SchemeFilter{{QStringLiteral("ssh"), QStringLiteral("sftp")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5EmailDiagnostics, tri(g5::probeEmail,
        SchemeFilter{{QStringLiteral("smtp"), QStringLiteral("smtps"),
                      QStringLiteral("imap"), QStringLiteral("imaps"),
                      QStringLiteral("pop3"), QStringLiteral("pop3s")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Telnet,           tri(g5::probeTelnet,
        SchemeFilter{{QStringLiteral("telnet")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Mysql,            desktopOnly(g5::probeMysql,
        SchemeFilter{{QStringLiteral("mysql")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Postgres,         desktopOnly(g5::probePostgres,
        SchemeFilter{{QStringLiteral("postgresql")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Redis,            desktopOnly(g5::probeRedis,
        SchemeFilter{{QStringLiteral("redis")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Mongodb,          desktopOnly(g5::probeMongodb,
        SchemeFilter{{QStringLiteral("mongodb")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Ldap,             desktopOnly(g5::probeLdap,
        SchemeFilter{{QStringLiteral("ldap")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Mqtt,             desktopOnly(g5::probeMqtt,
        SchemeFilter{{QStringLiteral("mqtt")}, false}));
}
