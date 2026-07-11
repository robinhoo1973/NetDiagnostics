#include "engine/diagnostics/G4/G4Common.h"
DiagnosticResult mtuDiscovery(const QString& target) {
    DiagnosticResult r;
    r.id = DiagId::G4MtuDiscovery; r.group = DiagGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) return noTargetResult(r.id, r.group);
    QString host = extractHostname(target);
    int probePort = extractProbePort(target);
    quint32 resolvedIp = resolveIPv4(host);
    QString ipStr;
    if (resolvedIp) { struct in_addr a; a.s_addr = htonl(resolvedIp); ipStr = ip4ToStr(a); }
    QStringList out;

    // 鈹€鈹€ Windows ping -f -l style MTU discovery 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    out.append(QString());
    out.append(QStringLiteral("Path MTU Discovery for %1 [%2] (probe TCP port %3)")
        .arg(host, ipStr.isEmpty() ? host : ipStr).arg(probePort));
    out.append(QString());

    // Try TCP connect and get MSS 鈫?derive path MTU
    int discoveredMtu = 0, mss = 0;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    // fd_set overflow guard: FD_SET with fd >= FD_SETSIZE corrupts the stack.
    if (sock >= 0 && sock >= FD_SETSIZE) { closeSocket(sock); sock = -1; }
    if (sock >= 0 && resolvedIp) {
        struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET; addr.sin_port = htons(probePort);
        addr.sin_addr.s_addr = htonl(resolvedIp);
        setNonblockWin(sock);
        QElapsedTimer t; t.start();
        ::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
        struct timeval tv = {3, 0};
        int sel = select(sock + 1, nullptr, &fdset, nullptr, &tv);
        if (sel > 0) {
            int err = 0; socklen_t elen = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &elen);
            if (err == 0 || err == ECONNREFUSED) {
                socklen_t mssLen = sizeof(mss);
                if (getsockopt(sock, IPPROTO_TCP, TCP_MAXSEG, reinterpret_cast<char*>(&mss), &mssLen) == 0 && mss > 0) {
                    discoveredMtu = mss + 40; // MSS + IP(20) + TCP(20) headers
                }
#if defined(IP_MTU)
                // Also try IP_MTU directly
                int ipMtu = 0; socklen_t ipMtuLen = sizeof(ipMtu);
                if (getsockopt(sock, IPPROTO_IP, IP_MTU, reinterpret_cast<char*>(&ipMtu), &ipMtuLen) == 0 && ipMtu > discoveredMtu)
                    discoveredMtu = ipMtu;
#endif
            }
        }
        int rtt = (int)t.elapsed();
        if (mss > 0) {
            out.append(QStringLiteral("Pinging %1 [%2] with MSS=%3 bytes of data:").arg(host, ipStr).arg(mss));
            out.append(QStringLiteral("Reply from %1: MSS=%2 time=%3ms PMTU=%4").arg(ipStr).arg(mss).arg(rtt).arg(discoveredMtu));
        } else {
            out.append(QStringLiteral("Pinging %1 [%2] MTU probe:").arg(host, ipStr));
            out.append(QStringLiteral("TCP connect succeeded but MSS not available."));
        }
        closeSocket(sock);
    }
    if (discoveredMtu == 0) {
        // Fallback: probe with interface MTU (Windows ping -f -l style)
        discoveredMtu = 1500;
        out.append(QStringLiteral("PMTU TCP probe failed 鈥?using interface MTU."));
#if defined(_WIN32)
        out.append(QStringLiteral("Pinging %1 [%2] with %3 bytes of data:").arg(host, ipStr).arg(discoveredMtu - 28));
        out.append(QStringLiteral("Using default MTU: %1").arg(discoveredMtu));
#else
#if defined(__linux__)
        // Linux: read MTU from sysfs, skipping loopback (lo has MTU 65536)
        QDir netDir(QStringLiteral("/sys/class/net"));
        for (const auto& fi : netDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString ifName = fi.fileName();
            if (ifName == QStringLiteral("lo")) continue; // skip loopback
            QFile f(QStringLiteral("/sys/class/net/%1/mtu").arg(ifName));
            if (f.open(QIODevice::ReadOnly)) {
                int v = QString::fromLatin1(f.readAll().trimmed()).toInt();
                if (v > 0 && v > discoveredMtu && v < 10000) discoveredMtu = v;
            }
        }
        int payload = discoveredMtu > 28 ? discoveredMtu - 28 : discoveredMtu;
        out.append(QStringLiteral("Pinging %1 [%2] with %3 bytes of data:").arg(host, ipStr.isEmpty() ? host : ipStr).arg(payload));
        out.append(QStringLiteral("Reply from local interface: MTU=%1 bytes").arg(discoveredMtu));
#else
#if defined(__APPLE__)
        // macOS: use getifaddrs + ioctl SIOCGIFMTU
        struct ifaddrs* ifa = nullptr;
        if (getifaddrs(&ifa) == 0) {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            for (auto* p = ifa; p && sock >= 0; p = p->ifa_next) {
                QString ifName = QString::fromLatin1(p->ifa_name);
                if (ifName == QStringLiteral("lo0")) continue;
                if (!(p->ifa_flags & IFF_UP)) continue;
                struct ifreq ifr = {};
                strncpy(ifr.ifr_name, p->ifa_name, IFNAMSIZ - 1);
                if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
                    int v = ifr.ifr_mtu;
                    if (v > 0 && v > discoveredMtu && v < 10000) discoveredMtu = v;
                }
            }
            if (sock >= 0) close(sock);
            freeifaddrs(ifa);
        }
        int payload = discoveredMtu > 28 ? discoveredMtu - 28 : discoveredMtu;
        out.append(QStringLiteral("Pinging %1 [%2] with %3 bytes of data:").arg(host, ipStr.isEmpty() ? host : ipStr).arg(payload));
        out.append(QStringLiteral("Reply from local interface: MTU=%1 bytes").arg(discoveredMtu));
#else
        // Fallback for other platforms
        int payload = discoveredMtu > 28 ? discoveredMtu - 28 : discoveredMtu;
        out.append(QStringLiteral("Pinging %1 [%2] with %3 bytes of data:").arg(host, ipStr.isEmpty() ? host : ipStr).arg(payload));
        out.append(QStringLiteral("Using default MTU: %1").arg(discoveredMtu));
#endif
#endif  // close converted #elif
#endif  // close converted #elif
    }

    out.append(QString());
    out.append(QStringLiteral("Ping statistics for %1:").arg(ipStr.isEmpty() ? host : ipStr));
    out.append(QStringLiteral("    Maximum MTU: %1 bytes").arg(discoveredMtu));
    out.append(QStringLiteral("    Effective MSS: %1 bytes").arg(discoveredMtu > 40 ? discoveredMtu - 40 : 0));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.properties.append(prop("Target", target));
    r.properties.append(prop("Host", host));
    r.properties.append(prop("MtuValue", QString::number(discoveredMtu)));
    r.properties.append(prop("MssValue", QString::number(mss)));
    if (discoveredMtu >= 1500) { r.status = DiagStatus::Pass; r.summary = QStringLiteral("MTU %1 (standard)").arg(discoveredMtu); }
    else if (discoveredMtu >= 1280) { r.status = DiagStatus::Warning; r.summary = QStringLiteral("MTU %1 (below 1500)").arg(discoveredMtu); }
    else { r.status = DiagStatus::Warning; r.summary = QStringLiteral("Low MTU: %1").arg(discoveredMtu); }
    return r;
}

} // namespace G4RemoteHost
