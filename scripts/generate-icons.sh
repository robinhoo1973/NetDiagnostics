#!/usr/bin/env bash
# =============================================================================
# Generate platform-specific icons from the master SVG
# =============================================================================
# Requirements:
#   - ImageMagick (`magick` or `convert`) — for .ico generation
#   - librsvg (`rsvg-convert`) — for SVG → PNG conversion
#   - macOS: `iconutil` (built-in) — for .icns generation
#
# Usage:
#   bash scripts/generate-icons.sh
# =============================================================================

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ICONS_DIR="$PROJECT_DIR/resources/icons"
SVG_MASTER="$ICONS_DIR/app-icon.svg"
OUT_ICO="$ICONS_DIR/netanalysis.ico"
OUT_PNG="$ICONS_DIR/netanalysis.png"

# ── Check prerequisites ──────────────────────────────────────────────────────
check_cmd() {
    if ! command -v "$1" &>/dev/null; then
        echo "ERROR: $1 not found. Please install it first."
        echo "  Ubuntu/Debian: sudo apt install $2"
        echo "  macOS:         brew install $2"
        echo "  Windows:       choco install $2"
        exit 1
    fi
}

echo "=== NetAnalysis Icon Generator ==="
echo ""

# ── 1. Generate high-res PNG from SVG ─────────────────────────────────────────
echo "[1/3] Generating PNG from SVG..."
# Use rsvg-convert for best SVG rendering, fallback to ImageMagick
if command -v rsvg-convert &>/dev/null; then
    rsvg-convert -w 1024 -h 1024 "$SVG_MASTER" -o "$OUT_PNG"
elif command -v magick &>/dev/null; then
    magick -background none -density 300 "$SVG_MASTER" -resize 1024x1024 "$OUT_PNG"
elif command -v convert &>/dev/null; then
    convert -background none -density 300 "$SVG_MASTER" -resize 1024x1024 "$OUT_PNG"
else
    echo "WARNING: No SVG→PNG converter found. Skipping PNG generation."
fi
echo "  → $OUT_PNG"

# ── 2. Generate Windows .ico (multi-resolution) ───────────────────────────────
echo "[2/3] Generating Windows .ico..."
if command -v magick &>/dev/null; then
    magick "$OUT_PNG" -define icon:auto-resize=256,128,96,64,48,40,32,24,20,16 "$OUT_ICO"
elif command -v convert &>/dev/null; then
    convert "$OUT_PNG" -define icon:auto-resize=256,128,96,64,48,40,32,24,20,16 "$OUT_ICO"
else
    echo "WARNING: No ImageMagick found. Skipping .ico generation."
fi
echo "  → $OUT_ICO"

# ── 3. Generate macOS .icns ───────────────────────────────────────────────────
echo "[3/3] Generating macOS .icns..."
if [[ "$(uname)" == "Darwin" ]]; then
    ICONSET="$ICONS_DIR/netanalysis.iconset"
    mkdir -p "$ICONSET"

    # Generate all required sizes
    sizes=(16 32 64 128 256 512)
    for s in "${sizes[@]}"; do
        s2=$((s * 2))
        if command -v rsvg-convert &>/dev/null; then
            rsvg-convert -w $s -h $s "$SVG_MASTER" -o "$ICONSET/icon_${s}x${s}.png"
            rsvg-convert -w $s2 -h $s2 "$SVG_MASTER" -o "$ICONSET/icon_${s}x${s}@2x.png"
        elif command -v magick &>/dev/null; then
            magick -background none "$SVG_MASTER" -resize "${s}x${s}" "$ICONSET/icon_${s}x${s}.png"
            magick -background none "$SVG_MASTER" -resize "${s2}x${s2}" "$ICONSET/icon_${s}x${s}@2x.png"
        fi
    done

    # Convert iconset to .icns
    iconutil -c icns "$ICONSET" -o "$ICONS_DIR/netanalysis.icns"
    rm -rf "$ICONSET"
    echo "  → $ICONS_DIR/netanalysis.icns"
else
    echo "  ⚠ Not on macOS — .icns generation skipped."
    echo "  On macOS, run: iconutil -c icns netanalysis.iconset -o netanalysis.icns"
fi

echo ""
echo "=== Done! ==="
echo ""
echo "Generated files:"
echo "  Windows:  resources/icons/netanalysis.ico"
echo "  Linux:    resources/icons/netanalysis.png"
echo "  macOS:    resources/icons/netanalysis.icns (macOS only)"
echo "  All OS:   Use resources/netanalysis.desktop (Linux)"
echo "            Use resources/netanalysis.rc (Windows)"
