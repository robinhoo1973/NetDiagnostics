#!/usr/bin/env bash
# =============================================================================
# generate-screenshots-sips.sh — macOS native (sips, built-in)
# =============================================================================
# Scale 6.1" screenshots to 6.3"/6.5"/6.9" using macOS built-in sips.
# No dependencies required.
#
# Usage:
#   chmod +x scripts/generate-screenshots-sips.sh
#   ./scripts/generate-screenshots-sips.sh
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$PROJECT_DIR/doc/screenshot/6.1"

if [ ! -d "$SRC_DIR" ]; then
    echo "ERROR: Source directory not found: $SRC_DIR"
    echo "       Expected: $SRC_DIR"
    exit 1
fi

# Target resolutions (Apple App Store Connect 2026)
#  6.9" — iPhone 16 Pro Max:  1320 x 2868
#  6.5" — iPhone 11 Pro Max:  1242 x 2688
#  6.3" — iPhone 16 Pro:      1206 x 2622
declare -A TARGET_W
declare -A TARGET_H
TARGET_W["6.9"]=1320; TARGET_H["6.9"]=2868
TARGET_W["6.5"]=1242; TARGET_H["6.5"]=2688
TARGET_W["6.3"]=1206; TARGET_H["6.3"]=2622

declare -A TARGET_SIZES
TARGET_SIZES=(["6.9"]=1 ["6.5"]=1 ["6.3"]=1)

for src in "$SRC_DIR"/*.png; do
    [ -f "$src" ] || continue
    filename="$(basename "$src")"

    # Get source dimensions
    src_w=$(sips -g pixelWidth  "$src" 2>/dev/null | awk '/pixelWidth/  {print $2}')
    src_h=$(sips -g pixelHeight "$src" 2>/dev/null | awk '/pixelHeight/ {print $2}')
    src_ratio=$(echo "scale=6; $src_w / $src_h" | bc -l 2>/dev/null || echo "0.4618")

    for size in "${!TARGET_SIZES[@]}"; do
        dst_dir="$PROJECT_DIR/doc/screenshot/$size"
        mkdir -p "$dst_dir"
        dst="$dst_dir/$filename"

        if [ -f "$dst" ]; then
            echo "SKIP (exists): $dst"
            continue
        fi

        tw=${TARGET_W[$size]}
        th=${TARGET_H[$size]}
        tgt_ratio=$(echo "scale=6; $tw / $th" | bc -l 2>/dev/null || echo "0.4603")

        # Work in a temp directory to avoid polluting source
        tmpdir=$(mktemp -d)
        cp "$src" "$tmpdir/input.png"

        # Step 1: resize proportionally to fill the target (cover)
        # Width-first: scale to target width, check if height is enough
        sips -z 99999 "$tw" "$tmpdir/input.png" --out "$tmpdir/scaled.png" 2>/dev/null
        scaled_h=$(sips -g pixelHeight "$tmpdir/scaled.png" 2>/dev/null | awk '/pixelHeight/ {print $2}')

        if [ "$scaled_h" -lt "$th" ]; then
            # Height-first: scale to target height
            sips -z "$th" 99999 "$tmpdir/input.png" --out "$tmpdir/scaled.png" 2>/dev/null
        fi

        # Step 2: crop to exact target dimensions (center-crop)
        sw=$(sips -g pixelWidth  "$tmpdir/scaled.png" 2>/dev/null | awk '/pixelWidth/  {print $2}')
        sh=$(sips -g pixelHeight "$tmpdir/scaled.png" 2>/dev/null | awk '/pixelHeight/ {print $2}')

        cx=$(( (sw - tw) / 2 ))
        cy=$(( (sh - th) / 2 ))

        sips --cropToHeightWidth "$th" "$tw" \
             --cropOffset "$cy" "$cx" \
             "$tmpdir/scaled.png" --out "$dst" 2>/dev/null

        rm -rf "$tmpdir"
        echo "OK: $filename -> $size (${tw}x${th})"
    done
done

echo ""
echo "Done. Generated screenshots:"
for size in 6.9 6.5 6.3; do
    count=$(ls "$PROJECT_DIR/doc/screenshot/$size"/*.png 2>/dev/null | wc -l)
    echo "  $size\": $count files"
done
