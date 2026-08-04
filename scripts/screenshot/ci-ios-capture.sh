#!/usr/bin/env bash
# =============================================================================
# ci-ios-capture.sh — Boot simulator, install app, drive it, capture screenshots
# =============================================================================
# Loops over a device list (name|output-subdir), boots each available
# simulator, installs the built .app, launches with SIMCTL_CHILD_* env
# inherited from the caller, runs drive-ios.sh, then shuts the device down.
#
# Device availability is checked against the runner's installed runtimes;
# missing devices are skipped with a warning (never a hard failure).  The job
# only fails if EVERY device failed to produce its PNG set.
#
# Usage:
#   ND_MAX_TESTS=4 ND_CAPTURE_TARGET=http://localhost:8899 \
#     bash scripts/screenshot/ci-ios-capture.sh build/net_diagnostics.app com.netdiagnostic.app
# =============================================================================
set -euo pipefail

APP="${1:?usage: ci-ios-capture.sh <path-to.app> <bundle-id>}"
BUNDLE_ID="${2:?usage: ci-ios-capture.sh <path-to.app> <bundle-id>}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

[ -d "$APP" ] || { echo "ERROR: app bundle not found: $APP" >&2; exit 1; }

# ── Device roster: "Simulator Name|output-subdir under resources/doc/screenshot" ─
# iPad 13" (M4) may be absent on older Xcode — fallback list is checked first.
TABLET_13="iPad Pro 13-inch (M4)"
if ! xcrun simctl list devices available | grep -qF "iPad Pro 13-inch (M4)"; then
    TABLET_13="iPad Pro 12.9-inch (6th generation)"
    echo "INFO: iPad Pro 13-inch (M4) not available; using '$TABLET_13'"
fi
DEVICES=(
    "iPhone SE (3rd generation)|ios/phone/6.1"
    "iPhone 16 Pro|ios/phone/6.3"
    "iPhone 16|ios/phone/6.5"
    "iPhone 16 Pro Max|ios/phone/6.9"
    "$TABLET_13|ios/tablet/13"
)

# ── Pass SIMCTL_CHILD_* (simctl env passthrough) from caller env ────────────
for v in ND_MAX_TESTS ND_CAPTURE_TARGET ND_RESULT_ROW_Y ND_REPORT_BTN_Y ND_DEBUG ND_INPUT_Y ND_RUN_Y; do
    val="${!v:-}"
    [ -n "$val" ] && export "SIMCTL_CHILD_${v}=$val"
done

FAILED=0
CAPTURED=0
for entry in "${DEVICES[@]}"; do
    name="${entry%%|*}"
    sub="${entry#*|}"
    out_dir="$PROJECT_DIR/resources/doc/screenshot/$sub"
    echo "═══════════════════════════════════════════════════════════════"
    echo "▶ device: $name  →  $sub"
    echo "═══════════════════════════════════════════════════════════════"

    if ! xcrun simctl list devices available | grep -qF "$name"; then
        echo "WARN: '$name' not available on this runner — skipping"
        continue
    fi

    udid=$(xcrun simctl list devices available | grep -F "$name" | head -1 | grep -oE '[0-9A-F-]{36}' | head -1)
    echo "UDID: $udid"

    if xcrun simctl list devices | grep -F "$name" | grep -q Booted; then
        xcrun simctl shutdown "$udid" 2>/dev/null || true
    fi

    # 5WHY: `xcrun simctl boot` returns immediately but the device is not
    # ready; bootstatus -b blocks until boot completes. Without it, install
    # races boot and fails intermittently.
    xcrun simctl boot "$udid" || { echo "WARN: boot failed for $name — skipping"; continue; }
    xcrun simctl bootstatus "$udid" -b || { echo "WARN: bootstatus failed for $name — skipping"; xcrun simctl shutdown "$udid" 2>/dev/null || true; continue; }

    mkdir -p "$out_dir"
    if ND_APP="$APP" ND_BUNDLE_ID="$BUNDLE_ID" \
       bash "$SCRIPT_DIR/drive-ios.sh" "$out_dir"; then
        count=$(find "$out_dir" -name '*.png' | wc -l | tr -d ' ')
        if [ "$count" -ge 6 ]; then
            echo "OK: '$name' → $count screenshots"
            CAPTURED=$((CAPTURED + 1))
        else
            echo "WARN: '$name' produced only $count PNGs (<6 expected)"
            FAILED=$((FAILED + 1))
        fi
    else
        echo "WARN: drive-ios.sh failed for '$name'"
        FAILED=$((FAILED + 1))
    fi

    xcrun simctl shutdown "$udid" 2>/dev/null || true
done

echo "─────────────────────────────────────────────────────────────────"
echo "captured=$CAPTURED failed=$FAILED"
if [ "$CAPTURED" -eq 0 ] && [ "$FAILED" -gt 0 ]; then
    echo "ERROR: no device produced screenshots" >&2
    exit 1
fi
if [ "$FAILED" -gt 0 ]; then
    echo "WARN: $FAILED device(s) had issues (partial success)"
fi
exit 0
