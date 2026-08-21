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
#include <QRandomGenerator>

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
    // 5WHY (2026-08-20 用户诉求 "输出与 dig 有距离"): dig 的 flags 行
    // （qr/aa/tc/rd/ra）与 "MSG SIZE rcvd" 均来自响应头——此前解析器
    // 丢弃 flags 字与报文长度，dig 风格输出只能硬编码 "qr rd ra"。
    // 捕获真实值供 G4DnsResolution 呈现。
    // 5WHY (复核 2026-08-20 计数半成品): 前次只捕获 flags/msgSize——
    // 事务 id 以字面量 0x1234 传入 formatter（dig 用户逐字段比对时
    // 即见常量假 id），AUTHORITY/ADDITIONAL 参数恒默认 0（API 看似
    // 上报真实计数实则死参）。5WHY (复核 2026-08-21): id 现为随机事务
    // id（udpQuery 生成，应答按 id 匹配），此处捕获真实值。
    quint16 flags    = 0;        // 响应头 flags 字（RFC 1035 §4.1.1 第 2-3 字节）
    // 5WHY (复核 2026-08-20 失败伪造): msgSize 曾默认 0——查询失败
    // （连接失败/超时，parseResponse 从未运行）时调用方拿默认 0 传
    // footer，guard(>=0) 放行 → 输出 ";; MSG SIZE  rcvd: 0" 伪造收到
    // 0 字节应答（dig 超时只输出 "connection timed out"，无 footer）。
    // 默认 -1 = 无响应哨兵，footer 抑制。
    int     msgSize  = -1;       // 响应报文总字节数；-1 = 未收到响应
    quint16 id       = 0;        // 事务 id（resp[0..1]）
    quint16 anCount  = 0;        // ANSWER 计数（真实头值，含未解析类型）
    quint16 nsCount  = 0;        // AUTHORITY 计数
    quint16 arCount  = 0;        // ADDITIONAL 计数
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

inline QByteArray buildQuery(const QString& domain, int qtype, quint16 id) {
    QByteArray q;
    // 5WHY (复核 2026-08-21 事务 id 死值): 曾恒以常量 0x1234 作事务 id，
    // 且 udpQuery 只接受 id==0x1234 的应答——新捕获的 Answer.id 因此
    // 恒为 0x1234（"真实事务 id"修复落空，header 仍打印假 id 4660）。
    // id 参数由 udpQuery 随机生成（必传，无默认死值——漏传即编译错误，
    // 防新调用方静默重获确定性 id）；应答按 id 匹配（防串扰应答）。
    q.append(char((id >> 8) & 0xFF)); q.append(char(id & 0xFF));   // transaction id
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
    a.id = (quint16)((quint8)resp[0] << 8) | (quint8)resp[1];
    a.rcode = (quint8)resp[3] & 0x0F;
    a.flags = (quint16)((quint8)resp[2] << 8) | (quint8)resp[3];
    a.msgSize = resp.size();
    a.anCount = (quint16)((quint8)resp[6] << 8) | (quint8)resp[7];
    a.nsCount = (quint16)((quint8)resp[8] << 8) | (quint8)resp[9];
    a.arCount = (quint16)((quint8)resp[10] << 8) | (quint8)resp[11];
    int pos = 12;
    // Skip the question section (echoed QNAME + QTYPE + QCLASS).
    readName(resp, pos);
    pos += 4;
    // 5WHY (复核 2026-08-21 死迭代): 曾 rrCount = anCount + nsCount——
    // 解析器只产出 A/AAAA/CNAME（ANSWER 节记录），AUTHORITY 迭代零产出，
    // 仅给截断报文多一条早退路径。只迭代 ANSWER 计数。
    const int rrCount = (int)a.anCount;
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
    // 随机事务 id（非 0x1234 常量）——应答按 id 匹配，Answer.id 才是真实值
    const quint16 txId = QRandomGenerator::global()->bounded(0x10000);
    const QByteArray query = buildQuery(domain, qtype, txId);
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
            if (resp.size() >= 12) {
                const quint16 respId = (quint16)((quint8)resp[0] << 8) | (quint8)resp[1];
                if (respId == txId) {
                    a = parseResponse(resp);
                    a.elapsedMs = (int)t.elapsed();
                    return a;
                }
            }
        }
    }
    a.elapsedMs = (int)t.elapsed();
    return a;
}

} // namespace dnsWire
