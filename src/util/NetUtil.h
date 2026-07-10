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
inline void closeSocket(int fd) { closesocket((SOCKET)(uintptr_t)fd); }
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
inline void closeSocket(int fd) { ::close(fd); }
#endif

#include <QString>
#include "util/DnsResolver.h"

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

// ── send() EAGAIN/WOULDBLOCK retry ──────────────────────────────────
// Returns true if the caller should select() for writability and retry.
// Replaces the 4-line #ifdef block that appears 4 times.
inline bool retrySendWouldBlock(int sock, int timeoutSec = 1) {
#if defined(_WIN32)
    if (WSAGetLastError() != WSAEWOULDBLOCK) return false;
#else
    if (errno != EAGAIN && errno != EWOULDBLOCK) return false;
#endif
    fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf);
    struct timeval tv = {timeoutSec, 0};
    select(sock + 1, nullptr, &wf, nullptr, &tv);
    return true;
}

// ── hostToAddr — forward declaration (defined below, used by tcpConnect) ──
static inline bool hostToAddr(const QString& host, int port, struct sockaddr_in& addr);

// ── TCP connect with timeout (non-blocking) ───────────────────────────
// Returns connected socket fd, or -1 on failure. Eliminates ~14 lines of
// boilerplate per call site (socket, hostToAddr, nonblock, connect, select, SO_ERROR).
// Replaces identical blocks in G4RemoteHost::tcpRttMs, G4RemoteHost::tcpTraceHop,
// G1G2G3Native::httpGet, httpDownload, tcpPingMs, speedTest upload.
inline int tcpConnect(const QString& host, int port, int timeoutMs = 3000) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return -1; }
    setSocketNonBlocking(sock);
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return -1; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
    if (err != 0) { closeSocket(sock); return -1; }
    return sock;
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
