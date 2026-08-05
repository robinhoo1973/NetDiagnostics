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
# 5WHY fixes applied:
#   1. URL input: `adb shell input text` cannot type :// etc.
#      → Use clipboard paste: `cmd clipboard set` + KEYCODE_PASTE (API 30+)
#   2. App startup: sleep 6 insufficient for software emulation (no KVM)
#      → Poll dumpsys window until app is foreground (up to 120s)
#   3. Coordinate math: (H-24)*DPR overflows screen → H - 24*DPR
#   4. No tap verification → added click_until_changed with frame hashing
#   5. Slow HTTP server: emulator reaches host via 10.0.2.2
#
# Usage (one booted+installed device):
#   bash scripts/screenshot/drive-android.sh resources/doc/screenshot/android/phone
#
# Env overrides:
#   ND_ANDROID_SERIAL  adb serial (default: first device)
#   ND_ANDROID_PKG     app package (default com.netdiagnostic.app)
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

# DPR = density / 160 (dp-to-px ratio)
DPR=$(python3 -c "print($DPI/160.0)" 2>/dev/null || awk "BEGIN{print $DPI/160}")
log "device ${W}x${H} density=${DPI}dpi dpr=$DPR"

# ── Device config: animations off + immersive full screen ───────────────
log "configuring device (animations, screen timeout, immersive)..."
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

# Dismiss any "System UI not responding" dialogs on slow emulators
adb_cmd shell input keyevent 3 2>/dev/null || true   # KEYCODE_HOME
sleep 1

# ── Coordinates (physical px) ────────────────────────────────────────────
# 5WHY: nav_y = (H-24)*DPR overflowed screen → H - 24*DPR (nav centre at
# 24dp from bottom, converted to physical px from top-left origin)
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
# 5WHY: report_btn_y = (H-56)*DPR overflowed → H - 56*DPR
report_btn_y=$(python3 -c "print(int($H - 56 * $DPR))")

log "nav_y=$nav_y input_y=$input_y run=$run_x,$run_y report=$report_btn_x,$report_btn_y"

# ── Helpers ─────────────────────────────────────────────────────────────
launch_app() {
    adb_cmd shell am start -n "$PKG/$ACT" >/dev/null 2>&1 || true
    log "launched $PKG/$ACT"
}

# 5WHY: sleep 6 was far too short for software emulation (20-60s startup).
# Poll dumpsys window until our package is the focused window.
wait_for_app() {
    local max="${1:-120}" t=0
    log "waiting for app to come to foreground (max ${max}s)..."
    while [ "$t" -lt "$max" ]; do
        local focus
        focus=$(adb_cmd shell dumpsys window 2>/dev/null | grep -i "mCurrentFocus" | grep "$PKG" || true)
        if [ -n "$focus" ]; then
            log "app in foreground after ~${t}s"
            sleep 4  # let UI fully render
            return 0
        fi
        # Also check if the window is visible even if not focused
        if adb_cmd shell dumpsys window windows 2>/dev/null | grep -q "$PKG"; then
            log "app window visible after ~${t}s"
            sleep 4
            return 0
        fi
        sleep 3
        t=$((t + 3))
    done
    warn "app not in foreground after ${max}s — proceeding anyway"
    return 1
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

# 5WHY: adb shell input text cannot type URLs (:// and other special chars
# are eaten by the shell).  Use clipboard + paste instead (API 30+).
# Fall back to hex-escaped input text if clipboard approach fails.
type_url() {
    tap "$((W / 2))" "$input_y" "target-input"
    sleep 1

    # Primary: clipboard + paste (API 30+, available on our AVD)
    local clipped
    clipped=$(adb_cmd shell "cmd clipboard set '$TARGET' 2>/dev/null && echo ok" || true)
    if [ "$clipped" = "ok" ]; then
        adb_cmd shell input keyevent 279  # KEYCODE_PASTE
        log "pasted URL via clipboard: $TARGET"
    else
        # Fallback: hex-escape every character so the shell passes it through
        log "clipboard set unavailable — using hex-escaped input text"
        local escaped="$'"
        for ((i=0; i<${#TARGET}; i++)); do
            local c="${TARGET:$i:1}"
            if [ "$c" = ' ' ]; then
                escaped+='%s'
            else
                escaped+=$(printf '\\x%02x' "'$c")
            fi
        done
        escaped+="'"
        adb_cmd shell input text "$escaped" 2>/dev/null || true
        log "typed URL via hex-escape: $TARGET"
    fi
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

# 5WHY: without state verification, taps fail silently and every screenshot
# shows the same idle screen.  click_until_changed verifies the tap actually
# caused a visible state change.
click_until_changed() { # click_until_changed <x> <y> <label> [timeout_secs]
    local x="$1" y="$2" label="$3" timeout="${4:-15}"
    local before
    before="$(frame_hash)"
    tap "$x" "$y" "$label"
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
wait_for_app 120
capture 1-idle

# ── Stage 2: Running ─────────────────────────────────────────────────────
click_until_changed "$nav_diag_x" "$nav_y" "nav-diagnostics"
type_url
click_until_changed "$run_x" "$run_y" "run-button" 20
sleep 2
capture 2-running

# ── Stage 3: Complete — wait for progress to stop ────────────────────────
wait_stable 240
capture 3-complete

# ── Stage 4: Detail overlay ──────────────────────────────────────────────
click_until_changed "$result_row_x" "$result_row_y" "result-row"
sleep 2
capture 4-detail
tap "$detail_close_x" "$detail_close_y" "detail-close"
sleep 1

# ── Stage 5: Dashboard ───────────────────────────────────────────────────
click_until_changed "$nav_dash_x" "$nav_y" "nav-dashboard"
capture 5-dashboard

# ── Stage 6: Report preview ──────────────────────────────────────────────
swipe_up 14
click_until_changed "$report_btn_x" "$report_btn_y" "review-report" 15
sleep 3
capture 6-report
stop_app

# ══════════════════════════════════════════════════════════════════════════
# PASS B — 7-config → 8-settings (fresh launch, no overlay to dismiss)
# ══════════════════════════════════════════════════════════════════════════
launch_app
wait_for_app 90
click_until_changed "$nav_cfg_x" "$nav_y" "nav-config"
capture 7-config

click_until_changed "$nav_set_x" "$nav_y" "nav-settings"
capture 8-settings

stop_app
log "done — screenshots in $OUT_DIR"
cleanup
exit 0
