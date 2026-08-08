#!/usr/bin/env python3
"""
Generate phone screenshots for all supported iPhone sizes from the 6.1"
canonical source.

Scales proportionally (cover), then center-crops to the exact target
resolution — the same strategy as generate-screenshots.py.  Requires
Pillow.

Canonical source:  resources/doc/screenshot/phone/6.1/*.png
Output:            resources/doc/screenshot/phone/<size>/*.png

Usage:  python3 scripts/generate-phone-screenshots.py [source-dir]
"""

import sys
from pathlib import Path
from PIL import Image

# Target resolutions (Apple App Store Connect / modern iPhones)
#  6.9" — iPhone 16 Pro Max:      1320 x 2868
#  6.7" — iPhone 14/15 Pro Max:   1290 x 2796
#  6.5" — iPhone 11 Pro Max / XS: 1242 x 2688
#  6.3" — iPhone 16 Pro:          1206 x 2622
#  6.1" — iPhone 14/15/16 Pro:    1170 x 2532
TARGETS = {
    "6.9": (1320, 2868),
    "6.7": (1290, 2796),
    "6.5": (1242, 2688),
    "6.3": (1206, 2622),
    "6.1": (1170, 2532),
}

PROJECT = Path(__file__).resolve().parent.parent
SRC_DIR = Path(sys.argv[1]) if len(sys.argv) > 1 else (
    PROJECT / "resources" / "doc" / "screenshot" / "phone" / "6.1")
OUT_BASE = SRC_DIR.parent  # resources/doc/screenshot/phone/

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
