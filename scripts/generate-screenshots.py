#!/usr/bin/env python3
"""
Generate App Store screenshots for 6.3", 6.5", 6.9" from 6.1" source images.

Scales proportionally (cover), then center-crops to exact target resolution.
Requires: pip install Pillow

Usage:  python3 scripts/generate-screenshots.py
"""

import os
import sys
from pathlib import Path
from PIL import Image

# Target resolutions (Apple App Store Connect)
TARGETS = {
    "6.9": (1320, 2868),   # iPhone 16 Pro Max
    "6.5": (1242, 2688),   # iPhone 11 Pro Max
    "6.3": (1206, 2622),   # iPhone 16 Pro
}

PROJECT = Path(__file__).resolve().parent.parent
SRC_DIR = PROJECT / "doc" / "screenshot" / "6.1"

if not SRC_DIR.is_dir():
    print(f"ERROR: Source directory not found: {SRC_DIR}")
    sys.exit(1)

pngs = sorted(SRC_DIR.glob("*.png"))
if not pngs:
    print(f"ERROR: No .png files found in {SRC_DIR}")
    sys.exit(1)

print(f"Found {len(pngs)} source images in 6.1/\n")

for src_path in pngs:
    img = Image.open(src_path)
    src_w, src_h = img.size
    filename = src_path.name

    for label, (tw, th) in TARGETS.items():
        dst_dir = PROJECT / "doc" / "screenshot" / label
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

print(f"\nDone.")
for label in ["6.3", "6.5", "6.9"]:
    d = PROJECT / "doc" / "screenshot" / label
    count = len(list(d.glob("*.png"))) if d.is_dir() else 0
    print(f"  {label}\": {count} files")
