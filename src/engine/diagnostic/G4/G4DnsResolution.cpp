#include "engine/diagnostic/G4/g4common.h"
namespace G4RemoteHost {
DiagnosticResult dnsResolution(const QString& target) {
    DiagnosticResult r;
    r.id = DiagId::G4DnsResolution; r.group = DiagGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) return noTargetResult(r.id, r.group);
    QString host = extractHostname(target);
    QElapsedTimer t; t.start();
    QStringList out;
    QStringList ipsAll;

    out.append(QString());
    out.append(QStringLiteral("; <<>> NetDiagnostics DNS <<>> %1").arg(host));
    out.append(QStringLiteral(";; global options: +cmd"));
    QByteArray hb = host.toUtf8();

#ifdef _WIN32
    // ── Windows: DNSQuery for A, AAAA, MX, CNAME, SOA, NS ──────────────
    wchar_t whost[256]; MultiByteToWideChar(CP_UTF8, 0, hb.constData(), -1, whost, 256);

    struct { WORD type; const char* name; } wQueries[] = {
        {DNS_TYPE_A, "A"}, {DNS_TYPE_AAAA, "AAAA"}, {DNS_TYPE_MX, "MX"},
        {DNS_TYPE_CNAME, "CNAME"}, {DNS_TYPE_SOA, "SOA"}, {DNS_TYPE_NS, "NS"}
    };
    bool hasAnswer = false;
    for (auto& wq : wQueries) {
        PDNS_RECORD rec = nullptr;
        if (DnsQuery_W(whost, wq.type, DNS_QUERY_STANDARD, nullptr, &rec, nullptr) != 0 || !rec) continue;
        bool first = true;
        for (auto* p = rec; p; p = p->pNext) {
            if (p->wType != wq.type) continue;
            if (first) {
                out.append(QStringLiteral(";; %1 SECTION:").arg(wq.name));
                first = false; hasAnswer = true;
            }
            // Build dig-style line
            if (p->wType == DNS_TYPE_A) {
                struct in_addr a; a.S_un.S_addr = p->Data.A.IpAddress;
                QString ip = ip4ToStr(a);
                out.append(QStringLiteral("%1.  %2  IN  A  %3").arg(host, -30).arg(p->dwTtl, 6).arg(ip));
                ipsAll.append(ip);
            } else if (p->wType == DNS_TYPE_AAAA) {
                char ip6[46]; inet_ntop(AF_INET6, &p->Data.AAAA.Ip6Address, ip6, sizeof(ip6));
                out.append(QStringLiteral("%1.  %2  IN  AAAA  %3").arg(host, -30).arg(p->dwTtl, 6).arg(QString::fromLatin1(ip6)));
            } else if (p->wType == DNS_TYPE_MX)
                out.append(QStringLiteral("%1.  %2  IN  MX  %3 %4").arg(host, -30).arg(p->dwTtl, 6)
                    .arg(p->Data.MX.wPreference).arg(QString::fromWCharArray(p->Data.MX.pNameExchange)));
            else if (p->wType == DNS_TYPE_CNAME)
                out.append(QStringLiteral("%1.  %2  IN  CNAME  %3").arg(host, -30).arg(p->dwTtl, 6)
                    .arg(QString::fromWCharArray(p->Data.CNAME.pNameHost)));
            else if (p->wType == DNS_TYPE_SOA)
                out.append(QStringLiteral("%1.  %2  IN  SOA  %3 %4 (serial %5)").arg(host, -30).arg(p->dwTtl, 6)
                    .arg(QString::fromWCharArray(p->Data.SOA.pNamePrimaryServer))
                    .arg(QString::fromWCharArray(p->Data.SOA.pNameAdministrator))
                    .arg(p->Data.SOA.dwSerialNo));
            else if (p->wType == DNS_TYPE_NS)
                out.append(QStringLiteral("%1.  %2  IN  NS  %3").arg(host, -30).arg(p->dwTtl, 6)
                    .arg(QString::fromWCharArray(p->Data.NS.pNameHost)));
        }
        if (!first) out.append(QString());
        DnsRecordListFree(rec, DnsFreeRecordList);
    }
    if (!hasAnswer) out.append(QStringLiteral(";; (no records found)"));
#else
    // ── Linux: full dig-like with all sections ─────────────────────────
    // Primary query: A record to get header + ANSWER + AUTHORITY + ADDITIONAL
    {
        unsigned char buf[4096];
        int len = res_query(hb.constData(), C_IN, ns_t_a, buf, sizeof(buf));
        if (len >= 0) {
            ns_msg handle;
            if (ns_initparse(buf, len, &handle) >= 0) {
                int rcode = ns_msg_getflag(handle, ns_f_rcode);
                int qdCount = ns_msg_count(handle, ns_s_qd);
                int anCount = ns_msg_count(handle, ns_s_an);
                int nsCount = ns_msg_count(handle, ns_s_ns);
                int arCount = ns_msg_count(handle, ns_s_ar);
                bool qr = ns_msg_getflag(handle, ns_f_qr);
                bool rd = ns_msg_getflag(handle, ns_f_rd);
                bool ra = ns_msg_getflag(handle, ns_f_ra);
                bool aa = ns_msg_getflag(handle, ns_f_aa);
                bool tc = ns_msg_getflag(handle, ns_f_tc);
                uint16_t id = ns_msg_id(handle);

                out.append(QStringLiteral(";; Got answer:"));
                out.append(QStringLiteral(";; ->>HEADER<<- opcode: QUERY, status: %1, id: %2")
                    .arg(rcodeStr(rcode)).arg(id));
                out.append(QStringLiteral(";; flags: %1%2%3%4%5; QUERY: %6, ANSWER: %7, AUTHORITY: %8, ADDITIONAL: %9")
                    .arg(qr ? "qr " : "").arg(aa ? "aa " : "").arg(tc ? "tc " : "")
                    .arg(rd ? "rd " : "").arg(ra ? "ra " : "")
                    .arg(qdCount).arg(anCount).arg(nsCount).arg(arCount));
                out.append(QString());

                // QUESTION SECTION
                out.append(QStringLiteral(";; QUESTION SECTION:"));
                out.append(QStringLiteral(";%1.\t\t\tIN\tA").arg(host));
                out.append(QString());

                // ANSWER SECTION
                bool gotCname = false; QString cnameTarget;
                dnsDumpSection(handle, ns_s_an, QStringLiteral(";; ANSWER SECTION:"), host, out, gotCname, cnameTarget);

                // Collect A/AAAA IPs from answer
                for (int i = 0; i < anCount; i++) {
                    ns_rr rr;
                    if (ns_parserr(&handle, ns_s_an, i, &rr) < 0) continue;
                    int rt = ns_rr_type(rr);
                    const unsigned char* rd = ns_rr_rdata(rr);
                    if (rt == ns_t_a) {
                        struct in_addr a; memcpy(&a, rd, 4);
                        ipsAll.append(ip4ToStr(a));
                    } else if (rt == ns_t_aaaa) {
                        char ip6[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, rd, ip6, sizeof(ip6));
                        ipsAll.append(QString::fromLatin1(ip6));
                    }
                }

                if (anCount == 0) out.append(QStringLiteral(";; ANSWER SECTION: (empty)"));
                out.append(QString());

                // AUTHORITY SECTION
                dnsDumpSection(handle, ns_s_ns, QStringLiteral(";; AUTHORITY SECTION:"), host, out, gotCname, cnameTarget);

                // ADDITIONAL SECTION
                dnsDumpSection(handle, ns_s_ar, QStringLiteral(";; ADDITIONAL SECTION:"), host, out, gotCname, cnameTarget);

                // If CNAME found, also resolve CNAME target
                if (gotCname && !cnameTarget.isEmpty()) {
                    QByteArray cb = cnameTarget.toUtf8();
                    len = res_query(cb.constData(), C_IN, ns_t_a, buf, sizeof(buf));
                    if (len >= 0 && ns_initparse(buf, len, &handle) >= 0) {
                        dnsDumpSection(handle, ns_s_an, QStringLiteral(";; CNAME RESOLUTION (%1):").arg(cnameTarget), cnameTarget, out, gotCname, cnameTarget);
                        // Also collect IPs from CNAME target
                        int cc = ns_msg_count(handle, ns_s_an);
                        for (int i = 0; i < cc; i++) {
                            ns_rr rr;
                            if (ns_parserr(&handle, ns_s_an, i, &rr) < 0) continue;
                            if (ns_rr_type(rr) == ns_t_a) {
                                struct in_addr a; memcpy(&a, ns_rr_rdata(rr), 4);
                                QString ip = ip4ToStr(a);
                                if (!ipsAll.contains(ip)) ipsAll.append(ip);
                            }
                        }
                    }
                }
            }
        } else {
            out.append(QStringLiteral(";; Query failed (A record)"));
            out.append(QString());
        }
    }

    // Secondary queries: MX, SOA, TXT (if not already in A response)
    struct { int type; const char* name; } extra[] = {{ns_t_mx, "MX"}, {ns_t_soa, "SOA"}, {ns_t_txt, "TXT"}};
    for (auto& q : extra) {
        unsigned char buf[4096];
        int len = res_query(hb.constData(), C_IN, q.type, buf, sizeof(buf));
        if (len < 0) continue;
        ns_msg handle;
        if (ns_initparse(buf, len, &handle) < 0) continue;
        if (ns_msg_getflag(handle, ns_f_rcode) != ns_r_noerror) continue;
        bool dump = false; QString unused;
        dnsDumpSection(handle, ns_s_an, QStringLiteral(";; %1 SECTION:").arg(q.name), host, out, dump, unused);
        if (ns_msg_count(handle, ns_s_an) > 0) out.append(QString());
    }
#endif

    // ── Footer ──────────────────────────────────────────────────────────
    out.append(QStringLiteral(";; Query time: %1 msec").arg(t.elapsed()));
#if defined(_WIN32) || defined(__ANDROID__)
    out.append(QStringLiteral(";; SERVER: system resolver"));
#else
    // Show actual resolver address from _res (glibc-specific)
    QStringList nsList;
    for (int i = 0; i < MAXNS && _res.nsaddr_list[i].sin_addr.s_addr != 0; i++)
        nsList.append(ip4ToStr(_res.nsaddr_list[i].sin_addr));
    out.append(QStringLiteral(";; SERVER: %1").arg(nsList.isEmpty() ? QStringLiteral("system") : nsList.join(QStringLiteral(", "))));
#endif
    out.append(QStringLiteral(";; WHEN: %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("ddd MMM d hh:mm:ss yyyy"))));
    out.append(QString());

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.durationMs = t.elapsed();

    if (!ipsAll.isEmpty()) {
        r.summary = QStringLiteral("%1 → %2").arg(host, ipsAll.join(QStringLiteral(", ")));
        r.status = DiagStatus::Pass;
    } else {
        r.summary = QStringLiteral("DNS resolution failed for %1").arg(host);
        r.status = DiagStatus::Fail;
    }
    r.properties.append(prop("Target", target));
    r.properties.append(prop("Host", host));
    r.properties.append(prop("Addresses", ipsAll.isEmpty() ? QStringLiteral("(none)") : ipsAll.join(QStringLiteral(", "))));
    return r;
}
