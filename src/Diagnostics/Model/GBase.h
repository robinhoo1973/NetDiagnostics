// GBase.h — shared platform includes for G1/G2/G3 diagnostics.
#pragma once
#include "Common/Model/DiagnosticResult.h"
#include "Common/Model/DiagId.h"
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariantMap>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>
#include <QProcess>

#if defined(_WIN32)
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
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#if defined(PLATFORM_IOS)
#include "Diagnostics/Model/G1/Platform/IOS/GatewayDhcpRouting.h"
#endif
#include <resolv.h>          // __res_state (DNS resolver state)
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <net/if_dl.h>       // sockaddr_dl, LLADDR
#if !defined(PLATFORM_IOS)
#include <net/route.h>       // rt_msghdr, RTM_VERSION, RTAX_* (macOS only)
#endif
#else
#include <netpacket/packet.h> // sockaddr_ll, AF_PACKET (Linux)
#include <linux/wireless.h>   // iwreq, SIOCGIWESSID, IW_ESSID_MAX_SIZE (Linux)
#include <net/if_arp.h>       // ARPHRD_ETHER (Linux)
#endif
#endif
#include "Common/Services/DnsResolver.h"
#include "Common/Utils/NetUtil.h"
