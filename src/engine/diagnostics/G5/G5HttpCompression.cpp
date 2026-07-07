#include "engine/diagnostics/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult httpCompression(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5HttpCompression, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5HttpCompression, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 15000, true);
    if (!cr.ok) return g5Result(DiagId::G5HttpCompression, cr.error, DiagStatus::Fail);
    bool compressed = false; QString enc;
    for (const auto& line : cr.lines) {
        if (line.startsWith("< ") && line.contains("content-encoding", Qt::CaseInsensitive)) {
            auto colon = line.indexOf(':');
            if (colon > 2) enc = line.mid(colon + 1).trimmed();
            compressed = true;
        }
    }
    auto r = g5Result(DiagId::G5HttpCompression,
        compressed ? QStringLiteral("Compressed: %1").arg(enc) : "Uncompressed", DiagStatus::Info);
    r.rawOutput = cr.lines.join('\n'); r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    return r;
}

}
} // namespace G5WebsiteUrl
} // namespace G5WebsiteUrl
