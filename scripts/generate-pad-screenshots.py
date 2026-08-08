#!/usr/bin/env python3
"""
Generate pad screenshots for all supported iPad sizes from the canonical
source (resources/doc/screenshot/pad/unknown).

Scales proportionally (cover), then center-crops to the exact target
resolution — the same strategy as generate-phone-screenshots.py.  Requires
Pillow.

Canonical source:  resources/doc/screenshot/pad/unknown/*.png (2048x1536 landscape)
Output:            resources/doc/screenshot/pad/<size>/*.png

Usage:  python3 scripts/generate-pad-screenshots.py [source-dir]
"""

import sys
from pathlib import Path
from PIL import Image

# Target resolutions (Apple App Store Connect / modern iPads, landscape)
# 13.0" — iPad Pro 13" (M4):        2752 x 2064
# 12.9" — iPad Pro 12.9" (M2/3rd):  2732 x 2048
# 11.0" — iPad Pro 11" (M2):        2388 x 1668
# 10.9" — iPad Air 11" / iPad 10.9": 2360 x 1640
# 10.5" — iPad Pro 10.5" / Air 3:   2224 x 1668
# 8.3"  — iPad mini 6:              2266 x 1488
TARGETS = {
    "13.0": (2752, 2064),
    "12.9": (2732, 2048),
    "11.0": (2388, 1668),
    "10.9": (2360, 1640),
    "10.5": (2224, 1668),
    "8.3":  (2266, 1488),
}

PROJECT = Path(__file__).resolve().parent.parent
SRC_DIR = Path(sys.argv[1]) if len(sys.argv) > 1 else (
    PROJECT / "resources" / "doc" / "screenshot" / "pad" / "unknown")
OUT_BASE = SRC_DIR.parent  # resources/doc/screenshot/pad/

if not SRC_DIR.is_dir():
    print(f"ERROR: Source directory not found: {SRC_DIR}")
    sys.exit(1)

pngs = sorted(SRC_DIR.glob("*.png"))
if not pngs:
    print(f"ERROR: No .png files found in {SRC_DIR}")
    sys.exit(1)

print(f"Found {len(pngs)} source images in {SRC_DIR.name}/\n")

for src_path in pngs:
    img = Image.open(src_path)
    src_w, src_h = img.size
    filename = src_path.name

    for label, (tw, th) in TARGETS.items():
        dst_dir = OUT_BASE / label
        dst_dir.mkdir(parents=True, exist_ok=True)
        dst_path = dst_dir / filename

        if dst_path.exists():
            print(f"  SKIP  {label}\" {filename}  (exists)")
            continue

        # Scale proportionally to cover the target (larger dimension wins)
        scale = max(tw / src_w, th / src_h)
        new_w = int(src_w * scale)
        new_h = int(src_h * scale)

        scaled = img.resize((new_w, new_h), Image.LANCZOS)

        # Center-crop to exact target size
        left = (new_w - tw) // 2
        top  = (new_h - th) // 2
        cropped = scaled.crop((left, top, left + tw, top + th))

        cropped.save(dst_path, "PNG")
        print(f"  OK    {label}\" {filename}  {src_w}x{src_h} -> {tw}x{th}  (scale {scale:.3f})")

print("\nDone.")
for label in sorted(TARGETS.keys()):
    d = OUT_BASE / label
    count = len(list(d.glob("*.png"))) if d.is_dir() else 0
    print(f"  {label}\": {count} files")
