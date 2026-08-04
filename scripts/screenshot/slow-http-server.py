#!/usr/bin/env python3
"""Slow HTTP server — a deterministic, long-running diagnostic target.

The regular ci-http-server.py responds instantly, so a full local diagnostic
run finishes in <0.1s and CI can never capture the "Running" stage.  This
server delays every response (~2s) so the run lasts several seconds, giving
the screenshot drivers a reliable window to capture the live progress UI.

Usage: python3 scripts/screenshot/slow-http-server.py [port] [delay_seconds]
Default port 8899, delay 2.0s.  Threaded so concurrent probes do not queue.
"""
import http.server
import socketserver
import sys
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8899
DELAY = float(sys.argv[2]) if len(sys.argv) > 2 else 2.0


class Handler(http.server.SimpleHTTPRequestHandler):
    def _respond(self):
        # Diagnostic aid: log every request so CI can verify the app target
        # actually reached this server (path → /tmp/slow-http-requests.log).
        try:
            with open('/tmp/slow-http-requests.log', 'a') as f:
                f.write('%s %s\n' % (self.command, self.path))
        except OSError:
            pass
        time.sleep(DELAY)
        body = b"ok"
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    do_GET = do_HEAD = _respond

    def log_message(self, fmt, *args):
        pass  # suppress access logs in CI


if __name__ == '__main__':
    with socketserver.ThreadingTCPServer(('', PORT), Handler) as httpd:
        httpd.serve_forever()
