// GBase.h — shared platform includes for G1/G2/G3 diagnostics.
#pragma once
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
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
#include "util/DnsResolver.h"
#include "util/NetUtil.h"
