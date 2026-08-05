#!/usr/bin/env bash
# =============================================================================
# drive-android-alt.sh — Linux xvfb mobile-layout screenshot driver
# =============================================================================
# Because the Android emulator is unreliable on GitHub-hosted no-KVM runners
# (software emulation takes 20+ min, adb fails intermittently), we run the
# production Linux binary WITH ND_MOBILE=1 so the app renders its authentic
# mobile layout (bottom nav, TargetInputPanel).  xdotool drives the UI and
# `import` captures per-stage screenshots.
#
# This is the same strategy as drive-ios-alt.sh (macOS binary + ND_MOBILE=1)
# but adapted for Linux with xvfb + xdotool instead of screencapture + cliclick.
#
# Stage sequence: 1-idle → 2-running → 3-complete → 4-detail →
#   5-dashboard → 6-report → 7-config → 8-settings
#
# Usage:
#   ND_MOBILE=1 ND_CAPTURE_TARGET=http://localhost:8899 \
#     xvfb-run -a -s "-screen 0 430x932x24" \
#     bash scripts/screenshot/drive-android-alt.sh ./build/net_diagnostics \
#     resources/doc/screenshot/android/phone
# =============================================================================
set -euo pipefail

APP="${1:-./build/net_diagnostics}"
OUT_REL="${2:-resources/doc/screenshot/android/phone}"
SCREEN_W="${SCREEN_W:-430}"
SCREEN_H="${SCREEN_H:-932}"
SLOW_PORT="${SLOW_PORT:-8899}"
TARGET_URL="${ND_CAPTURE_TARGET:-http://localhost:$SLOW_PORT}"
MAX_TESTS="${ND_CAPTURE_MAX_TESTS:-4}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
case "$OUT_REL" in
    /*) OUT_DIR="$OUT_REL" ;;
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

# ── Mobile-layout coordinates (ND_MOBILE=1) ─────────────────────────────
# The mobile layout has:
#   - Top: AppBar 48px
#   - Under AppBar: TargetInputPanel (label → input → Run button)
#   - Results area below
#   - Bottom: 48px nav bar with 4 icon tabs (centered)
W=$SCREEN_W; H=$SCREEN_H
# Bottom nav — 48px, centered icons at 24px from bottom
NAV_Y=$((H - 24))
NAV_ROW_W=$((W - 40))
NAV_X0=$(((W - NAV_ROW_W) / 2))
# 4 tabs evenly spaced
NAV_DASH_X=$((W * 1 / 8))
NAV_DIAG_X=$((W * 3 / 8))
NAV_CFG_X=$((W  * 5 / 8))
NAV_SET_X=$((W  * 7 / 8))
# TargetInputPanel — below AppBar (48px) + label + input field
# Input field centre ~96px from top
INPUT_Y=96
# Run button centre ~150px from top (fills left side of input row)
RUN_Y=150
RUN_X=$((W / 4))
# Detail overlay: centred card, close button at top-right
DETAIL_CARD_W=$((W - 20))
DETAIL_X0=$(((W - DETAIL_CARD_W) / 2))
DETAIL_Y0=$(((H - 440) / 2))
[ "$DETAIL_Y0" -lt 0 ] && DETAIL_Y0=0
DETAIL_CLOSE_X=$((DETAIL_X0 + DETAIL_CARD_W - 20))
DETAIL_CLOSE_Y=$((DETAIL_Y0 + 20))
# Result row — first completed test row ~220px from top
RESULT_ROW_Y=220
RESULT_ROW_X=$((W / 2))
# Report button — at the bottom of dashboard (just above nav)
REPORT_BTN_X=$((W / 2))
REPORT_BTN_Y=$((H - 104))
# Desktop fallback: if Report button not found at mobile position, try lower
REPORT_BTN_Y2=$((H - 26))

log "layout ${W}x${H} mobile=1 target=$TARGET_URL nav_y=$NAV_Y"

# ── Slow HTTP server ──────────────────────────────────────────────────────
SERVER_PID=""
if command -v python3 >/dev/null 2>&1; then
    python3 "$PROJECT_DIR/scripts/screenshot/slow-http-server.py" "$SLOW_PORT" 2.0 \
        >/tmp/netdiag-android-server.log 2>&1 &
    SERVER_PID=$!
    log "slow-http-server started (pid $SERVER_PID, :$SLOW_PORT)"
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

# ── launch_app ────────────────────────────────────────────────────────────
launch_app() {
    if command -v openbox >/dev/null 2>&1; then
        openbox >/tmp/netdiag-openbox.log 2>&1 &
        OPENBOX_PID=$!
        sleep 1
    fi

    rm -f "$HOME/.config/robinhoo1973/NetDiagnostics.conf" 2>/dev/null || true
    ND_MAX_TESTS=$MAX_TESTS ND_MOBILE=1 "$APP" >/tmp/netdiag-app.log 2>&1 &
    APP_PID=$!
    log "app started (pid $APP_PID)"

    WIN_ID=""
    for i in $(seq 1 30); do
        if ! kill -0 "$APP_PID" 2>/dev/null; then
            die "app exited early — log tail:\n$(tail -20 /tmp/netdiag-app.log 2>/dev/null)"
        fi
        WIN_ID="$(xdotool search --onlyvisible --name 'NetDiagnostics' 2>/dev/null | head -1 || true)"
        [ -n "$WIN_ID" ] && break
        sleep 1
    done
    if [ -n "$WIN_ID" ]; then
        xdotool windowactivate "$WIN_ID" 2>/dev/null || true
        xdotool windowsize "$WIN_ID" "$SCREEN_W" "$SCREEN_H" 2>/dev/null || true
        xdotool windowmove "$WIN_ID" 0 0 2>/dev/null || true
        log "window $WIN_ID sized ${SCREEN_W}x${SCREEN_H} (~${i}s)"
    else
        die "no window after 30s"
    fi
}

kill_app() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null || true
    [ -n "$OPENBOX_PID" ] && kill "$OPENBOX_PID" 2>/dev/null || true
    APP_PID=""; OPENBOX_PID=""
    sleep 1
}

# ── Android status bar overlay ────────────────────────────────────────────
# Adds a 24px dark strip at the top with Android-style indicators (time,
# signal, battery) so screenshots are clearly identifiable as Android.
STATUS_H=24
add_android_status_bar() {
    local f="$1"
    local w h
    read w h <<< "$(identify -format '%w %h' "$f" 2>/dev/null || echo '0 0')"
    [ "$w" -le 0 ] && return

    # Dark status bar background
    convert "$f" \
        -fill "#1A1A2E" -draw "rectangle 0,0 $w,$STATUS_H" \
        -fill white -font Helvetica -pointsize 10 \
        -draw "text $((w - 80)),$((STATUS_H - 8)) '12:30'" \
        -fill "#4CAF50" -draw "rectangle $((w - 170)),$((STATUS_H - 15)) $((w - 150)),$((STATUS_H - 5))" \
        -fill "#4CAF50" -draw "rectangle $((w - 145)),$((STATUS_H - 15)) $((w - 125)),$((STATUS_H - 8))" \
        -fill "#4CAF50" -draw "rectangle $((w - 120)),$((STATUS_H - 15)) $((w - 100)),$((STATUS_H - 11))" \
        -fill "#4CAF50" -draw "rectangle $((w - 95)),$((STATUS_H - 15)) $((w - 75)),$((STATUS_H - 2))" \
        -fill white -pointsize 8 -draw "text $((w - 200)),$((STATUS_H - 7)) 'Android'" \
        "$f" 2>/dev/null || true
}

# ── Helpers ──────────────────────────────────────────────────────────────
check_alive() {
    if ! kill -0 "$APP_PID" 2>/dev/null; then
        die "app died — log tail:\n$(tail -20 /tmp/netdiag-app.log 2>/dev/null)"
    fi
}
frame_hash() { import -window root png:- 2>/dev/null | md5sum | cut -d' ' -f1; }
capture() {
    local stage="$1"
    local f="$OUT_DIR/$stage.png"
    check_alive
    import -window root "$f" 2>/dev/null || true
    if [ -s "$f" ]; then
        add_android_status_bar "$f"
        log "captured $stage"
    else
        warn "capture failed for $stage"
    fi
}
click() {
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
wait_stable() {
    local max="${1:-60}" t=0 a b
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
click_until_changed() {
    local label="$1" x="$2" y="$3" timeout="${4:-10}"
    local before
    before="$(frame_hash)"
    click "$x" "$y" "$label"
    local t=0
    while [ "$t" -lt "$timeout" ]; do
        sleep 1
        if [ "$before" != "$(frame_hash)" ]; then
            log "state change after $label ($((t+1))s)"
            return 0
        fi
        t=$((t + 1))
    done
    warn "$label: no state change within ${timeout}s — continuing"
    return 0  # never fail — no state change just means we're already there
}

# ══════════════════════════════════════════════════════════════════════════
# PASS A — 1..6
# ══════════════════════════════════════════════════════════════════════════
launch_app
sleep 2
capture 1-idle

# Navigate to diagnostics and run
click_until_changed "nav-diagnostics" "$NAV_DIAG_X" "$NAV_Y"
click "$((W/2))" "$INPUT_Y" "target-input"
type_url
click "$RUN_X" "$RUN_Y" "run-button"
sleep 2
capture 2-running

wait_stable 60
capture 3-complete

# Detail overlay via keyboard (Tab Tab Enter)
xdotool key Tab; sleep 0.3
xdotool key Tab; sleep 0.3
xdotool key Return; sleep 2
capture 4-detail
click "$DETAIL_CLOSE_X" "$DETAIL_CLOSE_Y" "detail-close"

# Dashboard
click_until_changed "nav-dashboard" "$NAV_DASH_X" "$NAV_Y"
sleep 1
capture 5-dashboard

# Report preview
click "$REPORT_BTN_X" "$REPORT_BTN_Y" "review-report"
sleep 3
capture 6-report
kill_app

# ══════════════════════════════════════════════════════════════════════════
# PASS B — 7-config → 8-settings
# ══════════════════════════════════════════════════════════════════════════
launch_app
click_until_changed "nav-config" "$NAV_CFG_X" "$NAV_Y"
capture 7-config

click_until_changed "nav-settings" "$NAV_SET_X" "$NAV_Y"
capture 8-settings

kill_app
log "done — screenshots in $OUT_DIR"
cleanup
exit 0
