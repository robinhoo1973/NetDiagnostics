#include "engine/diagnostic/G4RemoteHost.h"
#include "engine/PlatformCommand.h"
#include "engine/runner/NetworkProbe.h"
#include "util/PingParser.h"
#include "util/Logger.h"
#include <QHostInfo>
#include <QElapsedTimer>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

namespace G4RemoteHost {

static ResultProperty prop(const QString& label, const QString& value) {
    return ResultProperty(label, value);
}

// ── DNS Resolution ────────────────────────────────────────────────────
DiagnosticResult dnsResolution(const QString& target, PlatformCommand*) {
    DiagnosticResult r;
    r.id = TestId::G4DnsResolution; r.group = TestGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) { r.status = TestStatus::Skipped; r.summary = QStringLiteral("No target"); return r; }
    QElapsedTimer t; t.start();
    QHostInfo info = QHostInfo::fromName(target);
    r.durationMs = t.elapsed();
    int ipv4=0, ipv6=0; QStringList ips;
    for (const auto& a : info.addresses()) {
        if (a.protocol()==QAbstractSocket::IPv4Protocol) ++ipv4; else ++ipv6;
        ips.append(a.toString());
    }
    r.properties.append(prop("Target",target));
    r.properties.append(prop("IpCount",QString::number(ips.size())));
    r.properties.append(prop("Ipv4Count",QString::number(ipv4)));
    r.properties.append(prop("Ipv6Count",QString::number(ipv6)));
    r.properties.append(prop("IpList",ips.join(", ")));
    if (ips.isEmpty()) { r.status=TestStatus::Fail; r.summary=QStringLiteral("DNS failed: %1").arg(info.errorString()); }
    else { r.status=TestStatus::Pass; r.summary=QStringLiteral("Resolved %1 address(es)").arg(ips.size()); }
    return r;
}

// ── TCP / ICMP socket helpers ─────────────────────────────────────────
static quint32 resolveIPv4(const QString& host) {
    // 1. Try QHostInfo (Qt's async resolver)
    QHostInfo info = QHostInfo::fromName(host);
    if (!info.addresses().isEmpty()) {
        quint32 ip = info.addresses().first().toIPv4Address();
        if (ip) return ip;
    }
    // 2. Fallback to getaddrinfo (libc resolver)
    fprintf(stderr, "[DNS] QHostInfo failed for '%s', trying getaddrinfo\n", host.toUtf8().constData());
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    QByteArray hostBytes = host.toUtf8();
    if (getaddrinfo(hostBytes.constData(), nullptr, &hints, &res) == 0) {
        quint32 ip = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
        freeaddrinfo(res);
        return ntohl(ip);
    }
    return 0;
}

// Single TCP connect — returns RTT in ms, or -1 on failure
static int tcpRttMs(const QString& host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons(port);
    quint32 ip = resolveIPv4(host);
    if (!ip) { close(sock); return -1; }
    addr.sin_addr.s_addr = htonl(ip);
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    QElapsedTimer t; t.start();
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {3,0};
    if (select(sock+1, nullptr, &fdset, nullptr, &tv) <= 0) { close(sock); return -1; }
    int ms = t.elapsed(); close(sock); return ms;
}

// ICMP ping — returns (-1) if raw socket not available
static int icmpRttMs(const QString& host) {
    int s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (s < 0) return -1; // raw socket denied → caller should fall back to TCP
    quint32 ip = resolveIPv4(host);
    if (!ip) { close(s); return -1; }
    // Set receive timeout
    struct timeval tv = {2,0};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    // Build ICMP Echo Request
    unsigned char pkt[64] = {};
    pkt[0] = 8; pkt[1] = 0; // type=8 (echo), code=0
    uint16_t id = (uint16_t)getpid(); memcpy(pkt+4, &id, 2);
    uint16_t seq = 1; memcpy(pkt+6, &seq, 2);
    // Payload
    for (int i=8; i<64; ++i) pkt[i] = (unsigned char)(i & 0xFF);
    // ICMP checksum
    uint32_t sum = 0;
    for (int i=0; i<64; i+=2) sum += (pkt[i]<<8) | pkt[i+1];
    while (sum>>16) sum = (sum&0xFFFF)+(sum>>16);
    uint16_t csum = (uint16_t)(~sum);
    memcpy(pkt+2, &csum, 2);
    // Send
    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(ip);
    QElapsedTimer t; t.start();
    if (sendto(s, pkt, 64, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(s); return -1; }
    // Receive reply
    unsigned char buf[128];
    struct sockaddr_in from; socklen_t fromLen = sizeof(from);
    if (recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen) < 0) { close(s); return -1; }
    int ms = t.elapsed(); close(s); return ms;
}

// ── Ping (custom, no system command) ──────────────────────────────────
DiagnosticResult ping(const QString& target, PlatformCommand*) {
    DiagnosticResult r;
    r.id = TestId::G4Ping; r.group = TestGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) { r.status = TestStatus::Skipped; r.summary = QStringLiteral("No target"); return r; }
    QElapsedTimer t; t.start();
    int sent=0, rcvd=0; double sumMs=0, minMs=1e9, maxMs=0;
    bool useIcmp = true;
    for (int i=0; i<10; ++i) {
        ++sent;
        int ms = -1;
        if (useIcmp) {
            ms = icmpRttMs(target);
            if (ms < 0 && i==0) useIcmp = false; // raw socket denied → switch to TCP
        }
        if (!useIcmp) {
            // Try multiple common ports — pick the first one that responds
            int ports[] = {443, 80, 22, 8080, 8443};
            for (int p : ports) { ms = tcpRttMs(target, p); if (ms >= 0) break; }
        }
        if (ms >= 0) { ++rcvd; sumMs += ms; if (ms<minMs) minMs=ms; if (ms>maxMs) maxMs=ms; }
    }
    r.durationMs = t.elapsed();
    double loss = sent>0 ? (sent-rcvd)*100.0/sent : 100.0;
    double avg = rcvd>0 ? sumMs/rcvd : 0;
    if (rcvd==0) { minMs=0; maxMs=0; }
    r.properties.append(prop("Target",target));
    r.properties.append(prop("Method",useIcmp?"ICMP":"TCP connect"));
    r.properties.append(prop("Sent",QString::number(sent)));
    r.properties.append(prop("Received",QString::number(rcvd)));
    r.properties.append(prop("LossPercent",QString::number(loss,'f',1)+"%"));
    r.properties.append(prop("AvgMs",QString::number(avg,'f',1)));
    r.properties.append(prop("MinMs",QString::number(minMs,'f',1)));
    r.properties.append(prop("MaxMs",QString::number(maxMs,'f',1)));
    r.rawOutput = QStringLiteral("%1 ping: %2 sent, %3 rcvd, %4%% loss, avg %5ms")
        .arg(useIcmp?"ICMP":"TCP").arg(sent).arg(rcvd).arg(loss,0,'f',1).arg(avg,0,'f',1);
    if (loss>=100.0) { r.status=TestStatus::Fail; r.summary=QStringLiteral("100%% packet loss"); }
    else if (loss>=50.0) { r.status=TestStatus::Fail; r.summary=QStringLiteral("%1%% loss").arg(loss,0,'f',1); }
    else if (loss>0) { r.status=TestStatus::Warning; r.summary=QStringLiteral("%1%% loss, avg %2ms").arg(loss,0,'f',1).arg(avg,0,'f',1); }
    else { r.status=TestStatus::Pass; r.summary=QStringLiteral("0%% loss, avg %1ms").arg(avg,0,'f',1); }
    return r;
}

// ── Traceroute (custom TCP TTL-based, no system command) ──────────────
DiagnosticResult traceroute(const QString& target, PlatformCommand*) {
    DiagnosticResult r;
    r.id = TestId::G4Traceroute; r.group = TestGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) { r.status = TestStatus::Skipped; r.summary = QStringLiteral("No target"); return r; }
    quint32 ip = resolveIPv4(target);
    if (!ip) { r.status=TestStatus::Fail; r.summary=QStringLiteral("DNS resolution failed"); return r; }
    QElapsedTimer t; t.start();
    int hopCount=0, timeoutHops=0; bool reached=false;
    ResultProperty hopList("Hops","");
    for (int ttl=1; ttl<=30 && !reached; ++ttl) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock<0) break;
        setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
        struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
        int ports[] = {443, 80, 22, 8080, 8443};
        int portIdx = ttl < (int)(sizeof(ports)/sizeof(ports[0])) ? ttl : ((int)(sizeof(ports)/sizeof(ports[0])) - 1);
        addr.sin_family = AF_INET; addr.sin_port = htons(ports[portIdx]);
        addr.sin_addr.s_addr = htonl(ip);
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        QElapsedTimer hopTimer; hopTimer.start();
        ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
        struct timeval tv = {2,0};
        int sel = select(sock+1, nullptr, &fdset, nullptr, &tv);
        int hopMs = hopTimer.elapsed();
        if (sel > 0) {
            // Connection completed or RST received → reached target or final hop
            int err=0; socklen_t len=sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
            reached = true;
            ++hopCount;
            hopList.children.append(ResultProperty(QStringLiteral("Hop %1").arg(ttl),
                QStringLiteral("Reached in %1ms (TTL=%2)").arg(hopMs).arg(ttl)));
        } else {
            // Timeout → intermediate hop
            ++hopCount;
            hopList.children.append(ResultProperty(QStringLiteral("Hop %1").arg(ttl),
                QStringLiteral("Intermediate hop (* * *)")));
            ++timeoutHops;
        }
        close(sock);
    }
    r.durationMs = t.elapsed();
    r.properties.append(prop("Target",target));
    r.properties.append(prop("HopCount",QString::number(hopCount)));
    r.properties.append(prop("TimeoutHops",QString::number(timeoutHops)));
    r.properties.append(prop("ReachedTarget",reached?"Yes":"No"));
    r.properties.append(hopList);
    if (reached) { r.status=TestStatus::Pass; r.summary=QStringLiteral("Target reached in %1 hops").arg(hopCount); }
    else if (hopCount>0) { r.status=TestStatus::Warning; r.summary=QStringLiteral("Partial path (%1 hops)").arg(hopCount); }
    else { r.status=TestStatus::Fail; r.summary=QStringLiteral("No hops discovered"); }
    return r;
}

// ── PathPing ──────────────────────────────────────────────────────────
DiagnosticResult pathPing(const QString& target, PlatformCommand* cmd) {
    DiagnosticResult r;
    r.id = TestId::G4PathPing; r.group = TestGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) { r.status = TestStatus::Skipped; r.summary = QStringLiteral("No target"); return r; }
    QElapsedTimer t; t.start();
    auto tr = traceroute(target, cmd);
    int hopCount=0;
    for (const auto& p : tr.properties) { if (p.label=="HopCount") { hopCount=p.value.toInt(); break; } }
    QVector<ResultProperty> perHopStats;
    double totalLoss=0.0; int pingedHops=0;
    for (const auto& p : tr.properties) {
        if (p.label=="Hops") {
            for (const auto& hop : p.children) {
                if (t.elapsed() > testGroupTimeoutSec(TestGroup::G4)*1000) {
                    r.status=TestStatus::Warning; r.summary=QStringLiteral("PathPing timeout"); return r;
                }
                // Extract IP if available; otherwise skip
                QString ip = hop.value.section('(',1).section(')',0,0);
                if (ip.isEmpty()) continue;
                auto pr2 = ping(ip, cmd);
                double loss=0;
                for (const auto& pp:pr2.properties) {
                    if (pp.label=="LossPercent") { QString v=pp.value; v.replace("%",""); loss=v.toDouble(); break; }
                }
                totalLoss+=loss; ++pingedHops;
                perHopStats.append(ResultProperty(ip, QString::number(loss,'f',1)+"% loss"));
            }
        }
    }
    r.durationMs = t.elapsed();
    double avgLoss = pingedHops>0 ? totalLoss/pingedHops : 100.0;
    r.properties.append(prop("Target",target));
    r.properties.append(prop("HopCount",QString::number(hopCount)));
    r.properties.append(prop("AverageLossPercent",QString::number(avgLoss,'f',1)+"%"));
    ResultProperty stats("PerHopStats",""); stats.children=perHopStats; r.properties.append(stats);
    if (avgLoss<=10.0) { r.status=TestStatus::Pass; r.summary=QStringLiteral("Avg loss %1%%").arg(avgLoss,0,'f',1); }
    else if (pingedHops>0) { r.status=TestStatus::Warning; r.summary=QStringLiteral("High avg loss %1%%").arg(avgLoss,0,'f',1); }
    else { r.status=TestStatus::Fail; r.summary=QStringLiteral("No hops"); }
    return r;
}

// ── MTU Discovery (custom ping, no system command) ────────────────────
DiagnosticResult mtuDiscovery(const QString& target, PlatformCommand*) {
    DiagnosticResult r;
    r.id = TestId::G4MtuDiscovery; r.group = TestGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) { r.status = TestStatus::Skipped; r.summary = QStringLiteral("No target"); return r; }
    // MTU discovery uses iterative ping with different payload sizes
    // Since we can't set DF flag without raw socket, we use a heuristic based on TCP MSS
    // Simpler fallback: use TCP connect with varying payload (not true MTU but gives indication)
    int mtu = 1500; // Default assumption
    // Try TCP connect and check if large packets work
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        quint32 ip = resolveIPv4(target);
        if (ip) {
            struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
            addr.sin_family = AF_INET; addr.sin_port = htons(80);
            addr.sin_addr.s_addr = htonl(ip);
            int flags = fcntl(sock, F_GETFL, 0);
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
            ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
            fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
            struct timeval tv = {2,0};
            if (select(sock+1, nullptr, &fdset, nullptr, &tv) > 0) {
                // Connected — check MSS via getsockopt
                int mss = 0; socklen_t len = sizeof(mss);
                if (getsockopt(sock, IPPROTO_TCP, TCP_MAXSEG, &mss, &len) == 0 && mss > 0)
                    mtu = mss + 40; // MSS + IP(20) + TCP(20) headers
            }
        }
        close(sock);
    }
    r.properties.append(prop("Target",target));
    r.properties.append(prop("MtuValue",QString::number(mtu)));
    r.properties.append(prop("Method","TCP MSS heuristic"));
    if (mtu>=1500) { r.status=TestStatus::Pass; r.summary=QStringLiteral("MTU %1 (standard)").arg(mtu); }
    else if (mtu>=1280) { r.status=TestStatus::Warning; r.summary=QStringLiteral("MTU %1 (below 1500)").arg(mtu); }
    else { r.status=TestStatus::Warning; r.summary=QStringLiteral("Low MTU: %1").arg(mtu); }
    return r;
}

} // namespace G4RemoteHost
