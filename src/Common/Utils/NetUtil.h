// =============================================================================
// NetUtil.h — Cross-platform socket utility helpers
//
// Eliminates repetitive #ifdef _WIN32 / #else patterns for non-blocking setup,
// send() error handling, and TCP connect boilerplate across all diagnostic files.
// =============================================================================
#pragma once

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
// Use an inline function instead of a macro to avoid renaming Qt's QAbstractSocket::close()
inline void closeSocket(int fd) { if (fd >= 0) closesocket((SOCKET)(uintptr_t)fd); }
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
inline void closeSocket(int fd) { if (fd >= 0) ::close(fd); }
#endif

#include <QString>
#include "Common/Services/DnsResolver.h"

// ── Non-blocking socket setup ────────────────────────────────────────
// Replaces the 3-line #ifdef block that appears 5+ times across files.
inline bool setSocketNonBlocking(int sock) {
#if defined(_WIN32)
    u_long mode = 1; return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    return flags >= 0 && fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

// ── Create non-blocking TCP socket ────────────────────────────────────
// 5WHY: The 5-line pattern (socket + FD_SETSIZE guard + setSocketNonBlocking
// check) was duplicated verbatim across tcpConnect, tcpConnect6,
// G4MtuDiscovery::mtuDiscovery, and G4TraceHop.inl (Linux + macOS TCP
// fallbacks).  Each copy had slightly different error handling but identical
// socket-setup logic.  If the guard or non-blocking check ever needs to
// change (e.g. switching to poll() to eliminate FD_SETSIZE), a single
// decision point replaces 5 fragile copies.
static inline int createNonBlockingSocket(int domain = AF_INET) {
    int sock = socket(domain, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    // 5WHY: FD_SET writes past the fd_set array if sock >= FD_SETSIZE
    // (1024 default on Linux).  Windows: SOCKET handles are opaque values
    // (not array indices); FD_SETSIZE is 64 but SOCKET values >= 64 are
    // common and valid — the guard would incorrectly reject valid sockets.
#if !defined(_WIN32)
    if (sock >= FD_SETSIZE) { closeSocket(sock); return -1; }
#endif
    // 5WHY: If setSocketNonBlocking fails, the socket stays blocking and
    // ::connect() hangs for the OS TCP timeout (75-120s), defeating the
    // timeoutMs parameter and freezing the diagnostic thread.
    if (!setSocketNonBlocking(sock)) { closeSocket(sock); return -1; }
    return sock;
}

// ── hostToAddr — forward declaration (defined below, used by tcpConnect) ──
static inline bool hostToAddr(const QString& host, int port, struct sockaddr_in& addr);

// ── Non-blocking connect + timeout + SO_ERROR check ──────────────────
// Shared tail of tcpConnect/tcpConnect6 (7 lines each, only sockaddr type
// differs). On any failure the socket is closed and -1 returned.
static inline int finishConnect(int sock, const struct sockaddr* addr,
                                socklen_t addrLen, int timeoutMs) {
    ::connect(sock, addr, addrLen);
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return -1; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
    if (err != 0) { closeSocket(sock); return -1; }
    return sock;
}

// ── TCP connect with timeout (non-blocking) ───────────────────────────
// Returns connected socket fd, or -1 on failure. Eliminates ~14 lines of
// boilerplate per call site (socket, hostToAddr, nonblock, connect, select, SO_ERROR).
// Replaces identical blocks in G4RemoteHost::tcpRttMs, G4RemoteHost::tcpTraceHop,
// SystemDiagnostics::httpDownload, tcpPingMs, httpTtfb.
static inline int tcpConnect(const QString& host, int port, int timeoutMs = 3000) {
    int sock = createNonBlockingSocket(AF_INET);
    if (sock < 0) return -1;
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return -1; }
    return finishConnect(sock, reinterpret_cast<const struct sockaddr*>(&addr),
                         sizeof(addr), timeoutMs);
}

// ── hostToAddr — resolve hostname to sockaddr_in (shared by all socket functions)
static inline bool hostToAddr(const QString& host, int port, struct sockaddr_in& addr) {
    QString ip = DnsResolver::instance().resolve(host, 3000);
    if (ip.isEmpty()) return false;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    return inet_pton(AF_INET, ip.toUtf8().constData(), &addr.sin_addr) == 1;
}

// ── IPv6 helpers ─────────────────────────────────────────────────────

static inline bool hostToAddr6(const QString& host, int port, struct sockaddr_in6& addr) {
    QString ip = DnsResolver::instance().resolve6(host, 3000);
    if (ip.isEmpty()) return false;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    return inet_pton(AF_INET6, ip.toUtf8().constData(), &addr.sin6_addr) == 1;
}

static inline int tcpConnect6(const QString& host, int port, int timeoutMs = 3000) {
    int sock = createNonBlockingSocket(AF_INET6);
    if (sock < 0) return -1;
    struct sockaddr_in6 addr;
    if (!hostToAddr6(host, port, addr)) { closeSocket(sock); return -1; }
    return finishConnect(sock, reinterpret_cast<const struct sockaddr*>(&addr),
                         sizeof(addr), timeoutMs);
}
