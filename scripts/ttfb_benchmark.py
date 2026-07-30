#!/usr/bin/env python3
"""
TTFB (Time To First Byte) benchmark for VPN detection.
Probes all speed-test servers from G3ServerDb.inc, measures TTFB to each,
reports latency distribution by country, bandwidth usage, and reliability.
"""
import socket, time, sys, os

# Server list extracted from G3ServerDb.inc
SERVERS = [
    # East Asia
    ("CN","speedtest1.gd.chinamobile.com",8080), ("CN","speedtest.bj.chinamobile.com",8080),
    ("KR","seoul.speedtest.gslnetworks.com",8080), ("KR","speedtest.kornet.net",8080),
    ("KR","speedtest.kt.com",8080), ("KR","speedtest.skbb.co.kr",8080),
    ("JP","speedtest.tokyo2.jp.leaseweb.net",8080), ("JP","speedtest.tyo1.jp.leaseweb.net",8080),
    ("JP","speedtest.kddi.com",8080), ("JP","speedtest.nuro.jp",8080),
    ("MN","speedtest.gemnet.mn",8080), ("MN","speedtest.mobinet.mn",8080),
    # Southeast Asia
    ("SG","speedtest.singtel.com",8080), ("SG","speedtest.myrepublic.net",8080),
    ("SG","speedtest.m1.com.sg",8080), ("SG","speedtest.starhub.com",8080),
    ("TH","speedtest.trueinternet.co.th",8080), ("TH","speedtest.ais.co.th",8080),
    ("TH","speedtest.3bb.co.th",8080),
    ("MY","speedtest.tm.com.my",8080), ("MY","speedtest.maxis.com.my",8080),
    ("ID","speedtest.telkom.co.id",8080), ("ID","speedtest.indosat.com",8080),
    ("PH","speedtest.pldt.com.ph",8080), ("PH","speedtest.globe.com.ph",8080),
    # South Asia
    ("IN","speedtest.actcorp.in",8080), ("IN","speedtest.jio.com",8080),
    # Oceania
    ("AU","speedtest.telstra.net",8080), ("AU","speedtest.optusnet.com.au",8080),
    ("AU","speedtest.launtel.net.au",8080),
    ("NZ","speedtest.spark.co.nz",8080), ("NZ","speedtest.2degrees.nz",8080),
    # Europe
    ("SE","speedtest.tele2.net",8080), ("SE","speedtest.ownit.se",8080),
    ("SE","speedtest.telia.com",8080),
    ("DE","speedtest.belwue.net",8080), ("DE","speedtest.telekom.de",8080),
    ("DE","speedtest.vodafone.de",8080),
    ("GB","speedtest.bt.com",8080), ("GB","speedtest.vodafone.co.uk",8080),
    ("NL","speedtest.ams1.nl.leaseweb.net",8080), ("NL","speedtest.kpn.net",8080),
    ("FR","speedtest.orange.fr",8080), ("FR","speedtest.sfr.fr",8080),
    ("IT","speedtest.optimaitalia.com",8080), ("IT","speedtest.tim.it",8080),
    ("ES","speedtest.movistar.es",8080), ("ES","speedtest.vodafone.es",8080),
    ("GR","speedtest.ftp.otenet.gr",8080), ("GR","speedtest.vodafone.gr",8080),
    ("RU","speedtest.mts.ru",8080), ("RU","speedtest.ertelecom.ru",8080),
    # Middle East
    ("AE","speedtest.du.ae",8080), ("AE","speedtest.etisalat.ae",8080),
    ("SA","speedtest.stc.com.sa",8080), ("SA","speedtest.mobily.com.sa",8080),
    ("TR","speedtest.turktelekom.com.tr",8080), ("TR","speedtest.superonline.net",8080),
    ("QA","speedtest.ooredoo.qa",8080),
    ("IL","speedtest.bezeq.co.il",8080),
    ("EG","speedtest.te.eg",8080),
    # Africa
    ("NG","speedtest.mtn.ng",8080),
    ("KE","speedtest.safaricom.co.ke",8080),
    ("ZA","speedtest.mtn.co.za",8080), ("ZA","speedtest.vodacom.co.za",8080),
    # North America
    ("US","speedtest.xfinity.com",8080), ("US","speedtest.att.com",8080),
    ("US","speedtest.verizon.net",8080), ("US","speedtest.sonic.net",8080),
    ("US","speedtest.centurylink.net",8080),
    ("CA","speedtest.bell.ca",8080), ("CA","speedtest.rogers.com",8080),
    ("MX","speedtest.telmex.com",8080), ("MX","speedtest.totalplay.com.mx",8080),
    # South America
    ("BR","speedtest.vivo.com.br",8080), ("BR","speedtest.timbrasil.com.br",8080),
    ("AR","speedtest.movistar.com.ar",8080), ("AR","speedtest.personal.com.ar",8080),
]

TTFB_TARGET = 2000       # Request 2KB (enough for HTTP headers + small body)
CONNECT_TIMEOUT = 3.0    # TCP connect timeout
TOTAL_TIMEOUT = 8.0       # Total per-server timeout
CONCURRENT = 10           # Thread count

import threading
from collections import defaultdict

class Results:
    def __init__(self):
        self.lock = threading.Lock()
        self.by_country = defaultdict(list)  # country -> [ttfb_ms, ...]
        self.ok = 0
        self.tcp_ok = 0  # TCP connect succeeded
        self.dns_fail = 0
        self.http_fail = 0
        self.total_bytes = 0
        self.servers_tested = 0

results = Results()
work_idx = [0]

def probe_one(srv):
    country, host, port = srv
    try:
        ip = socket.getaddrinfo(host, port, socket.AF_INET, socket.SOCK_STREAM)[0][4][0]
    except:
        with results.lock:
            results.dns_fail += 1
        return

    tcp_rtt = None
    total_start = time.monotonic()

    # Phase 1: TCP connect (measure RTT separately)
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(CONNECT_TIMEOUT)
        t0 = time.monotonic()
        sock.connect((ip, port))
        tcp_rtt = (time.monotonic() - t0) * 1000
    except:
        sock = None

    if sock is None:
        with results.lock:
            results.servers_tested += 1
        return

    with results.lock:
        results.tcp_ok += 1

    # Phase 2: HTTP GET + TTFB
    try:
        sock.settimeout(TOTAL_TIMEOUT)
        req = f'GET /download?size={TTFB_TARGET} HTTP/1.0\r\nHost: {host}:{port}\r\nConnection: close\r\n\r\n'
        sock.sendall(req.encode())

        # Read until we get first data byte after headers
        data = b''
        first_byte_time = None
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk: break
                data += chunk
                if first_byte_time is None and b'\r\n\r\n' in data:
                    first_byte_time = time.monotonic()
                if len(data) >= TTFB_TARGET + 4096:  # headers + target bytes
                    break
            except socket.timeout:
                break
        sock.close()

        ttfb = (first_byte_time - total_start) * 1000 if first_byte_time else None

        with results.lock:
            results.servers_tested += 1
            results.total_bytes += len(data)
            if ttfb is not None and ttfb > 0:
                results.ok += 1
                results.by_country[country].append(ttfb)
            else:
                results.http_fail += 1

    except Exception as e:
        try: sock.close()
        except: pass
        with results.lock:
            results.servers_tested += 1
            results.http_fail += 1

# ── Run benchmark ──────────────────────────────────────────
print(f"TTFB Benchmark: {len(SERVERS)} servers, {CONCURRENT} threads, target={TTFB_TARGET}B")
print(f"Connect timeout={CONNECT_TIMEOUT}s, Total timeout={TOTAL_TIMEOUT}s")
print("=" * 80)

t_start = time.monotonic()

# Work-queue threading
work = list(SERVERS)
import random
random.shuffle(work)  # mix countries to avoid head-of-line blocking

def worker():
    while True:
        with results.lock:
            if work_idx[0] >= len(work): break
            idx = work_idx[0]; work_idx[0] += 1
        probe_one(work[idx])

threads = [threading.Thread(target=worker) for _ in range(CONCURRENT)]
for t in threads: t.start()
for t in threads: t.join()

wall_time = time.monotonic() - t_start

# ── Report ─────────────────────────────────────────────────
print(f"\n{'='*80}")
print(f"RESULTS: {results.servers_tested}/{len(SERVERS)} tested in {wall_time:.1f}s")
print(f"  TCP connect OK: {results.tcp_ok}")
print(f"  HTTP TTFB OK:   {results.ok}")
print(f"  HTTP fail:      {results.http_fail}")
print(f"  DNS fail:       {results.dns_fail}")
print(f"  Total data:     {results.total_bytes/1024:.1f} KB")
print(f"  Bandwidth:      {results.total_bytes*8/wall_time/1000:.1f} kbps avg")

print(f"\nPer-country TTFB (sorted by median):")
print(f"{'Ctry':5s} {'N':>3s} {'Median':>8s} {'Min':>8s} {'Max':>8s} {'Samples'}")
print("-" * 50)

country_stats = []
for code, samples in results.by_country.items():
    if len(samples) >= 2:
        s = sorted(samples)
        med = s[len(s)//2]
        country_stats.append((code, len(samples), med, s[0], s[-1]))

country_stats.sort(key=lambda x: x[2])  # sort by median

for code, n, med, lo, hi in country_stats:
    print(f"{code:5s} {n:3d} {med:7.0f}ms {lo:7.0f}ms {hi:7.0f}ms")

# Efficiency metrics
print(f"\n{'='*80}")
print("EFFICIENCY & RELIABILITY:")
print(f"  Wall time:       {wall_time:.1f}s ({len(SERVERS)} servers, {CONCURRENT} threads)")
print(f"  Per-server avg:  {wall_time/len(SERVERS)*1000:.0f}ms wall-clock")
print(f"  Data per server: {results.total_bytes/max(results.servers_tested,1)/1024:.1f} KB avg")
if results.ok > 0:
    print(f"  Success rate:    {results.ok/max(results.servers_tested,1)*100:.1f}%")
    print(f"  Geographic spread: {country_stats[-1][2]-country_stats[0][2]:.0f}ms (fastest→slowest country)")
print(f"  TCP-only success: {results.tcp_ok}/{len(SERVERS)} ({results.tcp_ok/len(SERVERS)*100:.1f}%)")
print(f"  HTTP success:     {results.ok}/{len(SERVERS)} ({results.ok/len(SERVERS)*100:.1f}%)")
