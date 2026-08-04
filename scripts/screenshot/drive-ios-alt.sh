#!/usr/bin/env bash
# =============================================================================
# drive-ios-alt.sh — macOS-native mobile-layout screenshot driver
# =============================================================================
# Because pre-built Qt for iOS cannot link against the iPhoneSimulator SDK
# (Apple's linker rejects device-platform .o files in a simulator binary),
# we run the production macOS build WITH ND_MOBILE=1 so the app renders its
# mobile layout (bottom nav, TargetInputPanel).  This script resizes the
# window to iPhone-scale proportions and drives the mobile-layout UI with
# cliclick (or osascript as fallback).
#
# The window is sized to WxH (default 393x852 — iPhone 6.5" logical points)
# and positioned at (0,0) for deterministic coordinate mapping.
#
# Stage sequence (shared): 1-idle → 2-running → 3-complete → 4-detail →
#   5-dashboard → 6-report → 7-config → 8-settings
#
# Usage:
#   ND_MOBILE=1 ND_CAPTURE_TARGET=http://localhost:8899 \
#     bash scripts/screenshot/drive-ios-alt.sh build/net_diagnostics ./resources/doc/screenshot/ios/phone/6.5
# =============================================================================
set -euo pipefail

APP="${1:-./build/net_diagnostics}"
OUT_REL="${2:-resources/doc/screenshot/ios/phone/6.5}"

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

# ── Config ────────────────────────────────────────────────────────────────
WIN_W="${WIN_W:-393}"
WIN_H="${WIN_H:-852}"
TARGET="${ND_CAPTURE_TARGET:-http://localhost:8899}"
SLOW_PORT="${SLOW_PORT:-8899}"

# ── Mobile-layout coordinates (physical px for the resized window) ───────
TOOLBAR_Y=48                                                                      # app bar / toolbar
NAV_Y=$((WIN_H - 26))                                                             # bottom nav bar (4 tabs, 48px)
NAV_DASH_X=$((WIN_W * 1 / 8))
NAV_DIAG_X=$((WIN_W * 3 / 8))
NAV_CFG_X=$((WIN_W  * 5 / 8))
NAV_SET_X=$((WIN_W  * 7 / 8))
INPUT_Y=96                                                                        # TargetInputPanel input centre
RUN_Y=150                                                                         # Run button centre
RUN_X=$((WIN_W / 4))
# Detail overlay: centred card
CARD_W=$((WIN_W - 20))
CARD_X0=$(((WIN_W - CARD_W) / 2))
CARD_Y0=$(((WIN_H - 440) / 2))                                                    # card height estimate
[ "$CARD_Y0" -lt 0 ] && CARD_Y0=0
DETAIL_CLOSE_X=$((CARD_X0 + CARD_W - 20))
DETAIL_CLOSE_Y=$((CARD_Y0 + 20))
RESULT_ROW_X=$((WIN_W / 2))
RESULT_ROW_Y="${RESULT_ROW_Y:-220}"
REPORT_BTN_X=$((WIN_W / 2))
REPORT_BTN_Y="${REPORT_BTN_Y:-780}"

log "window ${WIN_W}x${WIN_H} target=$TARGET"

# ── macOS input injection ─────────────────────────────────────────────────
CLICK="cliclick"
if ! command -v "$CLICK" >/dev/null 2>&1; then
    CLICK=""
    # osascript fallback — less reliable but works without extra brew deps
    osa_click() { osascript -e "tell app \"System Events\" to click at {$(($1)),$(($2))}" 2>/dev/null || true; }
    osa_type() { osascript -e "tell app \"System Events\" to keystroke \"$1\"" 2>/dev/null || true; }
else
    cli_click()  { "$CLICK" "c:$1,$2" 2>/dev/null || true; sleep 0.8; }
    cli_type()   { "$CLICK" "t:$1" 2>/dev/null || true; sleep 0.8; }
fi
capture() {
    screencapture -x -R "0,0,${WIN_W},${WIN_H}" "$OUT_DIR/$1.png" 2>/dev/null || true
    if [ -s "$OUT_DIR/$1.png" ]; then log "captured $1"; else warn "capture failed $1"; fi
}

# ── Slow HTTP server ──────────────────────────────────────────────────────
SERVER_PID=""
if command -v python3 >/dev/null 2>&1; then
    python3 "$PROJECT_DIR/scripts/screenshot/slow-http-server.py" "$SLOW_PORT" 2.0 >/tmp/nd-ios-server.log 2>&1 &
    SERVER_PID=$!
    log "slow-http-server started (pid $SERVER_PID)"
fi
cleanup() { [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null || true; }
trap cleanup EXIT

# ── Launch + resize ───────────────────────────────────────────────────────
# Clean QSettings, then launch with ND_MOBILE=1 so the mobile layout renders.
rm -f ~/Library/Preferences/com.robinhoo1973.NetDiagnostics.plist 2>/dev/null || true
export ND_MOBILE=1
export ND_MAX_TESTS=4
export ND_CAPTURE_TARGET="$TARGET"

log "starting app: $APP"
"$APP" &
APP_PID=$!
sleep 4

# Resize via AppleScript
osascript -e "tell app \"NetDiagnostics\" to set bounds of front window to {0,0,${WIN_W},${WIN_H}}" 2>/dev/null || warn "window resize failed"
sleep 2

# ══════════════════════════════════════════════════════════════════════════
# PASS A — 1..6 (report overlay not dismissible; relaunch for B)
# ══════════════════════════════════════════════════════════════════════════
capture 1-idle

# Nav to diagnostics + type + run
if [ -n "$CLICK" ]; then cli_click "$NAV_DIAG_X" "$NAV_Y"; else osa_click "$NAV_DIAG_X" "$NAV_Y"; fi
sleep 1
if [ -n "$CLICK" ]; then cli_click "$((WIN_W/2))" "$INPUT_Y"; else osa_click "$((WIN_W/2))" "$INPUT_Y"; fi
sleep 1
if [ -n "$CLICK" ]; then cli_type "$TARGET"; else osa_type "$TARGET"; fi
sleep 1
if [ -n "$CLICK" ]; then cli_click "$RUN_X" "$RUN_Y"; else osa_click "$RUN_X" "$RUN_Y"; fi
sleep 2
capture 2-running

# Wait for completion
sleep 30
capture 3-complete

# Detail overlay — click result row + close
if [ -n "$CLICK" ]; then cli_click "$RESULT_ROW_X" "$RESULT_ROW_Y"; else osa_click "$RESULT_ROW_X" "$RESULT_ROW_Y"; fi
sleep 2
capture 4-detail
if [ -n "$CLICK" ]; then cli_click "$DETAIL_CLOSE_X" "$DETAIL_CLOSE_Y"; else osa_click "$DETAIL_CLOSE_X" "$DETAIL_CLOSE_Y"; fi
sleep 1

# Dashboard
if [ -n "$CLICK" ]; then cli_click "$NAV_DASH_X" "$NAV_Y"; else osa_click "$NAV_DASH_X" "$NAV_Y"; fi
sleep 1
capture 5-dashboard

# Report preview
if [ -n "$CLICK" ]; then cli_click "$REPORT_BTN_X" "$REPORT_BTN_Y"; else osa_click "$REPORT_BTN_X" "$REPORT_BTN_Y"; fi
sleep 3
capture 6-report

# Relaunch for B
kill "$APP_PID" 2>/dev/null || true; sleep 2
export ND_MOBILE=1 ND_MAX_TESTS=4
"$APP" &
APP_PID=$!
sleep 4
osascript -e "tell app \"NetDiagnostics\" to set bounds of front window to {0,0,${WIN_W},${WIN_H}}" 2>/dev/null || true
sleep 2

if [ -n "$CLICK" ]; then cli_click "$NAV_CFG_X" "$NAV_Y"; else osa_click "$NAV_CFG_X" "$NAV_Y"; fi
sleep 1
capture 7-config

if [ -n "$CLICK" ]; then cli_click "$NAV_SET_X" "$NAV_Y"; else osa_click "$NAV_SET_X" "$NAV_Y"; fi
sleep 1
capture 8-settings

kill "$APP_PID" 2>/dev/null || true
log "done — screenshots in $OUT_DIR"
exit 0
