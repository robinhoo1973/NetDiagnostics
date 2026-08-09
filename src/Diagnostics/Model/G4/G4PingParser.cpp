// =============================================================================
// PingParser.cpp
// =============================================================================
#include "Diagnostics/Model/G4/G4PingParser.h"

PingResult PingParser::parse(const QString& output) {
    // Try Unix format first (more common on Linux), then Windows
    auto r = parseUnix(output);
    if (r.valid) return r;
    r = parseWindows(output);
    if (r.valid) return r;

    // Fallback: count matching reply lines
    r.valid = true;
    r.received = countSuccessfulReplies(output);
    r.lossPercent = extractLossPercent(output);
    if (r.lossPercent < 0) r.lossPercent = (r.received > 0) ? 0.0 : 100.0;
    return r;
}

PingResult PingParser::parseUnix(const QString& output) {
    PingResult r;

    // Try: "N packets transmitted, N received, N% packet loss, time Nms"
    // Also: "N packets transmitted, N received, +N errors, N% packet loss"
    static const QRegularExpression unixRe(
        R"((\d+)\s+packets?\s+transmitted,\s*(\d+)\s+(?:packets?\s+)?received,)"
        R"((?:\+\d+\s+errors,\s*)?(\d+(?:\.\d+)?)%\s+packet\s+loss)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto m = unixRe.match(output);
    if (m.hasMatch()) {
        r.sent = m.captured(1).toInt();
        r.received = m.captured(2).toInt();
        r.lossPercent = m.captured(3).toDouble();
        r.lost = r.sent - r.received;
        r.valid = true;
    }

    // Try: "rtt min/avg/max/mdev = 1.234/5.678/9.012/1.234 ms"
    static const QRegularExpression rttRe(
        R"(rtt\s+min/avg/max/mdev\s*=\s*([\d.]+)/([\d.]+)/([\d.]+)/[\d.]+\s*ms)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto rm = rttRe.match(output);
    if (rm.hasMatch()) {
        r.minMs = rm.captured(1).toDouble();
        r.avgMs = rm.captured(2).toDouble();
        r.maxMs = rm.captured(3).toDouble();
    }

    // Fallback: find N% pattern anywhere
    if (!r.valid) {
        double loss = extractLossPercent(output);
        if (loss >= 0) {
            r.lossPercent = loss;
            r.received = countSuccessfulReplies(output);
            r.valid = true;
        }
    }

    return r;
}

PingResult PingParser::parseWindows(const QString& output) {
    PingResult r;

    // Try "Sent = N, Received = N, Lost = N (N% loss)"
    // Also locale-independent: parse numbers after "Packets:"
    static const QRegularExpression winRe(
        R"(Sent\s*=\s*(\d+).*?Received\s*=\s*(\d+).*?Lost\s*=\s*(\d+)\s*\((\d+(?:\.\d+)?)%\s*loss\))",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    auto m = winRe.match(output);
    if (m.hasMatch()) {
        r.sent = m.captured(1).toInt();
        r.received = m.captured(2).toInt();
        r.lost = m.captured(3).toInt();
        r.lossPercent = m.captured(4).toDouble();
        r.valid = true;
    }

    // Try "Minimum = Nms, Maximum = Nms, Average = Nms"
    static const QRegularExpression rttWinRe(
        R"(Minimum\s*=\s*(\d+)ms.*?Maximum\s*=\s*(\d+)ms.*?Average\s*=\s*(\d+)ms)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    auto rm = rttWinRe.match(output);
    if (rm.hasMatch()) {
        r.minMs = rm.captured(1).toDouble();
        r.maxMs = rm.captured(2).toDouble();
        r.avgMs = rm.captured(3).toDouble();
    }

    // Fallback: find locale-independent loss pattern
    if (!r.valid) {
        double loss = extractLossPercent(output);
        if (loss >= 0) {
            r.lossPercent = loss;
            r.received = countSuccessfulReplies(output);
            r.valid = true;
        }
    }

    return r;
}

double PingParser::extractLossPercent(const QString& output) {
    // Match "N% loss" or "N% packet loss" or "(N% loss)" — case insensitive, locale independent
    static const QRegularExpression re(
        R"((\d+(?:\.\d+)?)\s*%\s*(?:packet\s*)?loss)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto m = re.match(output);
    return m.hasMatch() ? m.captured(1).toDouble() : -1.0;
}

int PingParser::countSuccessfulReplies(const QString& output) {
    int count = 0;
    for (const auto& line : output.split('\n')) {
        if (line.contains("bytes from", Qt::CaseInsensitive) ||
            line.contains("time=", Qt::CaseInsensitive) ||
            line.contains("time<", Qt::CaseInsensitive) ||
            line.contains("zeit=", Qt::CaseInsensitive) ||
            line.contains("tempo=", Qt::CaseInsensitive)) {
            ++count;
        }
    }
    return count;
}
