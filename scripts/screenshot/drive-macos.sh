#!/usr/bin/env bash
# =============================================================================
# drive-macos.sh — External UI driver for NetDiagnostics (macOS desktop)
# =============================================================================
# Launches the app, drives it like a real user (cliclick, or AppleScript
# System Events as fallback) and captures OS-level screenshots per stage.
# ZERO app source modifications — pure OS-level automation.
#
# Stage sequence (shared): 1-idle → 2-running → 3-complete → 4-detail →
#   5-dashboard → 6-report → 7-config → 8-settings
#
# Usage:
#   bash scripts/screenshot/drive-macos.sh ./build/net_diagnostics \
#       resources/doc/screenshot/macos
#
# Env overrides (all optional):
#   ND_CAPTURE_TARGET    target URL       (default http://localhost:8888)
#   ND_CAPTURE_MAX_TESTS tests per group  (default 4)
#   ND_RESULT_ROW_Y      first result row (default 225)
#
# Notes:
#   - macOS CI GUI input injection needs the runner's GUI session; cliclick
#     is preferred (brew install cliclick), AppleScript System Events used
#     as fallback.  If neither can inject, stages still capture (may show
#     the wrong screen) and warnings are logged.
#   - The app is frameless + showMaximized → window fills the desktop.
#     Coordinates derive from the actual screen size (fixed internal layout).
# =============================================================================
set -euo pipefail

APP="${1:?usage: drive-macos.sh <app-binary> [out-dir]}"
OUT_REL="${2:-resources/doc/screenshot/macos}"
TARGET_URL="${ND_CAPTURE_TARGET:-http://localhost:8888}"
MAX_TESTS="${ND_CAPTURE_MAX_TESTS:-4}"
RESULT_ROW_Y="${ND_RESULT_ROW_Y:-0}"

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

command -v screencapture >/dev/null 2>&1 || die "missing: screencapture"
command -v cliclick >/dev/null 2>&1 || warn "cliclick not found — falling back to AppleScript (may need Accessibility permission)"

# ── Screen size (desktop bounds) ─────────────────────────────────────────
screen_bounds="$(osascript -e 'tell application "Finder" to get bounds of window of desktop' 2>/dev/null || true)"
if [[ "$screen_bounds" =~ ^([0-9]+),\ ([0-9]+),\ ([0-9]+),\ ([0-9]+)$ ]]; then
    SCREEN_W=$((BASH_REMATCH[3] - BASH_REMATCH[1]))
    SCREEN_H=$((BASH_REMATCH[4] - BASH_REMATCH[2]))
else
    SCREEN_W=1920; SCREEN_H=1080
    warn "could not read screen bounds — assuming ${SCREEN_W}x${SCREEN_H}"
fi

# ── Coordinates (window == full screen; fixed internal layout) ───────────
W=$SCREEN_W; H=$SCREEN_H; WX=0; WY=0
TOOLBAR_CY=$((WY + 74))
HOST_CX=$((WX + (W + 94) / 2))
RUN_CX=$((WX + W - 26))
NAV_Y=$((WY + H - 28))
NAV_X0=$((WX + (W - 539) / 2))
NAV_DASH_CX=$((NAV_X0 + 60))
NAV_DIAG_CX=$((NAV_X0 + 192))
NAV_CFG_CX=$((NAV_X0 + 342))
NAV_SET_CX=$((NAV_X0 + 482))
DETAIL_X0=$((WX + (W - 700) / 2))
DETAIL_Y0=$((WY + (H - 620) / 2))
DETAIL_CLOSE_CX=$((DETAIL_X0 + 670))
DETAIL_CLOSE_CY=$((DETAIL_Y0 + 30))
REPORT_CLOSE_CX=$((WX + W - 37))
REPORT_CLOSE_CY=$((WY + 30))
REPORT_BTN_X=$((WX + W / 2))
REPORT_BTN_Y=$((WY + H - 104))
[ "$RESULT_ROW_Y" -eq 0 ] && RESULT_ROW_Y=$((WY + 225))
RESULT_ROW_X=$((WX + 400))

log "layout ${W}x${H} target=$TARGET_URL max_tests=$MAX_TESTS out=$OUT_DIR"

# ── Input backend ────────────────────────────────────────────────────────
CLICLICK=""
command -v cliclick >/dev/null 2>&1 && CLICLICK="$(command -v cliclick)"
click() { # click <x> <y> <label>
    if [ -n "$CLICLICK" ]; then
        "$CLICLICK" "c:$1,$2" || true
    else
        osascript -e "tell application \"System Events\" to click at {$1, $2}" 2>/dev/null || true
    fi
    log "clicked $3 ($1,$2)"
    sleep 1
}
type_url() {
    if [ -n "$CLICLICK" ]; then
        "$CLICLICK" "t:$TARGET_URL" || true
    else
        osascript -e "tell application \"System Events\" to keystroke \"$TARGET_URL\"" 2>/dev/null || true
    fi
    log "typed target: $TARGET_URL"
    sleep 1
}
nav_to() { click "$1" "$NAV_Y" "nav-$2"; sleep 1; }
scroll_down() {
    if [ -n "$CLICLICK" ]; then
        "$CLICLICK" "w:-30" "w:-30" "w:-30" || true
    else
        osascript -e 'tell application "System Events" to key code 125' 2>/dev/null || true   # PageDown-ish
        sleep 1
        osascript -e 'tell application "System Events" to key code 125' 2>/dev/null || true
    fi
    sleep 2
}
capture() { # capture <stage>
    local stage="$1"
    local f="$OUT_DIR/$stage.png"
    screencapture -x "$f" 2>/dev/null || true
    if [ -s "$f" ]; then
        log "captured $stage -> $f"
    else
        warn "capture failed for $stage"
    fi
}
frame_hash() {
    local f; f="$(mktemp -t ndframe).png"
    screencapture -x "$f" 2>/dev/null || true
    md5 -q "$f" 2>/dev/null || md5sum "$f" 2>/dev/null | cut -d' ' -f1
    rm -f "$f"
}
click_until_changed() { # click_until_changed <label> <x> <y> [timeout_secs]
    local label="$1" x="$2" y="$3" timeout="${4:-10}"
    local before
    before="$(frame_hash)"
    click "$x" "$y" "$label"
    local t=0
    while [ "$t" -lt "$timeout" ]; do
        sleep 1
        if [ "$before" != "$(frame_hash)" ]; then
            log "state change detected after $label ($((t+1))s)"
            return 0
        fi
        t=$((t + 1))
    done
    warn "$label: no state change within ${timeout}s"
    return 1
}
wait_stable() { # wait_stable <max-seconds>
    local max="${1:-150}" t=0 a b
    while [ "$t" -lt "$max" ]; do
        screencapture -x /tmp/netdiag-frame.png 2>/dev/null || true
        a="$(md5 -q /tmp/netdiag-frame.png 2>/dev/null || true)"
        sleep 4
        screencapture -x /tmp/netdiag-frame.png 2>/dev/null || true
        b="$(md5 -q /tmp/netdiag-frame.png 2>/dev/null || true)"
        if [ -n "$a" ] && [ "$a" = "$b" ]; then
            log "screen stable after ~$((t + 4))s"
            sleep 2
            return 0
        fi
        t=$((t + 4))
    done
    warn "screen not stable within ${max}s — proceeding anyway"
}

# ── Slow deterministic HTTP server (keeps the run alive for the Running shot) ─
SLOW_PORT="${SLOW_PORT:-8899}"
SERVER_PID=""
if command -v python3 >/dev/null 2>&1; then
    python3 "$PROJECT_DIR/scripts/screenshot/slow-http-server.py" "$SLOW_PORT" 2.0 \
        >/tmp/netdiag-server.log 2>&1 &
    SERVER_PID=$!
    log "slow-http-server started (pid $SERVER_PID, localhost:$SLOW_PORT)"
else
    warn "python3 not found — server skipped"
fi

APP_PID=""
cleanup() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null || true
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null || true
}
trap cleanup EXIT

launch_app() {
    # Fresh QSettings → deterministic default config (all tests enabled).
    rm -f "$HOME/Library/Preferences/robinhoo1973.NetDiagnostics.plist" 2>/dev/null || true
    ND_MAX_TESTS=$MAX_TESTS "$APP" >/tmp/netdiag-app.log 2>&1 &
    APP_PID=$!
    log "app started (pid $APP_PID)"
    sleep 8
    # Bring the app to the foreground so clicks/keys land on it.
    osascript -e 'tell application "System Events" to set frontmost of first process whose unix id is '"$APP_PID" 2>/dev/null || true
    sleep 2
}
kill_app() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null || true
    APP_PID=""
    sleep 1
}

# ══════════════════════════════════════════════════════════════════════════
# PASS A — 1..6 (report preview is the last stage; its overlay cannot be
# dismissed reliably on macOS CI, so we relaunch for the remaining screens).
# ══════════════════════════════════════════════════════════════════════════
launch_app
sleep 2
capture 1-idle

# ── Stage 2: Running ─────────────────────────────────────────────────────
click "$HOST_CX" "$TOOLBAR_CY" "host-field"
type_url
click "$RUN_CX" "$TOOLBAR_CY" "run-button"
sleep 2
capture 2-running

# ── Stage 3: Complete — wait for screen to stop changing (diagnostic done) ──
wait_stable 60
capture 3-complete

# ── Stage 4: Detail (keyboard: after Run the focus is on the run button →
# Tab to first result row, Enter opens the overlay)
if command -v xdotool >/dev/null 2>&1; then
    xdotool key Tab; sleep 0.3
    xdotool key Tab; sleep 0.3
    xdotool key Return; sleep 2
else
    osascript -e 'tell application "System Events" to key code 48' 2>/dev/null || true   # Tab
    sleep 0.3
    osascript -e 'tell application "System Events" to key code 48' 2>/dev/null || true
    sleep 0.3
    osascript -e 'tell application "System Events" to key code 36' 2>/dev/null || true  # Return
    sleep 2
fi
capture 4-detail
click "$DETAIL_CLOSE_CX" "$DETAIL_CLOSE_CY" "detail-close"

# ── Stage 5: Dashboard ───────────────────────────────────────────────────
click_until_changed "nav-dashboard" "$NAV_DASH_CX" "$NAV_Y"
capture 5-dashboard

# ── Stage 6: Report preview ──────────────────────────────────────────────
click "$REPORT_BTN_X" "$REPORT_BTN_Y" "review-report"
sleep 3
capture 6-report
kill_app

# ══════════════════════════════════════════════════════════════════════════
# PASS B — 7-config → 8-settings (fresh launch)
# ══════════════════════════════════════════════════════════════════════════
launch_app
nav_to "$NAV_CFG_CX" "config"
capture 7-config

nav_to "$NAV_SET_CX" "settings"
capture 8-settings

kill_app
log "done — screenshots in $OUT_DIR"
cleanup
exit 0
