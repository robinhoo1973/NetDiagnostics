#!/usr/bin/env bash
# =============================================================================
# drive-linux.sh — External UI driver for NetDiagnostics (Linux desktop)
# =============================================================================
# Launches the app under Xvfb, drives it like a real user with xdotool, and
# captures OS-level screenshots at each runtime stage.  ZERO app source
# modifications — pure OS-level automation of the production binary.
#
# Stage sequence (shared by all platform drivers):
#   1-idle → 2-running → 3-complete → 4-detail → 5-dashboard → 6-report
#   → 7-config → 8-settings
#
# Usage:
#   xvfb-run -a -s "-screen 0 1280x800x24" \
#       bash scripts/screenshot/drive-linux.sh ./build/net_diagnostics \
#       resources/doc/screenshot/linux
#
# Env overrides (all optional):
#   ND_CAPTURE_TARGET    target URL        (default http://localhost:8888)
#   ND_CAPTURE_MAX_TESTS tests per group   (default 4 — caps runtime via the
#                        app's existing ND_MAX_TESTS feature; no source change)
#   SCREEN_W / SCREEN_H  virtual screen    (default 1280x800)
#   ND_RESULT_ROW_Y      first result row  (tunable if layout shifts)
#   ND_DEBUG             keep app + server running after capture (1)
#
# Requires (installed by the workflow): xvfb, xdotool, imagemagick, x11-utils
# =============================================================================
set -euo pipefail

APP="${1:?usage: drive-linux.sh <app-binary> [out-dir]}"
OUT_REL="${2:-resources/doc/screenshot/linux}"
SCREEN_W="${SCREEN_W:-1440}"
SCREEN_H="${SCREEN_H:-1100}"
SLOW_PORT="${SLOW_PORT:-8899}"
TARGET_URL="${ND_CAPTURE_TARGET:-http://localhost:$SLOW_PORT}"
MAX_TESTS="${ND_CAPTURE_MAX_TESTS:-4}"
RESULT_ROW_Y="${ND_RESULT_ROW_Y:-0}"   # 0 → computed below
REPORT_BTN_Y="${ND_REPORT_BTN_Y:-0}"   # 0 → computed below

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
case "$OUT_REL" in
    /*) OUT_DIR="$OUT_REL" ;;          # absolute path passed as-is
    *)  OUT_DIR="$PROJECT_DIR/$OUT_REL" ;;
esac
mkdir -p "$OUT_DIR"

log()  { echo -e "\033[1;36m[DRIVE]\033[0m $*"; }
warn() { echo -e "\033[1;33m[WARN]\033[0m  $*"; }
die()  { echo -e "\033[1;31m[ERROR]\033[0m $*" >&2; exit 1; }

for t in xdotool import identify md5sum; do
    command -v "$t" >/dev/null 2>&1 || die "missing tool: $t"
done
[ -n "${DISPLAY:-}" ] || die "DISPLAY not set — run under xvfb-run"
[ -x "$APP" ] || die "app binary not found: $APP"

# ── Coordinates — derived from the fixed layout (48px AppBar, toolbar row,
# 44px controls, 56px bottom nav with 4 centered tabs).  The app is a
# frameless window that C++ showMaximized() maps to the full virtual screen,
# so window origin == (0,0) and size == SCREEN_W x SCREEN_H.
# 5WHY: computed (not hardcoded) so a different xvfb resolution still works;
# every value is overridable below via env for runtime tuning.
WX=0; WY=0; W=$SCREEN_W; H=$SCREEN_H
APPBAR=48
TOOLBAR_CY=$((WY + APPBAR + 4 + 22))          # toolbar row centre
HOST_CX=$((WX + (W + 94) / 2))                # host field centre
RUN_CX=$((WX + W - 26))                       # run/stop button centre
NAV_Y=$((WY + H - 28))                        # bottom nav centre
# Nav tab widths ≈ label(mono 12px) + 56px padding; row is centred.
NAV_ROW_W=539
NAV_X0=$((WX + (W - NAV_ROW_W) / 2))
NAV_DASH_CX=$((NAV_X0 + 60))
NAV_DIAG_CX=$((NAV_X0 + 192))
NAV_CFG_CX=$((NAV_X0 + 342))
NAV_SET_CX=$((NAV_X0 + 482))
# Detail overlay: centred 700px card → close button (44px, 8px inset).
DETAIL_CARD_W=700; DETAIL_CARD_H=620
DETAIL_X0=$((WX + (W - DETAIL_CARD_W) / 2))
DETAIL_Y0=$((WY + (H - DETAIL_CARD_H) / 2))
DETAIL_CLOSE_CX=$((DETAIL_X0 + DETAIL_CARD_W - 30))
DETAIL_CLOSE_CY=$((DETAIL_Y0 + 30))
# Report preview overlay: dismiss via the 8px backdrop strip on the left edge.
# 5WHY: the overlay's own close button sits at the window's top-right corner,
# but main.qml declares the frameless window's close button AFTER AppContent,
# so it stacks ABOVE the overlay — clicking there closes the whole app.  The
# backdrop MouseArea (anchors.fill) dismisses the overlay on any outside click.
REPORT_CLOSE_CX=$((WX + 4))
REPORT_CLOSE_CY=$((WY + H / 2))
# Review Report button — with the tall default screen (1440x1100) the whole
# dashboard fits without scrolling (Qt6 Flickables ignore injected wheel/key/
# drag input under Xvfb), so the button sits at a fixed, measurable position.
REPORT_BTN_X=$((WX + W / 2))                  # Review Report (full width)
[ "$RESULT_ROW_Y" -eq 0 ] && RESULT_ROW_Y=$((WY + 240))
[ "$REPORT_BTN_Y" -eq 0 ] && REPORT_BTN_Y=$((WY + 1035))
RESULT_ROW_X=$((WX + 400))

log "layout: ${W}x${H} toolbar_y=$TOOLBAR_CY host_x=$HOST_CX run_x=$RUN_CX nav_y=$NAV_Y"
log "target=$TARGET_URL max_tests=$MAX_TESTS out=$OUT_DIR"

# ── Slow deterministic HTTP server (delays ~2s/response so the diagnostic
# run lasts several seconds — a <0.1s run can never show the Running stage) ─
SERVER_PID=""
if command -v python3 >/dev/null 2>&1; then
    python3 "$PROJECT_DIR/scripts/screenshot/slow-http-server.py" "$SLOW_PORT" 2.0 \
        >/tmp/netdiag-server.log 2>&1 &
    SERVER_PID=$!
    log "slow-http-server started (pid $SERVER_PID, localhost:$SLOW_PORT, ~2s delay)"
else
    warn "python3 not found — server skipped; network tests will fail"
fi

OPENBOX_PID=""
APP_PID=""
WIN_ID=""
cleanup() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null || true
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null || true
    [ -n "$OPENBOX_PID" ] && kill "$OPENBOX_PID" 2>/dev/null || true
}
trap cleanup EXIT

# ── launch_app / kill_app — the driver runs in TWO passes (see stage
# sequence below); the report preview overlay cannot be dismissed by any
# injected input under Xvfb, so Pass A ends there and we simply relaunch.
launch_app() { # start openbox + app, wait for window (bounded, fail-fast)
    # 5WHY: a minimal window manager must run inside Xvfb — without one,
    # showMaximized() is not honoured (window stays 160x160) and there is no
    # keyboard focus, so xdotool typing fails silently.
    OPENBOX_PID=""
    if command -v openbox >/dev/null 2>&1; then
        openbox >/tmp/netdiag-openbox.log 2>&1 &
        OPENBOX_PID=$!
        sleep 1
        log "openbox WM started (pid $OPENBOX_PID)"
    fi

    # Fresh QSettings every launch → deterministic default config (all tests
    # enabled).  Stale persisted state (e.g. a group toggled off in a prior
    # CI run) would silently schedule 0 tests and produce empty screenshots.
    rm -f "$HOME/.config/robinhoo1973/NetDiagnostics.conf" 2>/dev/null || true

    # ND_MAX_TESTS caps per-group tests — existing runtime feature keeping the
    # run fast + deterministic (skips G3 GeoIP/speedtest).
    ND_MAX_TESTS=$MAX_TESTS "$APP" >/tmp/netdiag-app.log 2>&1 &
    APP_PID=$!
    log "app started (pid $APP_PID) — waiting for window"

    # Bounded window wait — never block forever (xdotool search --sync hangs).
    WIN_ID=""
    local i
    for i in $(seq 1 30); do
        if ! kill -0 "$APP_PID" 2>/dev/null; then
            die "app exited early (pid $APP_PID) — log tail:\n$(tail -20 /tmp/netdiag-app.log 2>/dev/null)"
        fi
        WIN_ID="$(xdotool search --onlyvisible --name 'NetDiagnostics' 2>/dev/null | head -1 || true)"
        [ -n "$WIN_ID" ] && break
        sleep 1
    done
    if [ -n "$WIN_ID" ]; then
        xdotool windowactivate "$WIN_ID" 2>/dev/null || true
        xdotool windowraise "$WIN_ID" 2>/dev/null || true
        # Belt-and-suspenders: force the exact geometry so the fixed-layout
        # coordinate math is valid even if the WM skipped the maximize request.
        xdotool windowsize "$WIN_ID" "$SCREEN_W" "$SCREEN_H" 2>/dev/null || true
        xdotool windowmove "$WIN_ID" 0 0 2>/dev/null || true
        log "window $WIN_ID activated + sized ${SCREEN_W}x${SCREEN_H} (~${i}s)"
    else
        die "no window after 30s — app log tail:\n$(tail -20 /tmp/netdiag-app.log 2>/dev/null)"
    fi
}
kill_app() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null || true
    [ -n "$OPENBOX_PID" ] && kill "$OPENBOX_PID" 2>/dev/null || true
    APP_PID=""; OPENBOX_PID=""
    sleep 1
}

# ── Helpers ──────────────────────────────────────────────────────────────
check_alive() { # fail fast if the app died
    if ! kill -0 "$APP_PID" 2>/dev/null; then
        die "app died — log tail:\n$(tail -20 /tmp/netdiag-app.log 2>/dev/null)"
    fi
}
frame_hash() { import -window root png:- 2>/dev/null | md5sum | cut -d' ' -f1; }
capture() { # capture <stage> [frame-hash-before]
    local stage="$1" before="${2:-}"
    local f="$OUT_DIR/$stage.png" mean h
    check_alive
    import -window root "$f" 2>/dev/null || import -window root -screen "$f" 2>/dev/null || true
    if [ -s "$f" ]; then
        h="$(frame_hash)"
        mean="$(identify -format '%[fx:round(mean*255)]' "$f" 2>/dev/null || echo 0)"
        log "captured $stage (mean=$mean)"
        if [ -n "$before" ] && [ "$before" = "$h" ]; then
            warn "$stage identical to previous frame — expected a state change"
        fi
        # 5WHY: if-statement form (not `[ x ] && warn`): under `set -e` the
        # short-circuit && list makes the function return 1 when the
        # condition is false, killing the whole driver mid-run.
        if [ "$mean" -lt 8 ]; then
            warn "$stage looks blank (mean=$mean)"
        fi
    else
        warn "capture failed for $stage"
    fi
    return 0
}
click() { # click <x> <y> <label>
    check_alive
    xdotool mousemove --sync "$1" "$2" click 1
    log "clicked $3 ($1,$2)"
    sleep 1
}
type_url() {
    check_alive
    xdotool type --delay 30 "$TARGET_URL"
    log "typed target: $TARGET_URL"
    sleep 1
}
nav_to() { click "$1" "$NAV_Y" "nav-$2"; sleep 1; }
# click_until_changed — click a list of y-candidates at x until the frame
# changes (target state reached) or all candidates fail.  Robust to small
# layout drift.  Returns 0 if a state change was detected.
click_until_changed() { # click_until_changed <label> <x> <y1> [<y2> ...]
    local label="$1" x="$2"; shift 2
    local before y
    before="$(frame_hash)"
    for y in "$@"; do
        click "$x" "$y" "$label@$y"
        sleep 2
        if [ "$before" != "$(frame_hash)" ]; then
            log "state change detected after $label@$y"
            return 0
        fi
    done
    warn "$label: no state change after trying y=$*"
    return 1
}
wait_stable() { # wait_stable <max-seconds> — poll until 2 frames are identical
    local max="${1:-45}" t=0 a b
    while [ "$t" -lt "$max" ]; do
        a="$(frame_hash)"; sleep 4; b="$(frame_hash)"
        if [ -n "$a" ] && [ "$a" = "$b" ]; then
            log "screen stable after ~$((t + 4))s"
            sleep 2
            return 0
        fi
        t=$((t + 4))
    done
    warn "screen not stable within ${max}s — proceeding anyway"
    return 0
}

# ══════════════════════════════════════════════════════════════════════════
# PASS A — 1-idle → 2-running → 3-complete → 4-detail → 5-dashboard → 6-report
# The report preview overlay cannot be dismissed by injected input under
# Xvfb (its full-screen backdrop MouseArea and the navBlocked close path are
# both unresponsive to synthetic events), so Pass A ends there and we
# relaunch for the remaining screens.
# ══════════════════════════════════════════════════════════════════════════
launch_app
sleep 2
capture 1-idle

# ── Stage 2: Running — the slow server keeps the run alive for seconds ────
click "$HOST_CX" "$TOOLBAR_CY" "host-field"
type_url
click "$RUN_CX" "$TOOLBAR_CY" "run-button"
sleep 2
capture 2-running

# ── Stage 3: Complete ────────────────────────────────────────────────────
wait_stable 60
capture 3-complete

# ── Stage 4: Detail overlay ──────────────────────────────────────────────
# 5WHY: keyboard is the reliable opener — after clicking Run the focus is on
# the run button; Tab → first group header, Tab → first result row (rows are
# activeFocusOnTab), Enter opens the detail overlay.  Mouse-clicking a row is
# fragile (row y drifts with the results header; a near-miss collapses the
# group header, which is a false "state change").
BEFORE_3="$(frame_hash)"
xdotool key Tab; sleep 0.3
xdotool key Tab; sleep 0.3
xdotool key Return; sleep 2
if [ "$BEFORE_3" != "$(frame_hash)" ]; then
    log "detail overlay opened via keyboard (Tab Tab Enter)"
    capture 4-detail
    # Close the overlay — only if it is actually open (safe mid-screen point).
    click "$DETAIL_CLOSE_CX" "$DETAIL_CLOSE_CY" "detail-close"
else
    warn "detail overlay did not open via keyboard — capturing current frame"
    capture 4-detail
fi

# ── Stage 5: Dashboard ───────────────────────────────────────────────────
nav_to "$NAV_DASH_CX" "dashboard"
sleep 1
capture 5-dashboard

# ── Stage 6: Report preview (Review Report button, scan a small y-range) ──
if click_until_changed "review-report" "$REPORT_BTN_X" \
        "$((REPORT_BTN_Y - 15))" "$REPORT_BTN_Y" "$((REPORT_BTN_Y + 15))"; then
    sleep 2
    capture 6-report
    log "report preview captured — Pass A complete; relaunching for B"
else
    warn "report preview did not open — capturing current frame"
    capture 6-report
fi
kill_app

# ══════════════════════════════════════════════════════════════════════════
# PASS B — 7-config → 8-settings (fresh launch; no overlay to dismiss)
# ══════════════════════════════════════════════════════════════════════════
launch_app
nav_to "$NAV_CFG_CX" "config"
capture 7-config

nav_to "$NAV_SET_CX" "settings"
capture 8-settings

kill_app
log "done — screenshots in $OUT_DIR"
if [ "${ND_DEBUG:-0}" = "1" ]; then
    log "ND_DEBUG=1 — leaving app/server running for manual inspection"
    exit 0
fi
cleanup
exit 0
