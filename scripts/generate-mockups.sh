#!/usr/bin/env bash
# =============================================================================
# generate-mockups.sh — Capture simulator screenshots for all device profiles
#
# Usage: ./scripts/generate-mockups.sh [device_id]
#
# Output: mockups/{os}/{device_id}/
#   dashboard.png   diagnostics.png   config.png   report.png   settings.png
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-mockups"
EXE="${BUILD_DIR}/net_diagnostics_sim"
OUTPUT_BASE="${PROJECT_DIR}/mockups"

# ── Device profiles (must match SimulatorScreen.qml devices array) ──────
DEVICES=(
  # Desktop
  "win-x64|windows"
  "win-arm64|windows"
  "lx-x64|linux"
  "lx-arm64|linux"
  "mac-x64|macos"
  "mac-arm64|macos"
  # iOS Phones
  "ios-iphone-se|ios"
  "ios-iphone15|ios"
  "ios-iphone15pm|ios"
  "ios-iphone16|ios"
  "ios-iphone16pm|ios"
  # iOS Tablets
  "ios-ipad-mini|ios"
  "ios-ipadpro11|ios"
  "ios-ipadpro13|ios"
  # Android Phones
  "android-pixel8|android"
  "android-pixel9|android"
  "android-s24|android"
  "android-s24u|android"
  "android-oneplus|android"
)

echo "=== NetDiagnostics Mockup Generator ==="

# ── Filter to single device if specified ────────────────────────────────
if [ $# -ge 1 ]; then
  FILTERED=()
  for d in "${DEVICES[@]}"; do
    [[ "$d" == "$1|"* ]] && FILTERED+=("$d")
  done
  if [ ${#FILTERED[@]} -eq 0 ]; then
    echo "ERROR: Unknown device '$1'. Valid IDs:"
    for d in "${DEVICES[@]}"; do echo "  ${d%%|*}"; done
    exit 1
  fi
  DEVICES=("${FILTERED[@]}")
fi

# ── Build simulator if needed ───────────────────────────────────────────
if [ ! -f "$EXE" ] && [ ! -f "${EXE}.exe" ]; then
  echo ">>> Building simulator..."
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SIMULATOR=ON \
    -DBUILD_TESTS=OFF -B "$BUILD_DIR" -S "$PROJECT_DIR"
  ninja -C "$BUILD_DIR" net_diagnostics_sim
fi

# On Windows, the exe has .exe extension
[ -f "${EXE}.exe" ] && EXE="${EXE}.exe"
[ ! -f "$EXE" ] && { echo "ERROR: $EXE not found"; exit 1; }

echo ">>> Simulator: $EXE"
echo ">>> Devices to capture: ${#DEVICES[@]}"
echo ""

# ── Screens to capture per device ────────────────────────────────────────
SCREENS=("dashboard" "diagnostics" "config" "report" "settings")

for entry in "${DEVICES[@]}"; do
  DEV_ID="${entry%%|*}"
  DEV_OS="${entry##*|}"
  DEV_DIR="${OUTPUT_BASE}/${DEV_OS}/${DEV_ID}"
  mkdir -p "$DEV_DIR"

  for screen in "${SCREENS[@]}"; do
    OUT_PATH="${DEV_DIR}/${screen}.png"
    printf "  %-24s → %-12s " "$DEV_ID" "$screen"
    ND_MOCKUP=1 \
    ND_MOCKUP_DEVICE="$DEV_ID" \
    ND_MOCKUP_OUTPUT="$OUT_PATH" \
    QT_QPA_PLATFORM=offscreen \
      "$EXE" 2>/dev/null
    if [ -f "$OUT_PATH" ]; then
      SIZE=$(wc -c < "$OUT_PATH")
      echo "OK (${SIZE} bytes)"
    else
      echo "FAIL"
    fi
  done
  echo ""
done

echo "=== Done. Mockups in ${OUTPUT_BASE} ==="
