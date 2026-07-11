#include "Diagnostics/Model/G5/G5Common.h"
DiagnosticResult serviceBanner(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5ServiceBanner, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid() || u.host().isEmpty())
        return g5Result(DiagId::G5ServiceBanner, "Invalid target", DiagStatus::Fail);
    int port = portForUrl(u);
    // Raw socket banner grab
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return g5Result(DiagId::G5ServiceBanner, "socket() failed", DiagStatus::Fail);
    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    char ps[16]; snprintf(ps, sizeof(ps), "%d", port);
    QByteArray hb = u.host().toUtf8();
    if (getaddrinfo(hb.constData(), ps, &hints, &res) != 0) { closeSocket(sock); return g5Result(DiagId::G5ServiceBanner, "DNS failed", DiagStatus::Fail); }
    struct sockaddr_in addr; memcpy(&addr, res->ai_addr, sizeof(addr)); freeaddrinfo(res);
    // Non-blocking connect
#if defined(_WIN32)
    u_long m=1; ioctlsocket(sock, FIONBIO, &m);
#else
    int fl = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, fl | O_NONBLOCK);
#endif
    ::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {5, 0};
    if (select(sock+1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return g5Result(DiagId::G5ServiceBanner, "Connection timeout", DiagStatus::Fail); }
    // Read banner
    tv = {2, 0}; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    char buf[4096]; QByteArray banner;
    if (select(sock+1, &fdset, nullptr, nullptr, &tv) > 0) {
        ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
        if (n > 0) banner = QByteArray(buf, (int)n);
    }
    closeSocket(sock);
    auto r = g5Result(DiagId::G5ServiceBanner,
        banner.isEmpty() ? "No banner received" : "Banner received",
        banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
    r.rawOutput = QString::fromUtf8(banner).left(500);
    return r;
}

} // namespace G5WebsiteUrl
