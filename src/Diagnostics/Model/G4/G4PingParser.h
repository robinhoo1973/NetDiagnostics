// =============================================================================
// PingParser.h — Multi-locale ping/traceroute output parsing
// =============================================================================
#pragma once

#include <QString>
#include <QRegularExpression>
#include <QVector>

struct PingResult {
    int sent = 0;
    int received = 0;
    int lost = 0;
    double lossPercent = 0.0;
    double minMs = 0.0;
    double avgMs = 0.0;
    double maxMs = 0.0;
    bool valid = false;
};

struct TracerouteHop {
    int hop = 0;
    QString ip;
    double rtt1Ms = 0.0;
    double rtt2Ms = 0.0;
    double rtt3Ms = 0.0;
    bool timedOut = false;
};

struct TracerouteResult {
    QVector<TracerouteHop> hops;
    int hopCount = 0;
    int timeoutHops = 0;
    bool reachedTarget = false;
};

class PingParser {
public:
    /// Parse ping output. Handles Windows (ping -n) and Unix (ping -c) formats,
    /// English, Chinese, German, and other locales.
    static PingResult parse(const QString& output);

    /// Parse Windows tracert output.
    static TracerouteResult parseTraceroute(const QString& output);

    /// Extract loss percentage from any ping-like output.
    static double extractLossPercent(const QString& output);

    /// Count lines matching "bytes from" / "time=" patterns (fallback).
    static int countSuccessfulReplies(const QString& output);

private:
    static PingResult parseUnix(const QString& output);
    static PingResult parseWindows(const QString& output);
};
