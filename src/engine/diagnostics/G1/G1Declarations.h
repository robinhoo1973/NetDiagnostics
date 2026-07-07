#include <QMap>
#include <QVariantMap>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>
#include <QProcess>
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <wlanapi.h>
#include <winhttp.h>
#include <tlhelp32.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace G1G2G3Native {

// ── G1: System & Adapters ──────────────────────────────────────────────

/// Network adapters list — output matches "ipconfig /all" adapter enumeration
DiagnosticResult networkAdapters(DiagId id);

/// NIC advanced properties — speed, duplex, MTU, driver info
/// Output matches "wmic nic get Name,Speed,MACAddress,..." format
DiagnosticResult nicAdvanced(DiagId id);

/// WiFi diagnostics — SSID, signal, channel, security
/// Output matches "netsh wlan show interfaces" format
DiagnosticResult wifiDiagnostics(DiagId id);
