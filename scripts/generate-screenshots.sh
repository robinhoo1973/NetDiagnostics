#!/usr/bin/env bash
# =============================================================================
# generate-screenshots.sh — Scale 6.1" screenshots to 6.3"/6.5"/6.9"
# =============================================================================
# Requires: ImageMagick (convert)
# Usage:    ./scripts/generate-screenshots.sh
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$PROJECT_DIR/doc/screenshot/6.1"

if [ ! -d "$SRC_DIR" ]; then
    echo "ERROR: Source directory not found: $SRC_DIR"
    exit 1
fi

# ── Target resolutions (Apple App Store Connect 2026) ──────────────────
# 6.9"  — iPhone 16 Pro Max
# 6.5"  — iPhone 11 Pro Max
# 6.3"  — iPhone 16 Pro
declare -A TARGETS=(
    ["6.9"]="1320x2868"
    ["6.5"]="1242x2688"
    ["6.3"]="1206x2622"
)

# ── Process each source image ──────────────────────────────────────────
for src in "$SRC_DIR"/*.png; do
    [ -f "$src" ] || continue
    filename="$(basename "$src")"

    for size in "${!TARGETS[@]}"; do
        dst_dir="$PROJECT_DIR/doc/screenshot/$size"
        mkdir -p "$dst_dir"
        dst="$dst_dir/$filename"

        if [ -f "$dst" ]; then
            echo "SKIP (exists): $dst"
            continue
        fi

        resolution="${TARGETS[$size]}"

        # Scale proportionally to fill target area, then crop to exact size.
        # ^  = resize to fill (cover), preserving aspect ratio
        # -gravity center = center the crop
        # -extent = crop/extend to exact dimensions
        convert "$src" \
            -resize "${resolution}^" \
            -gravity center \
            -extent "$resolution" \
            "$dst"

        echo "OK: $filename -> $size ($resolution)"
    done
done

echo ""
echo "Done. Generated screenshots:"
for size in "${!TARGETS[@]}"; do
    count=$(ls "$PROJECT_DIR/doc/screenshot/$size"/*.png 2>/dev/null | wc -l)
    echo "  $size\": $count files"
done
