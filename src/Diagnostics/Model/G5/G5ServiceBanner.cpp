#include "Diagnostics/Model/G5/G5Common.h"
DiagnosticResult serviceBanner(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5ServiceBanner, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid() || u.host().isEmpty())
        return g5Result(DiagId::G5ServiceBanner, "Invalid target", DiagStatus::Fail);
    int port = portForUrl(u);

    // 5WHY: This file used raw POSIX/Winsock sockets (socket, getaddrinfo,
    // select, recv) with platform #ifdefs — duplicating NetUtil.h helpers
    // and DnsResolver.  Replaced with NetworkProbe::tcpProbe() which
    // handles connect+read via QTcpSocket with consistent timeouts and
    // cross-platform behavior.  The sendData is empty (we just want the
    // server's greeting banner, not to send a protocol handshake).
    auto probe = NetworkProbe::tcpProbe(u.host(), port, 5000, 2000);

    if (!probe.connected) {
        auto r = g5Result(DiagId::G5ServiceBanner, "Connection failed", DiagStatus::Fail);
        r.durationMs = probe.elapsedMs;  // tcpProbe records elapsed even on failure
        return r;
    }

    auto r = g5Result(DiagId::G5ServiceBanner,
        probe.data.isEmpty() ? "No banner received" : "Banner received",
        probe.data.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
    r.rawOutput = QString::fromUtf8(probe.data).left(500);
    r.durationMs = probe.elapsedMs;
    return r;
}

} // namespace G5WebsiteUrl
