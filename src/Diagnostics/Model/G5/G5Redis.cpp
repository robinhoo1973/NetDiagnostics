#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult redisDiagnostics(const QString& target) {
    if (target.isEmpty()) return skipped(DiagId::G5Redis, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "redis") return skipped(DiagId::G5Redis, "Not Redis");
    // PING round-trip via the shared probe (sendData = command, read = reply).
    auto p = g5Probe(u, "PING\r\n", 5000, 2000);
    auto r = g5ProbeResult(DiagId::G5Redis, u, p);
    if (!p.connected) return r;
    QString resp = QString::fromUtf8(p.banner).trimmed();
    bool pong = resp.contains("PONG");
    r.data["pong"] = pong;
    r.data["response"] = resp;
    r.data["banner"] = resp.left(200);
    r.summary = pong ? QStringLiteral("Redis: PONG")
               : resp.isEmpty() ? QStringLiteral("No response") : resp.left(200);
    r.status = pong ? DiagStatus::Pass : DiagStatus::Warning;
    if (!resp.isEmpty()) r.rawOutput = r.details = resp;
    autoErrorOutput(r);
    return r;
}
} // namespace G5WebsiteUrl
