#!/usr/bin/env bash
# =============================================================================
# drive-ios.sh — External UI driver for NetDiagnostics (iOS Simulator)
# =============================================================================
# Installs and launches the app on a booted iOS Simulator, drives it like a
# real user with idb (tap/swipe/text), and captures OS-level screenshots per
# stage with `xcrun simctl io booted screenshot`.  ZERO app source changes.
#
# Stage sequence (shared): 1-idle → 2-running → 3-complete → 4-detail →
#   5-dashboard → 6-report → 7-config → 8-settings
#
# The mobile layout (ThemeEngine.isMobile) differs from desktop: 48px AppBar,
# a 48px bottom nav with FOUR evenly-spaced icon tabs, and the TargetInputPanel
# (label → input → Run button).  Coordinates are computed from the device
# W x H so any simulator size works; every value is env-overridable.
#
# Usage (after `xcrun simctl boot <device>` + install):
#   ND_APP=build-ios/.../net_diagnostics.app \
#   ND_BUNDLE_ID=com.netdiagnostic.app \
#   bash scripts/screenshot/drive-ios.sh resources/doc/screenshot/ios/phone/6.5
#
# Requires: xcrun (simctl), idb (brew install idb-companion + pip install fb-idb)
# =============================================================================
set -euo pipefail

OUT_REL="${1:-resources/doc/screenshot/ios/phone}"
APP="${ND_APP:?set ND_APP to the built .app bundle}"
BUNDLE_ID="${ND_BUNDLE_ID:?set ND_BUNDLE_ID (from Info.plist CFBundleIdentifier)}"
SLOW_PORT="${SLOW_PORT:-8899}"

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

command -v xcrun >/dev/null 2>&1 || die "missing: xcrun"
command -v idb >/dev/null 2>&1 || warn "idb not found — taps/swipes will fail"
[ -d "$APP" ] || die "app bundle not found: $APP"

# ── Device size (points) from the booted simulator ────────────────────────
read -r W H <<< "$(idb list-targets --json 2>/dev/null \
    | python3 -c "import sys,json; d=json.load(sys.stdin); t=[x for x in d if x.get('state')=='Booted']; print(t[0]['width'], t[0]['height']) if t else print(393,852)" 2>/dev/null \
    || echo '393 852')"
W="${W:-393}"; H="${H:-852}"
log "device size ${W}x${H}"

# ── Mobile-layout coordinates (all env-overridable) ──────────────────────
NAV_Y=$((H - 24))                          # 48px icon nav, 4 tabs
NAV_DASH_X=$((W * 1 / 8))
NAV_DIAG_X=$((W * 3 / 8))
NAV_CFG_X=$((W * 5 / 8))
NAV_SET_X=$((W * 7 / 8))
INPUT_Y="${INPUT_Y:-96}"                   # TargetInputPanel input row centre
RUN_Y="${RUN_Y:-150}"                      # Run button centre
RUN_X=$((W * 1 / 4))                       # Run fills left half w/ stop beside it
# Detail overlay: centred card, close at card top-right.
DETAIL_CARD_W=700
[ "$W" -lt 720 ] && DETAIL_CARD_W=$((W - 20))
DETAIL_X0=$(((W - DETAIL_CARD_W) / 2))
DETAIL_Y0=$(((H - 620) / 2))
[ "$DETAIL_Y0" -lt 0 ] && DETAIL_Y0=0
DETAIL_CLOSE_X=$((DETAIL_X0 + DETAIL_CARD_W - 30))
DETAIL_CLOSE_Y=$((DETAIL_Y0 + 30))
# Dashboard "Review Report" button — below the fold on phones; we swipe to the
# bottom of the dashboard Flickable first (idb swipe = real touch drag).
REPORT_BTN_X=$((W / 2))
REPORT_BTN_Y=$((NAV_Y - 56))
RESULT_ROW_Y="${RESULT_ROW_Y:-220}"
RESULT_ROW_X=$((W / 2))

log "nav_y=$NAV_Y input_y=$INPUT_Y run=$RUN_X,$RUN_Y report_btn=$REPORT_BTN_X,$REPORT_BTN_Y"

# ── Launch helpers ───────────────────────────────────────────────────────
launch_app() {
    xcrun simctl install booted "$APP"
    log "installed $BUNDLE_ID"
    xcrun simctl launch booted "$BUNDLE_ID" >/dev/null
    sleep 6
}
terminate_app() {
    xcrun simctl terminate booted "$BUNDLE_ID" 2>/dev/null || true
    sleep 1
}
tap() { # tap <x> <y> <label>
    idb ui tap "$1" "$2" || true
    log "tapped $3 ($1,$2)"
    sleep 1
}
swipe_up() { # swipe_up — real touch drag to scroll the Flickable
    local steps="${1:-12}"
    for _ in $(seq 1 "$steps"); do
        idb ui swipe "$((W / 2))" "$((H * 3 / 4))" "$((W / 2))" "$((H / 3))" || true
        sleep 0.3
    done
    sleep 2
}
type_url() {
    tap "$((W / 2))" "$INPUT_Y" "target-input"
    idb ui text "http://localhost:$SLOW_PORT" || true
    log "typed http://localhost:$SLOW_PORT"
    sleep 1
}
capture() { # capture <stage>
    local stage="$1"
    local f="$OUT_DIR/$stage.png"
    xcrun simctl io booted screenshot "$f" >/dev/null 2>&1 || true
    if [ -s "$f" ]; then
        log "captured $stage -> $f"
    else
        warn "capture failed for $stage"
    fi
}
frame_hash() {
    local f; f="$(mktemp -t ndframe).png"
    xcrun simctl io booted screenshot "$f" >/dev/null 2>&1 || true
    md5 -q "$f" 2>/dev/null || md5sum "$f" | cut -d' ' -f1
    rm -f "$f"
}
wait_stable() {
    local max="${1:-60}" t=0 a b
    while [ "$t" -lt "$max" ]; do
        a="$(frame_hash)"; sleep 3; b="$(frame_hash)"
        if [ -n "$a" ] && [ "$a" = "$b" ]; then
            log "screen stable after ~$((t + 3))s"
            sleep 2
            return 0
        fi
        t=$((t + 3))
    done
    warn "screen not stable within ${max}s — proceeding anyway"
    return 0
}

# ── Slow deterministic HTTP server (keeps the run alive for the Running shot) ─
SERVER_PID=""
if command -v python3 >/dev/null 2>&1; then
    python3 "$PROJECT_DIR/scripts/screenshot/slow-http-server.py" "$SLOW_PORT" 2.0 \
        >/tmp/netdiag-server.log 2>&1 &
    SERVER_PID=$!
    log "slow-http-server started (pid $SERVER_PID, localhost:$SLOW_PORT)"
fi
cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null || true
}
trap cleanup EXIT

# ══════════════════════════════════════════════════════════════════════════
# PASS A — 1..6 (report overlay not dismissible; relaunch for B)
# ══════════════════════════════════════════════════════════════════════════
launch_app
sleep 2
capture 1-idle

tap "$NAV_DIAG_X" "$NAV_Y" "nav-diagnostics"
type_url
tap "$RUN_X" "$RUN_Y" "run-button"
sleep 2
capture 2-running

wait_stable 60
capture 3-complete

tap "$RESULT_ROW_X" "$RESULT_ROW_Y" "result-row"
sleep 2
capture 4-detail
tap "$DETAIL_CLOSE_X" "$DETAIL_CLOSE_Y" "detail-close"

tap "$NAV_DASH_X" "$NAV_Y" "nav-dashboard"
capture 5-dashboard

# Report preview: swipe to the dashboard bottom, then tap Review Report.
swipe_up 14
tap "$REPORT_BTN_X" "$REPORT_BTN_Y" "review-report"
sleep 3
capture 6-report
terminate_app

# ══════════════════════════════════════════════════════════════════════════
# PASS B — 7-config → 8-settings
# ══════════════════════════════════════════════════════════════════════════
launch_app
tap "$NAV_CFG_X" "$NAV_Y" "nav-config"
capture 7-config

tap "$NAV_SET_X" "$NAV_Y" "nav-settings"
capture 8-settings

terminate_app
log "done — screenshots in $OUT_DIR"
cleanup
exit 0
