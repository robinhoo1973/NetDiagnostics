#!/usr/bin/env python3
"""HTTP test server — serves HTTP with security headers on port 8888.

5WHY: Using external services (httpbin.org) as test targets was unreliable —
if the service was down or rate-limited, manual testing would fail for reasons
unrelated to the code under test.

Start with: python3 scripts/ci-http-server.py 8888
Then target diagnostics at: localhost:8888

This local server provides:
  - TCP reachability (G5TcpConnect)
  - HTTP response (G5HttpHeaders, G5CurlVerbose, G5HttpTiming)
  - All 7 security headers (G5SecurityHeaders — checks 7 headers,
    returns Fail if >4 are missing)
  - Deterministic, always-available, zero network latency

Features NOT covered (these are Skipped, not Fail):
  - TLS/SSL (G5SslCertificate — Skipped "Not HTTPS")
  - HTTP redirects (G5HttpRedirect — 200 OK → Pass)
  - Compression (G5HttpCompression — "Uncompressed" → Info)
"""
import http.server
import socketserver
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8888


class Handler(http.server.SimpleHTTPRequestHandler):
    """HTTP request handler that adds all 7 security headers."""

    def end_headers(self):
        self.send_header('Strict-Transport-Security', 'max-age=31536000')
        self.send_header('Content-Security-Policy', "default-src 'self'")
        self.send_header('X-Frame-Options', 'DENY')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('X-XSS-Protection', '1; mode=block')
        self.send_header('Referrer-Policy', 'strict-origin')
        self.send_header('Permissions-Policy', 'geolocation=()')
        super().end_headers()

    def log_message(self, fmt, *args):
        # Suppress access logs in CI output (noise reduction)
        pass


if __name__ == '__main__':
    with socketserver.TCPServer(('', PORT), Handler) as httpd:
        httpd.serve_forever()
