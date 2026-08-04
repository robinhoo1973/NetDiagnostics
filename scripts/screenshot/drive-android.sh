#!/usr/bin/env bash
# =============================================================================
# drive-android.sh — External UI driver for NetDiagnostics (Android emulator)
# =============================================================================
# Drives the installed app like a real user via `adb shell input` (tap/swipe/
# text) and captures OS-level screenshots with `adb exec-out screencap -p`.
# ZERO app source changes.
#
# Stage sequence (shared): 1-idle → 2-running → 3-complete → 4-detail →
#   5-dashboard → 6-report → 7-config → 8-settings
#
# Coordinates are computed from the emulator's PHYSICAL resolution (wm size)
# and density (wm density): logical dp × density/160 = physical px, matching
# Qt's own dp mapping.  The driver also puts the device into immersive full
# screen (hides status/nav bars) so the app window == full screen and the
# QML layout maps 1:1 — otherwise system bars shift all coordinates.
#
# The emulator reaches the host's slow-http-server via 10.0.2.2 (Android
# emulator host-loopback alias), so the typed target defaults to that.
#
# Usage (one booted+installed device):
#   bash scripts/screenshot/drive-android.sh resources/doc/screenshot/android/phone
#
# Env overrides:
#   ND_ANDROID_SERIAL  adb serial (default: first device)
#   ND_ANDROID_PKG     app package (default com.netdiagnostic.app)
#   ND_ANDROID_ACT     activity (default org.qtproject.qt6.android.bindings.QtActivity)
#   ND_TARGET          target URL (default http://10.0.2.2:8899)
# =============================================================================
set -euo pipefail

OUT_REL="${1:-resources/doc/screenshot/android/phone}"
SERIAL="${ND_ANDROID_SERIAL:-$(adb devices | awk 'NR==2{print $1}')}"
PKG="${ND_ANDROID_PKG:-com.netdiagnostic.app}"
ACT="${ND_ANDROID_ACT:-org.qtproject.qt6.android.bindings.QtActivity}"
TARGET="${ND_TARGET:-http://10.0.2.2:8899}"
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

command -v adb >/dev/null 2>&1 || die "missing: adb"
[ -n "$SERIAL" ] || die "no adb device found"
adb_cmd() { adb -s "$SERIAL" "$@"; }

# ── Physical resolution + density → DPR ─────────────────────────────────
SIZE_RAW="$(adb_cmd shell wm size 2>/dev/null | grep -oE '[0-9]+x[0-9]+' | head -1 || true)"
W="${SIZE_RAW%%x*}"; H="${SIZE_RAW##*x}"
[ -n "$W" ] && [ -n "$H" ] || die "cannot read wm size (got '$SIZE_RAW')"
DPI="$(adb_cmd shell wm density 2>/dev/null | grep -oE '[0-9]+' | head -1 || echo 160)"
[ -n "$DPI" ] || DPI=160
DPR=$(python3 -c "print($DPI/160.0)" 2>/dev/null || awk "BEGIN{print $DPI/160}")
log "device ${W}x${H} density=${DPI}dpi dpr=$DPR"

# ── Device config: animations off + immersive full screen ───────────────
# On no-KVM software emulators, sys.boot_completed becomes 1 while the
# settings service is still starting; retry until it responds (the
# android-emulator-runner action's own settings steps failed here with
# "cmd: Can't find service: settings" — so we configure the device
# ourselves, patiently).
for _ in $(seq 1 30); do
    if adb_cmd shell settings put global window_animation_scale 0 >/dev/null 2>&1; then
        adb_cmd shell settings put global transition_animation_scale 0 >/dev/null 2>&1 || true
        adb_cmd shell settings put global animator_duration_scale 0 >/dev/null 2>&1 || true
        adb_cmd shell settings put system screen_off_timeout 2147483647 >/dev/null 2>&1 || true
        log "animations disabled, settings service ready"
        break
    fi
    sleep 5
done
# Immersive full screen: app window == full screen → 1:1 coordinate map
adb_cmd shell settings put global policy_control immersive.full=* >/dev/null 2>&1 || true
sleep 1

# ── Mobile-layout coordinates (logical dp × DPR = physical px) ──────────
nav_y=$(python3 -c "print(int($H - 24 * $DPR))")
nav_dash_x=$(python3 -c "print(int($W * 1 / 8))")
nav_diag_x=$(python3 -c "print(int($W * 3 / 8))")
nav_cfg_x=$(python3 -c "print(int($W * 5 / 8))")
nav_set_x=$(python3 -c "print(int($W * 7 / 8))")
input_y=$(python3 -c "print(int(${INPUT_Y:-96} * $DPR))")
run_y=$(python3 -c "print(int(${RUN_Y:-150} * $DPR))")
run_x=$(python3 -c "print(int($W / 4))")
card_w=$(python3 -c "print(int(min(700, $W/$DPR - 20) * $DPR))")
card_x0=$(python3 -c "print(int(($W - $card_w) / 2))")
card_y0=$(python3 -c "print(int(max(0, ($H - 620*$DPR) / 2)))")
detail_close_x=$(python3 -c "print(int($card_x0 + $card_w - 30*$DPR))")
detail_close_y=$(python3 -c "print(int($card_y0 + 30*$DPR))")
result_row_x=$(python3 -c "print(int($W / 2))")
result_row_y=$(python3 -c "print(int(${RESULT_ROW_Y:-220} * $DPR))")
report_btn_x=$(python3 -c "print(int($W / 2))")
report_btn_y=$(python3 -c "print(int($H - 56 * $DPR))")

log "nav_y=$nav_y input_y=$input_y run=$run_x,$run_y report=$report_btn_x,$report_btn_y"

# ── Helpers ─────────────────────────────────────────────────────────────
launch_app() {
    adb_cmd shell am start -n "$PKG/$ACT" >/dev/null 2>&1 || true
    log "launched $PKG/$ACT"
    sleep 6
}
stop_app() {
    adb_cmd shell am force-stop "$PKG" >/dev/null 2>&1 || true
    sleep 2
}
tap() { # tap <x> <y> <label>
    adb_cmd shell input tap "$1" "$2" >/dev/null 2>&1 || true
    log "tapped $3 ($1,$2)"
    sleep 1
}
swipe_up() { # real touch drag → Flickable scrolls (unlike synthetic wheel)
    local steps="${1:-12}"
    for _ in $(seq 1 "$steps"); do
        adb_cmd shell input swipe "$((W / 2))" "$((H * 3 / 4))" "$((W / 2))" "$((H / 3))" 300 >/dev/null 2>&1 || true
        sleep 0.3
    done
    sleep 2
}
type_url() {
    tap "$((W / 2))" "$input_y" "target-input"
    adb_cmd shell "input text $TARGET" >/dev/null 2>&1 || true
    log "typed $TARGET"
    sleep 1
}
capture() { # capture <stage>
    local stage="$1"
    local f="$OUT_DIR/$stage.png"
    adb_cmd exec-out screencap -p > "$f" 2>/dev/null || true
    if [ -s "$f" ]; then
        log "captured $stage -> $f"
    else
        warn "capture failed for $stage"
    fi
}
frame_hash() {
    local f; f="$(mktemp -t ndframe).png"
    adb_cmd exec-out screencap -p > "$f" 2>/dev/null || true
    md5sum "$f" 2>/dev/null | cut -d' ' -f1
    rm -f "$f"
}
wait_stable() { # wait up to N s for two identical consecutive frames
    local max="${1:-240}" t=0 a b
    while [ "$t" -lt "$max" ]; do
        a="$(frame_hash)"; sleep 5; b="$(frame_hash)"
        if [ -n "$a" ] && [ "$a" = "$b" ]; then
            log "screen stable after ~$((t + 5))s"
            sleep 2
            return 0
        fi
        t=$((t + 5))
    done
    warn "screen not stable within ${max}s — proceeding anyway"
    return 0
}

# ── Slow deterministic HTTP server on the HOST (emulator reaches it via ──
#    10.0.2.2) — keeps the run alive long enough for the Running shot.
SERVER_PID=""
if command -v python3 >/dev/null 2>&1; then
    python3 "$PROJECT_DIR/scripts/screenshot/slow-http-server.py" "$SLOW_PORT" 2.0 \
        >/tmp/netdiag-android-server.log 2>&1 &
    SERVER_PID=$!
    log "slow-http-server started (pid $SERVER_PID, host :$SLOW_PORT)"
fi
cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null || true
    adb_cmd shell settings put global policy_control null 2>/dev/null || true
}
trap cleanup EXIT

# ══════════════════════════════════════════════════════════════════════════
# PASS A — 1..6 (report overlay not dismissible; relaunch for B)
# ══════════════════════════════════════════════════════════════════════════
launch_app
sleep 2
capture 1-idle

tap "$nav_diag_x" "$nav_y" "nav-diagnostics"
type_url
tap "$run_x" "$run_y" "run-button"
sleep 2
capture 2-running

wait_stable 240
capture 3-complete

tap "$result_row_x" "$result_row_y" "result-row"
sleep 2
capture 4-detail
tap "$detail_close_x" "$detail_close_y" "detail-close"

tap "$nav_dash_x" "$nav_y" "nav-dashboard"
capture 5-dashboard

swipe_up 14
tap "$report_btn_x" "$report_btn_y" "review-report"
sleep 3
capture 6-report
stop_app

# ══════════════════════════════════════════════════════════════════════════
# PASS B — 7-config → 8-settings
# ══════════════════════════════════════════════════════════════════════════
launch_app
tap "$nav_cfg_x" "$nav_y" "nav-config"
capture 7-config

tap "$nav_set_x" "$nav_y" "nav-settings"
capture 8-settings

stop_app
log "done — screenshots in $OUT_DIR"
cleanup
exit 0
