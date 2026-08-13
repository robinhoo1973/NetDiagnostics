// =============================================================================
// DnsWire.h — RFC 1035 DNS wire-format helpers (shared service)
//
// R5-6: G3 (hijack/pollution scoring) and G4 (dig-like resolution) each had
// a private copy of the DNS query builder / response parser (~150 duplicated
// lines).  Single implementation here: A/AAAA/CNAME records, TTL, rcode,
// compression-pointer name parsing, and a blocking UDP query that is safe in
// worker threads (waitFor* only — no event loop).
// =============================================================================
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QByteArray>
#include <QHostAddress>
#include <QUdpSocket>
#include <QElapsedTimer>

namespace dnsWire {

struct Record {
    int     type = 0;    // 1=A, 5=CNAME, 28=AAAA
    int     ttl  = 0;
    QString value;
};

struct Answer {
    QStringList aRecords;       // A 记录（顺序无关）
    QStringList cnameChain;     // CNAME 链（出现顺序）
    bool        hasCname  = false;
    int         minTtl    = -1;   // -1 = 无 TTL 数据（哨兵）
    int         elapsedMs = 0;
    int         rcode     = -1;   // DNS RCODE（0=NOERROR, 3=NXDOMAIN...）
    QVector<Record> records;      // 全部应答记录（含 AAAA）
};

// Name parsing with RFC 1035 compression pointers.
inline QString readName(const QByteArray& msg, int& pos, int depth = 0) {
    QString name;
    while (pos < msg.size() && depth < 16) {
        const quint8 len = (quint8)msg[pos];
        if (len == 0) { ++pos; break; }
        if ((len & 0xC0) == 0xC0) {          // compression pointer
            if (pos + 1 >= msg.size()) break;
            int jump = ((len & 0x3F) << 8) | (quint8)msg[pos + 1];
            pos += 2;
            const QString tail = readName(msg, jump, depth + 1);
            if (!name.isEmpty() && !tail.isEmpty()) name += QLatin1Char('.');
            name += tail;
            break;
        }
        if (pos + 1 + len > msg.size()) break;
        if (!name.isEmpty()) name += QLatin1Char('.');
        name += QString::fromLatin1(msg.mid(pos + 1, len));
        pos += 1 + len;
    }
    return name;
}

inline QByteArray buildQuery(const QString& domain, int qtype) {
    QByteArray q;
    q.append(char(0x12)); q.append(char(0x34));   // transaction id
    q.append(char(0x01)); q.append(char(0x00));   // flags: RD
    q.append(char(0x00)); q.append(char(0x01));   // QDCOUNT = 1
    q.append(char(0x00)); q.append(char(0x00));   // ANCOUNT
    q.append(char(0x00)); q.append(char(0x00));   // NSCOUNT
    q.append(char(0x00)); q.append(char(0x00));   // ARCOUNT
    const QStringList labels = domain.split(QLatin1Char('.'));
    for (const QString& label : labels) {
        const QByteArray b = label.toLatin1();
        q.append(char(b.size()));
        q.append(b);
    }
    q.append(char(0x00));
    q.append(char((qtype >> 8) & 0xFF)); q.append(char(qtype & 0xFF));
    q.append(char(0x00)); q.append(char(0x01));   // QCLASS = IN
    return q;
}

inline Answer parseResponse(const QByteArray& resp) {
    Answer a;
    if (resp.size() < 12) return a;
    a.rcode = (quint8)resp[3] & 0x0F;
    const quint16 an = (quint16)((quint8)resp[6] << 8) | (quint8)resp[7];
    const quint16 ns = (quint16)((quint8)resp[8] << 8) | (quint8)resp[9];
    int pos = 12;
    // Skip the question section (echoed QNAME + QTYPE + QCLASS).
    readName(resp, pos);
    pos += 4;
    const int rrCount = (int)an + (int)ns;
    for (int i = 0; i < rrCount && pos + 12 <= resp.size(); ++i) {
        readName(resp, pos);                 // owner name
        if (pos + 10 > resp.size()) break;
        const quint16 type = (quint16)((quint8)resp[pos] << 8) | (quint8)resp[pos + 1];
        const quint32 ttl  = (quint32)(quint8)resp[pos + 4] << 24
                           | (quint32)(quint8)resp[pos + 5] << 16
                           | (quint32)(quint8)resp[pos + 6] << 8
                           | (quint32)(quint8)resp[pos + 7];
        const quint16 rdlen = (quint16)((quint8)resp[pos + 8] << 8) | (quint8)resp[pos + 9];
        pos += 10;
        if (pos + rdlen > resp.size()) break;
        if (type == 1 && rdlen == 4) {      // A
            const QByteArray ip = resp.mid(pos, 4);
            const QString ipStr = QStringLiteral("%1.%2.%3.%4")
                .arg((quint8)ip[0]).arg((quint8)ip[1]).arg((quint8)ip[2]).arg((quint8)ip[3]);
            a.aRecords.append(ipStr);
            a.records.append({type, (int)ttl, ipStr});
            if (a.minTtl < 0 || (int)ttl < a.minTtl) a.minTtl = (int)ttl;
        } else if (type == 28 && rdlen == 16) {   // AAAA
            QHostAddress v6;
            v6.setAddress(resp.mid(pos, 16));
            a.records.append({type, (int)ttl, v6.toString()});
        } else if (type == 5) {             // CNAME
            int np = pos;
            const QString target = readName(resp, np);
            if (!target.isEmpty()) {
                a.cnameChain.append(target);
                a.hasCname = true;
                a.records.append({type, (int)ttl, target});
            }
        }
        pos += rdlen;
    }
    return a;
}

// Blocking UDP query to a specific DNS server — worker-thread safe
// (waitFor* only; no event loop, no QNAM).
inline Answer udpQuery(const QString& domain, int qtype, const QString& serverIp,
                       int timeoutMs = 3000) {
    Answer a;
    QElapsedTimer t; t.start();
    QUdpSocket sock;
    const QByteArray query = buildQuery(domain, qtype);
    sock.connectToHost(QHostAddress(serverIp), 53);
    if (!sock.waitForConnected(1500)) { a.elapsedMs = (int)t.elapsed(); return a; }
    if (sock.write(query) != query.size()) { a.elapsedMs = (int)t.elapsed(); return a; }
    const qint64 deadline = t.elapsed() + timeoutMs;
    while (t.elapsed() < deadline) {
        if (!sock.waitForReadyRead(qMin<qint64>(250, deadline - t.elapsed())))
            break;
        while (sock.hasPendingDatagrams()) {
            QByteArray resp;
            resp.resize((int)sock.pendingDatagramSize());
            sock.readDatagram(resp.data(), resp.size());
            if (resp.size() >= 12 && (quint8)resp[0] == 0x12 && (quint8)resp[1] == 0x34) {
                a = parseResponse(resp);
                a.elapsedMs = (int)t.elapsed();
                return a;
            }
        }
    }
    a.elapsedMs = (int)t.elapsed();
    return a;
}

} // namespace dnsWire
